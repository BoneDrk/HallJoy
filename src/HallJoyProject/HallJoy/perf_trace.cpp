#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include "perf_trace.h"

#include <algorithm>
#include <atomic>
#include <iterator>
#include <cstdio>
#include <cstring>
#include <cwchar>

#if defined(_MSC_VER)
// Runs before every other static initializer of this image ("compiler"
// segment): separates loader time from static initialization in the timeline.
#pragma warning(push)
#pragma warning(disable : 4073)
#pragma init_seg(lib)
#pragma warning(pop)
#endif
namespace
{
std::int64_t QpcNow() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
const std::int64_t g_staticInitQpc = QpcNow();
}

namespace halljoy::perf
{
namespace
{

struct Record
{
    const char* name;
    const char* detail;
    std::uint64_t index;
    std::int64_t start;
    std::int64_t end;
    DWORD thread;
};

constexpr std::size_t kCapacity = 131072; // untouched pages are never committed
Record* g_records = nullptr;      // allocated only when perf mode is enabled
std::atomic<std::size_t> g_count{ 0 };
std::atomic<bool> g_enabled{ false };
wchar_t g_path[MAX_PATH]{};
std::int64_t g_frequency = 1;
std::int64_t g_originQpc = 0;      // QPC at process creation (estimated)

// CPU sampler: every kSampleMs the cumulative CPU time of each thread.
struct CpuSample
{
    std::int64_t qpc;
    std::uint64_t cpu100ns;
    std::uintptr_t startAddress;
    DWORD thread;
};
constexpr DWORD kSampleMs = 500;
constexpr std::size_t kSampleCapacity = 65536;
CpuSample* g_samples = nullptr;   // allocated only when perf mode is enabled
std::atomic<std::size_t> g_sampleCount{ 0 };
HANDLE g_samplerStop = nullptr;
HANDLE g_sampler = nullptr;
using NtQueryInformationThreadFn = LONG(WINAPI*)(HANDLE, int, void*, ULONG, ULONG*);

struct TrackedThread
{
    DWORD id;
    HANDLE handle;
    std::uintptr_t startAddress;
};

// A system-wide Toolhelp snapshot is expensive; refresh the thread list only
// every tenth sample and read times from the open handles in between.
void SampleThreads(bool refresh) noexcept
{
    static const auto query = reinterpret_cast<NtQueryInformationThreadFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    static TrackedThread tracked[512];
    static std::size_t trackedCount = 0;
    if (refresh || !trackedCount) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
                if (entry.th32OwnerProcessID != GetCurrentProcessId()) continue;
                bool known = false;
                for (std::size_t i = 0; i < trackedCount && !known; ++i) known = tracked[i].id == entry.th32ThreadID;
                if (known || trackedCount >= std::size(tracked)) continue;
                HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                if (!thread) continue;
                void* startAddress = nullptr;
                if (query) query(thread, 9, &startAddress, sizeof(startAddress), nullptr);
                tracked[trackedCount++] = TrackedThread{ entry.th32ThreadID, thread,
                    reinterpret_cast<std::uintptr_t>(startAddress) };
            }
            CloseHandle(snapshot);
        }
    }
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    for (std::size_t i = 0; i < trackedCount; ++i) {
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetThreadTimes(tracked[i].handle, &created, &exited, &kernel, &user)) continue;
        const std::size_t slot = g_sampleCount.fetch_add(1, std::memory_order_relaxed);
        if (slot >= kSampleCapacity) return;
        ULARGE_INTEGER k{}, u{};
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        g_samples[slot] = CpuSample{ now.QuadPart, k.QuadPart + u.QuadPart, tracked[i].startAddress, tracked[i].id };
    }
}

DWORD WINAPI SamplerThread(void*) noexcept
{
    unsigned tick = 0;
    do SampleThreads(tick++ % 10 == 0);
    while (WaitForSingleObject(g_samplerStop, kSampleMs) == WAIT_TIMEOUT);
    SampleThreads(true);
    return 0;
}

void Push(const char* name, const char* detail, std::uint64_t index, std::int64_t start, std::int64_t end) noexcept
{
    const std::size_t slot = g_count.fetch_add(1, std::memory_order_relaxed);
    if (slot >= kCapacity) return;
    g_records[slot] = Record{ name, detail, index, start, end, GetCurrentThreadId() };
}

double Ms(std::int64_t qpcTicks) noexcept
{
    return static_cast<double>(qpcTicks) * 1000.0 / static_cast<double>(g_frequency);
}

double FileTimeMs(const FILETIME& value) noexcept
{
    ULARGE_INTEGER v{};
    v.LowPart = value.dwLowDateTime;
    v.HighPart = value.dwHighDateTime;
    return static_cast<double>(v.QuadPart) / 10000.0;
}

