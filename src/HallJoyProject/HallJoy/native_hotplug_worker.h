#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Background owner of periodic native-protocol hotplug checks. Discovery opens
// HID interfaces and probes devices; it must never run on the realtime output
// thread. The owning service starts this worker after opening its gate and
// joins it before stopping the protocol poller, so hotplug can never race the
// final stop.
namespace halljoy::native_hotplug
{
class Worker final
{
public:
    using Tick = void (*)(ULONGLONG nowMs);

    Worker() = default;
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Idempotent. False means no worker is running for this service generation.
    bool Start(Tick tick, DWORD periodMs) noexcept
    {
        if (thread_)
        {
            if (WaitForSingleObject(thread_, 0) != WAIT_OBJECT_0)
                return !stopRequested_; // Running, or a retained timed-out stop.
            CloseHandle(thread_);
            thread_ = nullptr;
            if (stop_) CloseHandle(stop_);
            stop_ = nullptr;
        }
        if (!tick) return false;
        stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stop_) return false;
        tick_ = tick;
        periodMs_ = periodMs ? periodMs : 1;
        stopRequested_ = false;
        thread_ = CreateThread(nullptr, 0, &Worker::ThreadProc, this, 0, nullptr);
        if (!thread_)
        {
            CloseHandle(stop_);
            stop_ = nullptr;
            return false;
        }
        return true;
    }

    // True when no worker remains. A timed-out join retains both handles; the
    // worker has been signalled and exits after its current discovery pass.
    bool Stop(DWORD timeoutMs) noexcept
    {
        if (!thread_) return true;
        stopRequested_ = true;
        if (stop_) SetEvent(stop_);
        if (WaitForSingleObject(thread_, timeoutMs) != WAIT_OBJECT_0)
            return false;
        CloseHandle(thread_);
        thread_ = nullptr;
        if (stop_) CloseHandle(stop_);
        stop_ = nullptr;
        return true;
    }

private:
    static DWORD WINAPI ThreadProc(void* context) noexcept
    {
        auto* self = static_cast<Worker*>(context);
        while (WaitForSingleObject(self->stop_, self->periodMs_) == WAIT_TIMEOUT)
        {
            try
            {
                self->tick_(GetTickCount64());
            }
            catch (...)
            {
                // The next pass retries; the protocol poller owns published input.
            }
        }
        return 0;
    }

    HANDLE thread_ = nullptr;
    HANDLE stop_ = nullptr;
    Tick tick_ = nullptr;
    DWORD periodMs_ = 100;
    bool stopRequested_ = false;
};
}
