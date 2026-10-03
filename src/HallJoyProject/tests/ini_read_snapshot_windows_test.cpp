// Differential test: ini::Read served by a ReadSnapshot must return exactly
// what the per-key Win32 path returns (value and success), for every probed
// section/key and buffer limit. Optional argv[1]: a directory whose *.ini files
// (real settings, bindings, layouts) are compared key by key as well.
#include "bounded_ini.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;

static void Compare(const std::wstring& path, const wchar_t* section, const wchar_t* key) {
    for (std::size_t maximum : {std::size_t(2), std::size_t(3), std::size_t(5), std::size_t(8), std::size_t(16),
                                std::size_t(129), std::size_t(300), std::size_t(65536)}) {
        std::wstring legacy, cached;
        const bool legacyOk = halljoy::ini::Read(path.c_str(), section, key, legacy, maximum);
        bool cachedOk = false;
        {
            halljoy::ini::ReadSnapshot snapshot(path.c_str());
            cachedOk = halljoy::ini::Read(path.c_str(), section, key, cached, maximum);
        }
        if (legacyOk != cachedOk || (legacyOk && legacy != cached)) {
            ++failures;
            std::fwprintf(stderr, L"MISMATCH %ls [%ls] %ls max=%zu legacy=%d '%ls' cached=%d '%ls'\n",
                path.c_str(), section, key, maximum, legacyOk, legacy.c_str(), cachedOk, cached.c_str());
        }
    }
}

static std::vector<std::wstring> Split(const std::vector<wchar_t>& buffer) {
    std::vector<std::wstring> out;
    for (const wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1) out.emplace_back(p);
    return out;
}

static std::size_t CompareWholeFile(const std::wstring& path) {
    std::size_t probes = 0;
    std::vector<wchar_t> names(1 << 16);
    GetPrivateProfileSectionNamesW(names.data(), static_cast<DWORD>(names.size()), path.c_str());
    for (const auto& section : Split(names)) {
        std::vector<wchar_t> lines(1 << 20);
        GetPrivateProfileSectionW(section.c_str(), lines.data(), static_cast<DWORD>(lines.size()), path.c_str());
        for (const auto& line : Split(lines)) {
            const auto equals = line.find(L'=');
            std::wstring key = line.substr(0, equals == std::wstring::npos ? line.size() : equals);
            Compare(path, section.c_str(), key.c_str());
            // Case-insensitive and padded spellings must resolve identically.
            std::wstring upper = key;
            for (auto& c : upper) c = static_cast<wchar_t>(towupper(c));
            Compare(path, section.c_str(), upper.c_str());
            ++probes;
        }
        Compare(path, section.c_str(), L"HallJoyDefinitelyMissingKey");
    }
    Compare(path, L"HallJoyDefinitelyMissingSection", L"Key");
    return probes;
}

static std::wstring TempFile(const wchar_t* tag) {
    wchar_t directory[MAX_PATH]{};
    GetTempPathW(MAX_PATH, directory);
    return std::wstring(directory) + L"hj-ini-snapshot-" + std::to_wstring(GetCurrentProcessId()) + L"-" + tag + L".ini";
}