void WriteThreads(FILE* file) noexcept
{
    const auto query = reinterpret_cast<NtQueryInformationThreadFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != GetCurrentProcessId()) continue;
        HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        if (!thread) continue;
        FILETIME created{}, exited{}, kernel{}, user{};
        GetThreadTimes(thread, &created, &exited, &kernel, &user);
        void* startAddress = nullptr;
        if (query) query(thread, 9 /* ThreadQuerySetWin32StartAddress */, &startAddress, sizeof(startAddress), nullptr);
        const auto address = reinterpret_cast<std::uintptr_t>(startAddress);
        HMODULE module = nullptr;
        wchar_t moduleName[MAX_PATH] = L"?";
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(startAddress), &module) && module) {
            wchar_t full[MAX_PATH]{};
            if (GetModuleFileNameW(module, full, MAX_PATH)) {
                const wchar_t* slash = wcsrchr(full, L'\\');
                wcscpy_s(moduleName, slash ? slash + 1 : full);
            }
        }
        const std::uintptr_t moduleBase = reinterpret_cast<std::uintptr_t>(module);
        fwprintf(file, L"thread tid=%lu user_ms=%.1f kernel_ms=%.1f module=%ls rva=0x%llx exe_rva=0x%llx\n",
            entry.th32ThreadID, FileTimeMs(user), FileTimeMs(kernel), moduleName,
            static_cast<unsigned long long>(module ? address - moduleBase : address),
            static_cast<unsigned long long>(module == reinterpret_cast<HMODULE>(base) ? address - base : 0));
        CloseHandle(thread);
    }
    CloseHandle(snapshot);
}

// UI-thread stack sampler (perf mode only): every ~1 ms the UI thread is
// suspended, its stack unwound into a preallocated buffer (no allocation or
// locks while it is suspended), and resumed. Flush writes module-relative
// return addresses; tools resolve them with the linker map.
constexpr std::size_t kStackDepth = 16;
struct StackSample
{
    std::int64_t qpc;
    std::uintptr_t frames[kStackDepth];
};
constexpr std::size_t kStackCapacity = 200000;
StackSample* g_stacks = nullptr;
std::atomic<std::size_t> g_stackCount{ 0 };
HANDLE g_uiThread = nullptr;
HANDLE g_stackSampler = nullptr;

void SampleUiStack() noexcept
{
    if (SuspendThread(g_uiThread) == static_cast<DWORD>(-1)) return;
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    const std::size_t slot = g_stackCount.load(std::memory_order_relaxed);
    if (slot < kStackCapacity && GetThreadContext(g_uiThread, &context)) {
        StackSample& sample = g_stacks[slot];
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        sample.qpc = now.QuadPart;
        std::size_t depth = 0;
        for (; depth < kStackDepth && context.Rip; ++depth) {
            sample.frames[depth] = static_cast<std::uintptr_t>(context.Rip);
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
            if (!function) {
                // Leaf function: the return address is on top of the stack.
                context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
                context.Rsp += 8;
            } else {
                void* handlerData = nullptr;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
                    &handlerData, &establisher, nullptr);
            }
        }
        for (; depth < kStackDepth; ++depth) sample.frames[depth] = 0;
        g_stackCount.store(slot + 1, std::memory_order_relaxed);
    }
    ResumeThread(g_uiThread);
}

DWORD WINAPI StackSamplerThread(void*) noexcept
{
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /* high resolution */, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    while (WaitForSingleObject(g_samplerStop, 0) == WAIT_TIMEOUT) {
        SampleUiStack();
        LARGE_INTEGER due{};
        due.QuadPart = -10000; // 1 ms
        if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, 50);
        else Sleep(1);
    }
    if (timer) CloseHandle(timer);
    return 0;
}

} // namespace

