#include "keyboard_hook_thread.h"
#include <cassert>
static HWND workerWindow;
static HANDLE workerDone;
static DWORD workerId;
static LRESULT CALLBACK PassHook(int code, WPARAM w, LPARAM l) { return CallNextHookEx(nullptr,code,w,l); }
static LRESULT CALLBACK WorkerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_APP) { SetEvent(workerDone); return 0; }
    return DefWindowProcW(hwnd,msg,w,l);
}
static void InitializeWorker() {
    workerId = GetCurrentThreadId();
    WNDCLASSW cls{}; cls.lpfnWndProc = WorkerProc;
    cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"HallJoyHookPumpTest";
    RegisterClassW(&cls);
    workerWindow = CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
}
int main() {
    using namespace halljoy::block_keys;
    workerDone = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    KeyboardHookThread worker;
    assert(worker.Start(PassHook,InitializeWorker) == ERROR_SUCCESS);
    assert(workerWindow && workerId != GetCurrentThreadId());
    assert(PostMessageW(workerWindow,WM_APP,0,0));
    // Calling/UI thread does NOT pump messages while waiting, like a blocked
    // settings save. The real hook thread must continue dispatching independently.
    assert(WaitForSingleObject(workerDone,1000) == WAIT_OBJECT_0);
    worker.Stop(); worker.Stop();
    assert(!IsWindow(workerWindow));
    CloseHandle(workerDone);
}
