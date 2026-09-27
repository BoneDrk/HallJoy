// Execute the production session with synchronous fake HID I/O; no hardware access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <vector>
#include <array>
#include <cassert>
#include <stdexcept>
#include <cstdio>
static std::array<unsigned char,64> request{};
static std::vector<int> modes;
static int scenario=0, streamRead=0;
void CheckHeld(); void RequestStop();
static HANDLE FakeCreate(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE) {
    return CreateEventW(nullptr,TRUE,FALSE,nullptr);
}
static BOOLEAN FakeBuffers(HANDLE,ULONG) { return TRUE; }
static BOOLEAN FakeFlush(HANDLE) { return TRUE; }
static BOOL FakeWrite(HANDLE,LPCVOID buffer,DWORD n,LPDWORD,LPOVERLAPPED o) {
    assert(n==64); memcpy(request.data(),buffer,64); o->InternalHigh=n;
    if(request[1]==0x36) modes.push_back(request[8]);
    else assert(request[1]==0x12 || request[1]==0x16);
    return TRUE;
}
static BOOL FakeResult(HANDLE,LPOVERLAPPED o,LPDWORD n,BOOL) { *n=static_cast<DWORD>(o->InternalHigh);return TRUE; }
static BOOL FakeRead(HANDLE,LPVOID buffer,DWORD n,LPDWORD,LPOVERLAPPED o) {
    auto* b=static_cast<unsigned char*>(buffer); memset(b,0,n);o->InternalHigh=n;
    if(n==64) {
        memcpy(b,request.data(),64); b[7]=0x55;
        if(request[1]==0x36 && request[8] && scenario==1) b[7]=0x0f;
        if(request[1]==0x12 || request[1]==0x16) {
            std::array<unsigned char,384> payload{};
            if(request[1]==0x12) { payload[4]=128; if(scenario==4) payload[4]=0; }
            else for(int k=0;k<68;++k) {payload[k*3]=0x10;payload[k*3+2]=static_cast<unsigned char>(k+4);}
            const unsigned offset=request[2]|request[3]<<8;
            assert(offset+request[4]<=payload.size()); memcpy(b+8,payload.data()+offset,request[4]);
            if(scenario==5) RequestStop();
        }
        return TRUE;
    }
    assert(n==3); ++streamRead;
    if(streamRead==3) {
        CheckHeld();
        if(scenario==2) { SetLastError(ERROR_DEVICE_NOT_CONNECTED);return FALSE; }
        if(scenario==3) throw std::runtime_error("synthetic transport exception");
    }
    b[0]=7;b[1]=0;b[2]=streamRead==1?0x5c:streamRead==2?0x57:0x40;
    if(streamRead==4) RequestStop();
    return TRUE;
}
#define CreateFileW FakeCreate
#define HidD_SetNumInputBuffers FakeBuffers
#define HidD_FlushQueue FakeFlush
#define WriteFile FakeWrite
#define ReadFile FakeRead
#define GetOverlappedResult FakeResult
#include "../HallJoy/mad68_dual_trial_backend.cpp"
#undef CreateFileW
#undef ReadFile
#undef WriteFile
#undef GetOverlappedResult
void CheckHeld(){assert(Connected() && Get(4)==462);}
void RequestStop(){g_stop.store(true);}
void DebugLog_Write(const wchar_t*,...){}
void DebugLog_WriteBuffered(const wchar_t*,...){}
void SupportLog_Event(const char*,std::uint64_t,SupportLogDetail) noexcept {}
void SupportLog_ReportFailure(const char*,std::uint64_t) noexcept {}
bool NativeAnalogRouting_IsClaimed(const wchar_t*){return false;}
bool NativeAnalogRouting_IsClaimedBy(const wchar_t*,NativeAnalogProtocol){return false;}
bool NativeAnalogRouting_Claim(std::uint16_t,std::uint16_t,const wchar_t*,NativeAnalogProtocol){return true;}
int main(){
    ControlReport late{};late[0]=6;late[1]=0x36;late[4]=1;late[7]=0x55;late[8]=1;
    assert(IsControlAck(late,64,0x36,true)); assert(!IsControlAck(late,64,0x36,false));
    DevicePair pair{};pair.control.path=L"fake";pair.stream.path=L"fake";
    pair.control.caps.OutputReportByteLength=64;pair.stream.caps.InputReportByteLength=3;
    for(scenario=0;scenario<6;++scenario){
        modes.clear();streamRead=0;g_stop.store(false);g_connected.store(false);ClearVisualAnalog();
        bool result=false,exception=false;
        try { result=RunCandidate(pair); } catch(const std::runtime_error&) { exception=true; }
        assert(!Connected() && Get(4)==0 && !g_mapReady.load());
        if(scenario<4) assert((modes==std::vector<int>{1,0})); else assert(modes.empty());
        assert(exception==(scenario==3)); assert(result==(scenario==0));
    }
    puts("MAD68 Dual production session PASS: mapped gamepad values/release, failed ACK, disconnect, exception, bad map, stop during map; exit cleanup");
}