int main(int argc, char** argv) {
    const std::wstring body =
        L"; comment line\r\n# hash comment\r\n"
        L"[Main]\r\nPollingMs=1\r\n  Leading=lead\r\nTrailing   =trail\r\nSpaced = both  \r\n\tTabbed\t=\ttab\t\r\n"
        L"Empty=\r\nQuoted=\"double\"\r\nSingle='single'\r\nMixed=\"mixed'\r\nOneQuote=\"\r\nInner=a\"b\"c\r\n"
        L"Equals=a=b=c\r\nNoEquals\r\nDup=first\r\nDUP=second\r\n;Commented=x\r\nUnicode=\x0416\x65E5\x00e9\r\n"
        L"Long=" + std::wstring(400, L'x') + L"\r\nSeven=1234567\r\nEight=12345678\r\n"
        L"[ main ]\r\nPadded=section\r\n[Main]\r\nPollingMs=second-section\r\nOnlyInSecond=2\r\n"
        L"[Empty Section]\r\n[Input]\r\nDeadzoneLow=80\r\n";
    const wchar_t* keys[] = {L"PollingMs", L"pollingms", L"Leading", L"  Leading", L"Trailing", L"Trailing   ",
        L"Spaced", L"Tabbed", L"Empty", L"Quoted", L"Single", L"Mixed", L"OneQuote", L"Inner", L"Equals",
        L"NoEquals", L"Dup", L"dup", L";Commented", L"Unicode", L"Long", L"Seven", L"Eight", L"Padded",
        L"OnlyInSecond", L"Missing", L"DeadzoneLow", L"\tSpaced", L"Spaced\t", L"\tTabbed\t"};
    // Not probed: a key of only spaces crashes kernel32's GetPrivateProfileStringW.
    const wchar_t* sections[] = {L"Main", L"MAIN", L" main ", L"main", L"Empty Section", L"Input", L"Absent"};

    // UTF-16 LE with BOM (HallJoy's writer) and ANSI (legacy files).
    const std::wstring utf16 = TempFile(L"utf16"), ansi = TempFile(L"ansi");
    {
        std::ofstream out(std::filesystem::path(utf16), std::ios::binary);
        const wchar_t bom = 0xFEFF;
        out.write(reinterpret_cast<const char*>(&bom), 2);
        out.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size() * 2));
    }
    {
        std::string narrow;
        for (wchar_t c : body) narrow.push_back(c < 128 ? static_cast<char>(c) : '?');
        std::ofstream out(std::filesystem::path(ansi), std::ios::binary);
        out.write(narrow.data(), static_cast<std::streamsize>(narrow.size()));
    }
    std::size_t probes = 0;
    for (const auto& path : {utf16, ansi}) {
        halljoy::ini::ReadFile lease(path.c_str());
        if (!lease) { std::fprintf(stderr, "cannot pin test file\n"); return 1; }
        for (const wchar_t* section : sections)
            for (const wchar_t* key : keys) { Compare(path, section, key); ++probes; }
        probes += CompareWholeFile(path);
    }
    DeleteFileW(utf16.c_str());
    DeleteFileW(ansi.c_str());

    // Nested snapshots: an inner snapshot of another file must not capture reads of the outer one.
    {
        const std::wstring a = TempFile(L"a"), b = TempFile(L"b");
        WritePrivateProfileStringW(L"S", L"K", L"from-a", a.c_str());
        WritePrivateProfileStringW(L"S", L"K", L"from-b", b.c_str());
        halljoy::ini::ReadSnapshot outer(a.c_str());
        {
            halljoy::ini::ReadSnapshot inner(b.c_str());
            std::wstring va, vb;
            if (!halljoy::ini::Read(a.c_str(), L"S", L"K", va) || va != L"from-a" ||
                !halljoy::ini::Read(b.c_str(), L"S", L"K", vb) || vb != L"from-b") { ++failures; std::fprintf(stderr, "nested snapshot mismatch\n"); }
        }
        DeleteFileW(a.c_str()); DeleteFileW(b.c_str());
    }

    std::size_t realFiles = 0;
    if (argc > 1) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1], ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".ini") continue;
            halljoy::ini::ReadFile lease(entry.path().c_str());
            if (!lease) continue;
            probes += CompareWholeFile(entry.path().wstring());
            ++realFiles;
        }
    }
    if (failures) { std::fprintf(stderr, "INI_READ_SNAPSHOT=FAIL mismatches=%d\n", failures); return 1; }
    std::printf("INI_READ_SNAPSHOT=PASS probes=%zu real_files=%zu limits=8 encodings=utf16,ansi nested\n", probes, realFiles);
    return 0;
}