void ConfigureFromCommandLine(const wchar_t* commandLine) noexcept
{
    if (!commandLine) return;
    const wchar_t* key = wcsstr(commandLine, L"--halljoy-perf-log=");
    if (!key) return;
    key += wcslen(L"--halljoy-perf-log=");
    const bool quoted = *key == L'"';
    if (quoted) ++key;
    std::size_t length = 0;
    while (key[length] && length + 1 < MAX_PATH &&
        (quoted ? key[length] != L'"' : key[length] != L' ' && key[length] != L'\t'))
        ++length;
    if (!length) return;
    wmemcpy(g_path, key, length);
    g_path[length] = 0;

    LARGE_INTEGER frequency{}, now{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    g_frequency = frequency.QuadPart ? frequency.QuadPart : 1;
    // Anchor the timeline at process creation: QPC now minus the elapsed
    // wall time since CreateProcess.
    FILETIME created{}, exited{}, kernel{}, user{}, current{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimePreciseAsFileTime(&current);
    const double sinceCreateMs = FileTimeMs(current) - FileTimeMs(created);
    g_originQpc = now.QuadPart - static_cast<std::int64_t>(sinceCreateMs * static_cast<double>(g_frequency) / 1000.0);
    g_records = static_cast<Record*>(VirtualAlloc(nullptr, sizeof(Record) * kCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    g_samples = static_cast<CpuSample*>(VirtualAlloc(nullptr, sizeof(CpuSample) * kSampleCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!g_records || !g_samples) return;
    g_enabled.store(true, std::memory_order_release);
    Push("crt.static_init", nullptr, 0, g_staticInitQpc, g_staticInitQpc);
    Mark("perf.configured");
    g_samplerStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_samplerStop) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    // The configuring thread is the UI thread (wWinMain).
    if (g_samplerStop && wcsstr(commandLine, L"--halljoy-perf-stacks")) {
        g_stacks = static_cast<StackSample*>(VirtualAlloc(nullptr, sizeof(StackSample) * kStackCapacity,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        g_uiThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
            FALSE, GetCurrentThreadId());
        if (g_stacks && g_uiThread)
            g_stackSampler = CreateThread(nullptr, 0, StackSamplerThread, nullptr, 0, nullptr);
    }
}

bool Enabled() noexcept
{
    return g_enabled.load(std::memory_order_relaxed);
}

std::int64_t Now() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

void Mark(const char* name, std::uint64_t index) noexcept
{
    if (!Enabled()) return;
    const auto now = Now();
    Push(name, nullptr, index, now, now);
}

void Span(const char* name, std::uint64_t index, std::int64_t startQpc, const char* detail) noexcept
{
    if (!Enabled()) return;
    Push(name, detail, index, startQpc, Now());
}

const char* Intern(const char* text) noexcept
{
    static char table[256][64];
    static std::size_t used = 0;
    if (!text) return "?";
    for (std::size_t i = 0; i < used; ++i)
        if (std::strncmp(table[i], text, 63) == 0) return table[i];
    if (used >= std::size(table)) return "?";
    strncpy_s(table[used], text, _TRUNCATE);
    return table[used++];
}

void Flush() noexcept
{
    if (!Enabled() || !g_path[0]) return;
    if (g_sampler || g_stackSampler) SetEvent(g_samplerStop);
    if (g_stackSampler) {
        WaitForSingleObject(g_stackSampler, 2000);
        CloseHandle(g_stackSampler);
        g_stackSampler = nullptr;
    }
    if (g_sampler) {
        WaitForSingleObject(g_sampler, 2000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    FILE* file = nullptr;
    if (_wfopen_s(&file, g_path, L"w") != 0 || !file) return;
    const std::size_t count = std::min(g_count.load(std::memory_order_acquire), kCapacity);
    fwprintf(file, L"# HallJoy perf timeline v1: start_ms (since process creation) duration_ms tid name index detail\n");
    for (std::size_t i = 0; i < count; ++i) {
        const Record& r = g_records[i];
        if (!r.name) continue;
        fwprintf(file, L"event %.3f %.3f %lu %S %llu %S\n", Ms(r.start - g_originQpc), Ms(r.end - r.start),
            r.thread, r.name, static_cast<unsigned long long>(r.index), r.detail ? r.detail : "-");
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    fwprintf(file, L"process user_ms=%.1f kernel_ms=%.1f wall_ms=%.1f\n", FileTimeMs(user), FileTimeMs(kernel),
        Ms(Now() - g_originQpc));
    WriteThreads(file);
    // stk <t_ms> <module>+<rva> ... (leaf first)
    const std::size_t stacks = std::min(g_stackCount.load(std::memory_order_acquire), kStackCapacity);
    for (std::size_t i = 0; i < stacks; ++i) {
        const StackSample& sample = g_stacks[i];
        fwprintf(file, L"stk %.2f", Ms(sample.qpc - g_originQpc));
        for (std::size_t f = 0; f < kStackDepth && sample.frames[f]; ++f) {
            HMODULE module = nullptr;
            wchar_t name[MAX_PATH] = L"?";
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(sample.frames[f]), &module) && module) {
                wchar_t full[MAX_PATH]{};
                if (GetModuleFileNameW(module, full, MAX_PATH)) {
                    const wchar_t* slash = wcsrchr(full, L'\\');
                    wcscpy_s(name, slash ? slash + 1 : full);
                }
            }
            fwprintf(file, L" %ls+0x%llx", name,
                static_cast<unsigned long long>(sample.frames[f] - reinterpret_cast<std::uintptr_t>(module)));
        }
        fwprintf(file, L"\n");
    }
    // cpu <t_ms> <tid> <cumulative_cpu_ms> <module> <rva>
    const auto exeBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::size_t samples = std::min(g_sampleCount.load(std::memory_order_acquire), kSampleCapacity);
    for (std::size_t i = 0; i < samples; ++i) {
        const CpuSample& c = g_samples[i];
        HMODULE module = nullptr;
        wchar_t moduleName[MAX_PATH] = L"?";
        if (c.startAddress && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(c.startAddress), &module) && module) {
            wchar_t full[MAX_PATH]{};
            if (GetModuleFileNameW(module, full, MAX_PATH)) {
                const wchar_t* slash = wcsrchr(full, L'\\');
                wcscpy_s(moduleName, slash ? slash + 1 : full);
            }
        }
        const std::uintptr_t base = module ? reinterpret_cast<std::uintptr_t>(module) : 0;
        fwprintf(file, L"cpu %.1f %lu %.2f %ls 0x%llx\n", Ms(c.qpc - g_originQpc), c.thread,
            static_cast<double>(c.cpu100ns) / 10000.0, moduleName,
            static_cast<unsigned long long>(c.startAddress - base));
        (void)exeBase;
    }
    fclose(file);
}

} // namespace halljoy::perf
