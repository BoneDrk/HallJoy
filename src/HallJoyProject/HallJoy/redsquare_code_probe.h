#pragma once
// Bounded exact-device research through the normal HallJoy support log.
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <unordered_map>
#include <utility>
#include "hid_io_operation.h"
#include "alumix104_protocol.h"
#include "support_log.h"
#include <shellapi.h>
#include <atomic>
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")
namespace halljoy::redsquare_probe {
inline std::atomic<bool> cancelled{false};
inline std::atomic<HWND> statusWindow{nullptr};
inline constexpr UINT StatusMessage=WM_APP+382;
enum class Status : unsigned { Searching, NotFound, Opening, Waiting, NoData, SingleKey, TwoKeys, Ready, WaitingAnalogRelease, TypingObserved, Finishing, Done, Incomplete, AnalogActive, BatchScanning, BatchEcho, BatchCandidate, UnknownTrace, RawCaptureFull, Fault };
inline const wchar_t* StatusTitle(Status status) noexcept {
 switch(status){
 case Status::Searching:return L"HallJoy — Alumix 104: поиск устройства…";
 case Status::NotFound:return L"HallJoy — Alumix 104 не найден. Откройте лог";
 case Status::Opening:return L"HallJoy — Alumix 104: проверка соединения…";
 case Status::Waiting:return L"HallJoy — Alumix 104: нажмите клавишу";
 case Status::NoData:return L"HallJoy — Alumix 104: поток молчит; нажмите клавишу или закройте для лога";
 case Status::SingleKey:return L"HallJoy — Alumix 104: плавно нажмите и отпустите букву";
 case Status::TwoKeys:return L"HallJoy — Alumix 104: две буквы вместе; если этап стоит — закройте для лога";
 case Status::Ready:return L"HallJoy — Alumix 104: отпустите эти две буквы; если этап стоит — закройте для лога";
 case Status::WaitingAnalogRelease:return L"HallJoy — Alumix 104: буквы отпущены; жду освобождения датчиков";
 case Status::TypingObserved:return L"HallJoy — Alumix 104: отпускание подтверждено; сохраняю лог…";
 case Status::Finishing:return L"HallJoy — Alumix 104: выключаю режим и сохраняю лог…";
 case Status::Done:return L"HallJoy — Alumix 104: всё готово; пришлите HallJoy.log";
 case Status::Incomplete:return L"HallJoy — Alumix 104: данных мало. Откройте HallJoy.log";
 case Status::AnalogActive:return L"HallJoy — Alumix 104: аналог активен; проверьте геймпад, затем пришлите лог";
 case Status::BatchScanning:return L"HallJoy — Alumix 104: нажмите 2–6 букв вместе, отпускайте по одной";
 case Status::BatchEcho:return L"HallJoy — Alumix 104: 0x68 пока эхо; нажмите 2–6 букв, затем пришлите лог";
 case Status::BatchCandidate:return L"HallJoy — Alumix 104: данные найдены; отпустите буквы по одной, затем пришлите лог";
 case Status::UnknownTrace:return L"HallJoy — Alumix 104: запись HID-кадров; только тестовые буквы 2–6, затем закройте и пришлите лог";
 case Status::RawCaptureFull:return L"HallJoy — Alumix 104: кадры записаны; закройте и пришлите HallJoy.log";
 case Status::Fault:return L"HallJoy — Alumix 104: ошибка. Откройте HallJoy.log";
 }
 return L"HallJoy";
}
inline void BindWindow(HWND window) noexcept { statusWindow.store(window,std::memory_order_release); }
inline void Show(Status status) noexcept {
 if(const auto window=statusWindow.load(std::memory_order_acquire))
  PostMessageW(window,StatusMessage,static_cast<WPARAM>(status),0);
}
// Raw Input is observed on the UI thread. Only aggregate physical-letter
// transitions from the admitted keyboard are shared with the research worker.
enum class LetterTransition { None, Down, Up, Repeat, Orphan };
struct RawDigitalDevice {
 bool target=false;
 unsigned generation=0;
 std::array<bool,26> held{};
 unsigned heldCount=0;
 LetterTransition Observe(unsigned hid,bool down) noexcept {
  if(hid<4 || hid>29)return LetterTransition::None;
  auto& active=held[hid-4];
  if(down){if(active)return LetterTransition::Repeat;active=true;++heldCount;return LetterTransition::Down;}
  if(!active)return LetterTransition::Orphan;
  active=false;--heldCount;return LetterTransition::Up;
 }
};
inline std::unordered_map<HANDLE,RawDigitalDevice> rawDigitalDevices;
inline std::atomic<bool> rawInputAvailable{false},digitalEnabled{false},pairReleaseStage{false};
// Official exact-model keyboard map: physical letter HID usage 04..1D ->
// 55FB sensor index. Kept in memory; neither letter identities nor raw values are logged.
inline constexpr std::array<unsigned char,26> letterSensorIndex{
 49,69,67,51,35,52,53,54,40,55,56,57,71,70,41,42,33,36,50,37,39,68,34,66,38,65};
inline std::atomic<std::uint64_t> rawHeldState{0}; // upper32: revision, lower26: held letter mask
inline std::atomic<unsigned> rawHeldRevision{0},rawSessionGeneration{0};
inline std::atomic<std::uint64_t> rawMatched{0},rawNameFailed{0},rawCacheOverflow{0};
inline std::atomic<std::uint64_t> rawRemoved{0};
inline std::atomic<std::uint64_t> letterDown{0},letterUp{0},letterRepeat{0},letterOrphan{0};
inline std::atomic<std::uint64_t> readyLetterDown{0},readyLetterUp{0},peakLettersHeld{0};
inline bool MatchesRawName(const std::wstring& name){
 std::wstring lower=name;
 std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
 return lower.find(L"vid_0c45&pid_80ac")!=std::wstring::npos;
}
inline void SetRawInputAvailable(bool available) noexcept {rawInputAvailable.store(available);}
inline void ObserveRawKeyboard(HANDLE device,unsigned hid,bool down) noexcept {
 if(!digitalEnabled.load(std::memory_order_acquire) || !device)return;
 try {
  auto found=rawDigitalDevices.find(device);
  if(found==rawDigitalDevices.end()){
   if(rawDigitalDevices.size()>=64){++rawCacheOverflow;return;}
   wchar_t name[2048]{};UINT chars=_countof(name);
   const bool read=GetRawInputDeviceInfoW(device,RIDI_DEVICENAME,name,&chars)!=UINT(-1);
   if(!read){++rawNameFailed;return;}
   RawDigitalDevice entry;entry.target=MatchesRawName(name);
   found=rawDigitalDevices.emplace(device,entry).first;
   if(entry.target)++rawMatched;
   }
   auto& state=found->second;if(!state.target)return;
   const auto generation=rawSessionGeneration.load(std::memory_order_acquire);
   if(state.generation!=generation){state.held.fill(false);state.heldCount=0;state.generation=generation;}
   const auto transition=state.Observe(hid,down);
   if(transition==LetterTransition::Down || transition==LetterTransition::Up){
    std::uint32_t mask=0;
    for(unsigned i=0;i<26;++i)if(state.held[i])mask|=1u<<i;
    const auto revision=rawHeldRevision.fetch_add(1,std::memory_order_relaxed)+1;
    rawHeldState.store((std::uint64_t(revision)<<32)|mask,std::memory_order_release);
   }
   switch(transition){
    case LetterTransition::Down:
     ++letterDown;if(pairReleaseStage.load())++readyLetterDown;
    {auto peak=peakLettersHeld.load();while(state.heldCount>peak && !peakLettersHeld.compare_exchange_weak(peak,state.heldCount)){} }
    break;
    case LetterTransition::Up:
     ++letterUp;
     if(pairReleaseStage.load()){
      ++readyLetterUp;
     }
     break;
   case LetterTransition::Repeat:++letterRepeat;break;
   case LetterTransition::Orphan:++letterOrphan;break;
   default:break;
  }
 }catch(...){++rawNameFailed;}
}
inline void RawKeyboardRemoved(HANDLE device) noexcept {
 auto found=rawDigitalDevices.find(device);
 if(found!=rawDigitalDevices.end() && found->second.target){
  ++rawRemoved;
  const auto revision=rawHeldRevision.fetch_add(1,std::memory_order_relaxed)+1;
  rawHeldState.store((std::uint64_t(revision)<<32)|(1u<<31),std::memory_order_release);
 }
 rawDigitalDevices.erase(device);
}
inline bool RawDigitalSelfTest(){
 RawDigitalDevice d;
 return MatchesRawName(L"HID#VID_0C45&PID_80AC&MI_00") &&
  !MatchesRawName(L"HID#VID_0C45&PID_8032") &&
  d.Observe(4,true)==LetterTransition::Down &&
  d.Observe(4,true)==LetterTransition::Repeat &&
  d.Observe(5,true)==LetterTransition::Down && d.heldCount==2 &&
  d.Observe(4,false)==LetterTransition::Up &&
  d.Observe(4,false)==LetterTransition::Orphan &&
  d.Observe(5,false)==LetterTransition::Up && d.heldCount==0;
}
inline HANDLE worker=nullptr;
inline bool attempted=false; // Lifecycle callbacks serialize Start/Stop.
constexpr unsigned First=0x2a00,End=0xfa00; // 68 reference: flash C000..19000; exclude settings/macros below C000.
inline bool Request(unsigned size,unsigned offset,unsigned length,std::vector<unsigned char>& out){
 if((size!=33 && size!=65)||offset<First||offset>=End||!length||length>size-9||length>End-offset)return false;
 out.assign(size,0);out[1]=0xaa;out[2]=0x12;out[3]=(unsigned char)length;
 out[4]=(unsigned char)offset;out[5]=(unsigned char)(offset>>8);out[7]=1;return true;
}
inline bool Reply(const std::vector<unsigned char>& r,unsigned got,unsigned off,unsigned length){
 return got==r.size() && r.size()>=9+length && r[0]==0 && r[1]==0x55 && r[2]==0x12 && r[3]==length && (unsigned(r[4])|(unsigned(r[5])<<8))==off;
}
inline bool SelfTest(){
 std::vector<unsigned char> r;
 for(unsigned size:{33u,65u})for(unsigned off=First;off<End;){
  unsigned n=(std::min)(size-9,End-off);if(!Request(size,off,n,r)||r[2]!=0x12)return false;
  r[1]=0x55;if(!Reply(r,size,off,n)||Reply(r,size-1,off,n)||Reply(r,size,off+1,n))return false;
  off+=n;
 }
 return !Request(65,First-1,8,r)&&!Request(65,End,1,r)&&!Request(65,End-1,2,r)&&!Request(65,First,57,r)&&!Request(34,First,1,r);
}
struct Handle{HANDLE h=INVALID_HANDLE_VALUE;~Handle(){if(h!=INVALID_HANDLE_VALUE && h)CloseHandle(h);}};
inline bool Io(HANDLE h,bool write,std::vector<unsigned char>& b,DWORD& got){
 if(cancelled.load())return false;
 HidIoOperation op(h);DWORD error=0;
 auto start=write?op.StartWrite(b.data(),(DWORD)b.size(),&error):op.StartRead(b.data(),(DWORD)b.size(),&error);
 bool ok=false;
 if(start!=HidIoOperation::StartResult::Failed){
  if(start==HidIoOperation::StartResult::Pending && op.Wait(400)!=WAIT_OBJECT_0){
   op.CancelAndDrain(&got,&error);
   if(!error)error=ERROR_TIMEOUT;
  }else ok=op.Finish(&got,&error,false);
 }
 if(!ok){SupportLog_Event("redsquare.io_failed",write,SupportLog_Win32(error));return false;}
 if(got!=b.size()){SupportLog_Event("redsquare.io_short",got,SupportLog_Data(b.size()));return false;}
 return true;
}
inline bool Emit(const std::string& s){return SupportLog_RedSquareResearch(s.c_str());}
// Input reports include the Windows report-ID byte; WebHID's DataView does not.
enum class FrameKind { Other, AckOn, AckOff, Invalid, Sample };
struct StreamStats {
 std::uint64_t reports=0,samples=0,zero=0,shallow=0,near_max=0,changed=0,nonzero_changed=0,releases=0;
 std::uint64_t unique=0,unique_positive=0,malformed=0,out_of_catalog=0,range_anomaly=0,unrelated=0,idle_reads=0;
 std::uint64_t bad_report_id=0,bad_prefix=0,other_opcode=0,short_header=0;
 std::uint64_t ack_on=0,ack_off=0;
 std::uint64_t calibration_nonzero=0,calibration_changed=0,adc_invalid_bounds=0,adc_outside_bounds=0;
 std::uint64_t adc_changed=0,max_stroke_changed=0,min_flagged=0;
 std::uint64_t max_zero=0,max_zero_positive=0,over_full=0,over_110=0,over_125=0,over_150=0,over_200=0;
 std::array<bool,128> seen{},positive{},stage_two_seen{};
 std::array<bool,128> shallow_key{},near_key{},changed_key{},released_key{},has_previous{};
 std::array<unsigned short,128> previous{},previous_adc{},previous_max{};
 std::array<unsigned char,128> previous_calibration{};
 unsigned stage_two_count=0;
 std::uint64_t held_positive_samples=0,held_zero_samples=0,pair_witnesses=0;
 std::uint64_t pair_revision=0;
 std::uint32_t pair_positive_mask=0,witness_mask=0,release_zero_mask=0;
 std::array<bool,26> held_positive_letter{};
 std::array<std::uint64_t,26> held_positive_by_letter{},held_changed_by_letter{};
 FrameKind Observe(const std::vector<unsigned char>& r,DWORD got,bool stageTwo,std::uint64_t rawHeld=0,bool stageRelease=false) {
  ++reports;
  if(got<3){++unrelated;++short_header;return FrameKind::Other;}
  if(r[0]!=0){++unrelated;++bad_report_id;return FrameKind::Other;}
  if(r[1]!=0x55){++unrelated;++bad_prefix;return FrameKind::Other;}
  if(r[2]==0x66){++ack_on;return FrameKind::AckOn;}
  if(r[2]==0x67){++ack_off;return FrameKind::AckOff;}
  if(r[2]!=0xfb){++unrelated;++other_opcode;return FrameKind::Other;}
  if(got<15){++malformed;return FrameKind::Invalid;}
  const unsigned index=r[3],calibration=r[4];
  const unsigned adc_max=unsigned(r[5])|(unsigned(r[6])<<8);
  const unsigned adc_min_raw=unsigned(r[7])|(unsigned(r[8])<<8);
  const unsigned adc_min=adc_min_raw&0x7fff;
  const unsigned adc=unsigned(r[9])|(unsigned(r[10])<<8);
  const unsigned stroke=unsigned(r[11])|(unsigned(r[12])<<8);
  const unsigned maximum=unsigned(r[13])|(unsigned(r[14])<<8);
  if(index>120){++out_of_catalog;return FrameKind::Invalid;}
  ++samples;
  if(!seen[index]){seen[index]=true;++unique;}
  if(stroke && !positive[index]){positive[index]=true;++unique_positive;}
  if(!stroke)++zero;
  if(calibration)++calibration_nonzero;
  if(adc_min_raw&0x8000)++min_flagged;
  if(adc_min>adc_max)++adc_invalid_bounds;
  else if(adc<adc_min || adc>adc_max)++adc_outside_bounds;
  // Vendor UI converts keyStroke/100 and maxStroke/10: raw full scale is maxStroke*10.
  if(!maximum){++range_anomaly;++max_zero;if(stroke)++max_zero_positive;}
  else if(stroke>maximum*10u){
   ++range_anomaly;++over_full;
   if(stroke>maximum*11u)++over_110;
   if(stroke*2u>maximum*25u)++over_125;
   if(stroke>maximum*15u)++over_150;
   if(stroke>maximum*20u)++over_200;
  }
  if(maximum && stroke && stroke<maximum){++shallow;shallow_key[index]=true;}
  if(maximum && stroke<=maximum*10u && stroke>maximum*9u){++near_max;near_key[index]=true;}
  const bool positiveChanged=has_previous[index] && previous[index] && stroke && previous[index]!=stroke;
  const bool releasedNow=has_previous[index] && previous[index] && !stroke;
  if(has_previous[index]) {
   if(previous_calibration[index]!=calibration)++calibration_changed;
   if(previous_adc[index]!=adc)++adc_changed;
   if(previous_max[index]!=maximum)++max_stroke_changed;
   if(previous[index]!=stroke){++changed;if(previous[index] && stroke){++nonzero_changed;changed_key[index]=true;}}
   if(releasedNow){++releases;released_key[index]=true;}
  }
  previous[index]=static_cast<unsigned short>(stroke);
  previous_adc[index]=static_cast<unsigned short>(adc);
  previous_max[index]=static_cast<unsigned short>(maximum);
  previous_calibration[index]=static_cast<unsigned char>(calibration);
   has_previous[index]=true;
   if(stageTwo && stroke && !stage_two_seen[index]){stage_two_seen[index]=true;++stage_two_count;}
   const auto heldMask=static_cast<std::uint32_t>(rawHeld)&((1u<<26)-1);
   const auto heldRevision=rawHeld>>32;
   if(heldRevision!=pair_revision){pair_revision=heldRevision;pair_positive_mask=0;}
   if(stageTwo && heldMask){
    for(unsigned letter=0;letter<26;++letter)if(letterSensorIndex[letter]==index && (heldMask&(1u<<letter))){
     if(stroke){++held_positive_samples;++held_positive_by_letter[letter];held_positive_letter[letter]=true;}
     else {++held_zero_samples;pair_positive_mask&=~(1u<<letter);}
     if(positiveChanged)++held_changed_by_letter[letter];
     if(stroke && (heldMask&(heldMask-1))){
      const auto before=pair_positive_mask;
      pair_positive_mask|=1u<<letter;
      if(!(before&(before-1)) && (pair_positive_mask&(pair_positive_mask-1))){
       ++pair_witnesses;
       if(!witness_mask)witness_mask=pair_positive_mask;
      }
     }
     break;
    }
   }
   if(stageRelease && releasedNow)
    for(unsigned letter=0;letter<26;++letter)
     if((witness_mask&(1u<<letter)) && letterSensorIndex[letter]==index){release_zero_mask|=1u<<letter;break;}
   return FrameKind::Sample;
 }
 bool OneKeyComplete() const noexcept {
  for(unsigned i=0;i<121;++i)
   if(shallow_key[i] && near_key[i] && changed_key[i] && released_key[i])return true;
  return false;
 }
 bool TwoKeysComplete() const noexcept {return pair_witnesses>0;}
 unsigned PairDigitalReleased(std::uint64_t rawHeld) const noexcept {
  if(!witness_mask || (rawHeld&(1u<<31)))return 0;
  const auto stillHeld=static_cast<std::uint32_t>(rawHeld)&witness_mask;
  unsigned count=0;for(unsigned letter=0;letter<26;++letter)
   if((witness_mask&(1u<<letter)) && !(stillHeld&(1u<<letter)))++count;
  return count;
 }
 unsigned PairAnalogReleased() const noexcept {
  unsigned count=0;for(unsigned letter=0;letter<26;++letter)
   if((witness_mask&(1u<<letter)) && (release_zero_mask&(1u<<letter)))++count;
  return count;
 }
 bool PairReleaseComplete(std::uint64_t rawHeld) const noexcept {
  return PairDigitalReleased(rawHeld)==2 && PairAnalogReleased()==2;
 }
 std::pair<std::uint64_t,std::uint64_t> WitnessSampleBalance() const noexcept {
  std::uint64_t lo=UINT64_MAX,hi=0;
  for(unsigned letter=0;letter<26;++letter)if(witness_mask&(1u<<letter)){
   lo=(std::min)(lo,held_positive_by_letter[letter]);
   hi=(std::max)(hi,held_positive_by_letter[letter]);
  }
  return {lo==UINT64_MAX?0:lo,hi};
 }
 std::pair<std::uint64_t,std::uint64_t> WitnessChangeBalance() const noexcept {
  std::uint64_t lo=UINT64_MAX,hi=0;
  for(unsigned letter=0;letter<26;++letter)if(witness_mask&(1u<<letter)){
   lo=(std::min)(lo,held_changed_by_letter[letter]);
   hi=(std::max)(hi,held_changed_by_letter[letter]);
  }
  return {lo==UINT64_MAX?0:lo,hi};
 }
 unsigned HeldPositiveLetters() const noexcept {
  unsigned count=0;for(bool observed:held_positive_letter)if(observed)++count;return count;
 }
};
inline bool Milestone(std::uint64_t n) noexcept {return n && !(n&(n-1));}
inline bool StreamSelfTest(){
 StreamStats s;std::vector<unsigned char> r(65);
 r[1]=0x55;r[2]=0xfb;r[3]=83;r[5]=0x20;r[6]=3;r[7]=0x10;r[8]=0;r[9]=0x80;
 r[11]=10;r[13]=35;
 if(s.Observe(r,14,false)!=FrameKind::Invalid || s.samples)return false;
 if(s.Observe(r,65,false)!=FrameKind::Sample || s.OneKeyComplete())return false;
 r[11]=20;r[9]=0x90;s.Observe(r,65,false);
 r[11]=0x54;r[12]=1;s.Observe(r,65,false); // 340 raw stroke; maxStroke=35 => 350 raw full scale.
 r[11]=0;r[12]=0;s.Observe(r,65,false);
 if(!s.OneKeyComplete() || s.stage_two_count)return false;
 r[3]=1;r[11]=10;s.Observe(r,65,true);
  if(s.stage_two_count!=1 || s.TwoKeysComplete())return false;
  r[3]=2;s.Observe(r,65,true);
  if(s.stage_two_count!=2 || s.TwoKeysComplete())return false;
  StreamStats pair; r[3]=letterSensorIndex[0];r[11]=10;
  const auto twoLetters=(std::uint64_t(1)<<32)|3;
  pair.Observe(r,65,true,twoLetters);
  if(pair.TwoKeysComplete() || pair.HeldPositiveLetters()!=1)return false;
  r[3]=letterSensorIndex[1];pair.Observe(r,65,true,twoLetters);
  if(!pair.TwoKeysComplete() || pair.HeldPositiveLetters()!=2 || pair.pair_witnesses!=1)return false;
  pair.Observe(r,65,true,twoLetters);
  if(pair.pair_witnesses!=1)return false;
  if(pair.PairReleaseComplete((std::uint64_t(2)<<32)|3))return false;
  r[3]=letterSensorIndex[0];r[11]=0;
  pair.Observe(r,65,false,(std::uint64_t(2)<<32)|2,true);
  if(pair.PairDigitalReleased((std::uint64_t(2)<<32)|2)!=1 || pair.PairAnalogReleased()!=1)return false;
  r[3]=letterSensorIndex[1];
  pair.Observe(r,65,false,std::uint64_t(3)<<32,true);
  if(!pair.PairReleaseComplete(std::uint64_t(3)<<32) ||
     pair.PairReleaseComplete((std::uint64_t(4)<<32)|(1u<<31)))return false;
  StreamStats range;r[3]=1;r[13]=0;r[11]=0;range.Observe(r,65,false);
  r[11]=5;range.Observe(r,65,false);
  r[13]=35;r[11]=104;r[12]=1;range.Observe(r,65,false); // 360/350.
  r[11]=194;r[12]=1;range.Observe(r,65,false); // 450/350.
  r[11]=32;r[12]=3;range.Observe(r,65,false); // 800/350.
  if(range.max_zero!=2 || range.max_zero_positive!=1 || range.over_full!=3 ||
     range.over_110!=2 || range.over_125!=2 || range.over_150!=1 || range.over_200!=1)return false;
 r[2]=0x66;if(s.Observe(r,65,false)!=FrameKind::AckOn)return false;
 r[2]=0x67;if(s.Observe(r,65,false)!=FrameKind::AckOff)return false;
 r[2]=0xfb;r[3]=121;if(s.Observe(r,65,false)!=FrameKind::Invalid)return false;
 return s.samples==6 && s.releases==1 && s.nonzero_changed==2 &&
  s.adc_changed==1 && !s.adc_invalid_bounds && !s.adc_outside_bounds &&
  !s.range_anomaly && s.shallow==4 && s.near_max==1 &&
  s.ack_on==1 && s.ack_off==1 && s.out_of_catalog==1 &&
  Milestone(1) && Milestone(8) && !Milestone(3) &&
  StatusTitle(Status::Ready)[0]==L'H';
}
// Return 0 for a packet, 1 for an ordinary idle timeout, 2 for a transport failure.
inline int ReadMaybe(HANDLE h,std::vector<unsigned char>& b,DWORD& got,DWORD waitMs){
 got=0;HidIoOperation op(h);DWORD error=0;
 auto started=op.StartRead(b.data(),static_cast<DWORD>(b.size()),&error);
 if(started==HidIoOperation::StartResult::Failed){SupportLog_Event("redsquare.stream_read_failed",0,SupportLog_Win32(error));return 2;}
 const DWORD wait=started==HidIoOperation::StartResult::Pending?op.Wait(waitMs):WAIT_OBJECT_0;
 if(wait==WAIT_TIMEOUT){
  const bool completed=op.CancelAndDrain(&got,&error);
  if(completed && got==b.size())return 0;
  if(error==ERROR_OPERATION_ABORTED)return 1;
  SupportLog_Event("redsquare.stream_read_failed",0,SupportLog_Win32(error));return 2;
 }
 if(wait!=WAIT_OBJECT_0){const DWORD waitError=GetLastError();op.CancelAndDrain(&got,&error);SupportLog_Event("redsquare.stream_wait_failed",0,SupportLog_Win32(waitError));return 2;}
 if(!op.Finish(&got,&error,false)){SupportLog_Event("redsquare.stream_read_failed",0,SupportLog_Win32(error));return 2;}
 if(got!=b.size()){SupportLog_Event("redsquare.stream_short",got,SupportLog_Data(b.size()));return 2;}
 return 0;
}
inline std::vector<unsigned char> ModePacket(unsigned size,unsigned command){
 std::vector<unsigned char> b(size);b[1]=0xaa;b[2]=static_cast<unsigned char>(command);return b;
}
inline bool WriteMode(HANDLE h,unsigned size,unsigned command){
 auto b=ModePacket(size,command);
 // Official Ja(0x66/0x67,0,0) leaves its final-packet flag at zero.
 HidIoOperation op(h);DWORD got=0,error=0;
 auto started=op.StartWrite(b.data(),static_cast<DWORD>(b.size()),&error);
 bool ok=false;
 if(started!=HidIoOperation::StartResult::Failed){
  if(started==HidIoOperation::StartResult::Pending && op.Wait(400)!=WAIT_OBJECT_0)
   op.CancelAndDrain(&got,&error);
  else ok=op.Finish(&got,&error,false);
 }
 if(!ok || got!=b.size()){
  SupportLog_Event("redsquare.mode_write_failed",command,SupportLog_Win32(error));return false;
 }
 return true;
}
inline int Stream(HANDLE h,unsigned in,unsigned out){
 StreamStats stats;
 std::uint64_t droppedRecords=0,otherStreak=0,idleStreak=0;
 bool initialOffWrite=false,onWrite=false,finalOffWrite=false,finalOffAck=false;
 int result=0;
 Status stage=Status::Opening;
 auto note=[&](const std::string& line){if(!Emit(line))++droppedRecords;};
 auto phase=[&](Status status,const char* label){
  if(status==Status::Ready)pairReleaseStage.store(true,std::memory_order_release);
  stage=status;Show(status);note(std::string("phase=")+label+
   " samples="+std::to_string(stats.samples)+" unique_positive="+std::to_string(stats.unique_positive));
 };
 auto advanceRelease=[&](){
  const auto held=rawHeldState.load(std::memory_order_acquire);
  if(stage==Status::Ready && stats.PairDigitalReleased(held)==2)
   phase(Status::WaitingAnalogRelease,"pair_digital_released");
  if(stage==Status::WaitingAnalogRelease && stats.PairReleaseComplete(held)){
   phase(Status::TypingObserved,"paired_digital_analog_release_observed");return true;
  }
  return false;
 };
 try {
  note("stream_enter exact_alumix104=1 readiness_driven=1 commands=66,67 raw_values=0");
   note("raw_keyboard_registered="+std::to_string(rawInputAvailable.load())+
    " letter_identity=physical_hid_04_1d map=official_alumix104 counts_only=1");
  std::vector<unsigned char> rx(in);
  phase(Status::Opening,"opening");
  // Sequential writes follow the vendor's transient mode lease. Neither ACK nor
  // stream acquisition is cut short by an arbitrary experiment duration.
  initialOffWrite=WriteMode(h,out,0x67);
  note("initial_off_write="+std::to_string(initialOffWrite));
  if(!cancelled.load()){
   onWrite=WriteMode(h,out,0x66);
   note("on_write="+std::to_string(onWrite));
   phase(Status::Waiting,"waiting_stream");
  }
  while(!cancelled.load()){
   DWORD got=0;const int io=ReadMaybe(h,rx,got,250);
   if(io==2){result=23;note("transport_read_failed=1");break;}
   if(io==1){
    ++stats.idle_reads;++idleStreak;
    if(Milestone(idleStreak))note("idle_streak="+std::to_string(idleStreak)+
     " samples="+std::to_string(stats.samples)+" ack_on="+std::to_string(stats.ack_on));
    if(idleStreak==4 && (stage==Status::Waiting || stage==Status::Opening))Show(Status::NoData);
    if(advanceRelease())break;
    continue;
   }
   if(idleStreak>=4)Show(stage);
   idleStreak=0;
   const bool releaseStage=stage==Status::Ready || stage==Status::WaitingAnalogRelease;
   const auto kind=stats.Observe(rx,got,stage==Status::TwoKeys || releaseStage,
    rawHeldState.load(std::memory_order_acquire),releaseStage);
   if(kind==FrameKind::AckOn && stats.ack_on==1)note("on_ack=1");
   if(kind==FrameKind::AckOff && stats.ack_off==1)note("initial_off_ack=1");
   if(kind==FrameKind::Sample){
    otherStreak=0;
    if(stage==Status::Waiting){phase(Status::SingleKey,"single_key");}
    if(stage==Status::SingleKey && stats.OneKeyComplete())phase(Status::TwoKeys,"two_keys");
    if(stage==Status::TwoKeys && stats.TwoKeysComplete())phase(Status::Ready,"paired_analog_digital_hold_observed");
    if(Milestone(stats.samples))note("sample_checkpoint="+std::to_string(stats.samples)+
     " unique="+std::to_string(stats.unique)+" shallow="+std::to_string(stats.shallow)+
     " near_max="+std::to_string(stats.near_max)+" releases="+std::to_string(stats.releases));
   }else if(kind==FrameKind::Other || kind==FrameKind::Invalid){
    ++otherStreak;
    if(Milestone(stats.unrelated+stats.malformed+stats.out_of_catalog) ||
       (otherStreak>=4 && Milestone(otherStreak)))note("nonstream_checkpoint="+
     std::to_string(stats.unrelated+stats.malformed+stats.out_of_catalog)+
     " streak="+std::to_string(otherStreak)+
     " unrelated="+std::to_string(stats.unrelated)+" malformed="+std::to_string(stats.malformed)+
     " out_of_catalog="+std::to_string(stats.out_of_catalog));
   }
   if(advanceRelease())break;
  }
 }catch(...){result=24;SupportLog_Event("redsquare.stream_exception",1);}
 Show(Status::Finishing);note("phase=cleanup reason="+
  std::string(cancelled.load()?"pause_or_exit":result?"read_fault":
   stage==Status::TypingObserved?"evidence_complete":"stopped"));
 // Cleanup is bounded for the engine owner join. The experiment itself has no
 // duration limit; individual USB operations retain finite cancellation bounds.
 try {
  finalOffWrite=WriteMode(h,out,0x67);
  if(!finalOffWrite){note("final_off_retry=1");finalOffWrite=WriteMode(h,out,0x67);}
  const auto ackBefore=stats.ack_off;
  if(finalOffWrite){
   std::vector<unsigned char> rx(in);
   for(unsigned i=0;i<8 && stats.ack_off==ackBefore;++i){
    DWORD got=0;const int io=ReadMaybe(h,rx,got,100);
    if(io==2){if(!result)result=25;break;}
    if(io==0)stats.Observe(rx,got,false);
   }
   finalOffAck=stats.ack_off>ackBefore;
  }
 }catch(...){if(!result)result=26;SupportLog_Event("redsquare.cleanup_exception",1);}
 note("cleanup initial_off_write="+std::to_string(initialOffWrite)+" on_write="+std::to_string(onWrite)+
  " final_off_write="+std::to_string(finalOffWrite)+" final_off_ack="+std::to_string(finalOffAck));
 note("totals reports="+std::to_string(stats.reports)+" samples="+std::to_string(stats.samples)+
  " unique="+std::to_string(stats.unique)+" unique_positive="+std::to_string(stats.unique_positive)+
  " zero="+std::to_string(stats.zero)+" releases="+std::to_string(stats.releases)+
  " changed="+std::to_string(stats.changed)+" nonzero_changed="+std::to_string(stats.nonzero_changed));
 note("depth_coverage scale=stroke_x100_max_x10 shallow="+std::to_string(stats.shallow)+" near_max="+std::to_string(stats.near_max)+
   " range_anomaly="+std::to_string(stats.range_anomaly)+" one_key_complete="+
   std::to_string(stats.OneKeyComplete())+" two_indices_after_prompt="+std::to_string(stats.stage_two_count));
 note("range_categories max_zero="+std::to_string(stats.max_zero)+
  " max_zero_positive="+std::to_string(stats.max_zero_positive)+
  " above_full="+std::to_string(stats.over_full)+
  " above_110="+std::to_string(stats.over_110)+
  " above_125="+std::to_string(stats.over_125)+
  " above_150="+std::to_string(stats.over_150)+
  " above_200="+std::to_string(stats.over_200));
 note("paired_hold evidence=two_mapped_positive_sensors_during_same_digital_hold_revision"+
  std::string(" witnesses=")+std::to_string(stats.pair_witnesses)+
  " held_positive_samples="+std::to_string(stats.held_positive_samples)+
  " held_zero_samples="+std::to_string(stats.held_zero_samples)+
  " distinct_held_positive_letters="+std::to_string(stats.HeldPositiveLetters())+
  " digital_peak_held="+std::to_string(peakLettersHeld.load()));
 const auto witnessSamples=stats.WitnessSampleBalance(),witnessChanges=stats.WitnessChangeBalance();
 note("pair_sensor_balance held_positive_min="+std::to_string(witnessSamples.first)+
  " held_positive_max="+std::to_string(witnessSamples.second)+
  " held_changes_min="+std::to_string(witnessChanges.first)+
  " held_changes_max="+std::to_string(witnessChanges.second));
 const auto finalHeld=rawHeldState.load(std::memory_order_acquire);
 note("pair_release digital_up="+std::to_string(stats.PairDigitalReleased(finalHeld))+
  " analog_zero="+std::to_string(stats.PairAnalogReleased())+
  " raw_removed="+std::to_string(rawRemoved.load()));
 note("quality malformed="+std::to_string(stats.malformed)+" out_of_catalog="+
  std::to_string(stats.out_of_catalog)+" unrelated="+std::to_string(stats.unrelated)+
  " idle_reads="+std::to_string(stats.idle_reads)+" ack_on="+std::to_string(stats.ack_on)+
  " ack_off="+std::to_string(stats.ack_off)+" record_drops="+std::to_string(droppedRecords));
 note("nonstream_types bad_report_id="+std::to_string(stats.bad_report_id)+
  " bad_prefix="+std::to_string(stats.bad_prefix)+" other_opcode="+
  std::to_string(stats.other_opcode)+" short_header="+std::to_string(stats.short_header));
 note("digital_identity raw_registered="+std::to_string(rawInputAvailable.load())+
  " matched_devices="+std::to_string(rawMatched.load())+
  " name_failures="+std::to_string(rawNameFailed.load())+
  " cache_overflow="+std::to_string(rawCacheOverflow.load())+
  " removed="+std::to_string(rawRemoved.load()));
 note("digital_letters down="+std::to_string(letterDown.load())+
  " up="+std::to_string(letterUp.load())+
  " repeats="+std::to_string(letterRepeat.load())+
  " orphan_up="+std::to_string(letterOrphan.load())+
   " after_pair_down="+std::to_string(readyLetterDown.load())+
   " after_pair_up="+std::to_string(readyLetterUp.load())+
   " peak_held="+std::to_string(peakLettersHeld.load()));
 note("sensor_quality calibration_nonzero="+std::to_string(stats.calibration_nonzero)+
  " calibration_changed="+std::to_string(stats.calibration_changed)+
  " adc_changed="+std::to_string(stats.adc_changed)+
  " adc_invalid_bounds="+std::to_string(stats.adc_invalid_bounds)+
  " adc_outside_bounds="+std::to_string(stats.adc_outside_bounds)+
  " max_stroke_changed="+std::to_string(stats.max_stroke_changed)+
  " min_flagged="+std::to_string(stats.min_flagged));
 const bool analogComplete=stats.OneKeyComplete() && stats.TwoKeysComplete();
 const bool releaseComplete=stats.PairReleaseComplete(finalHeld);
 const bool complete=analogComplete && releaseComplete;
 note("evidence complete="+std::to_string(complete)+" missing_stream="+std::to_string(!stats.samples)+
  " missing_shallow="+std::to_string(!stats.shallow)+" missing_near_max="+std::to_string(!stats.near_max)+
  " missing_release="+std::to_string(!stats.releases)+" missing_two_indices="+
   std::to_string(stats.stage_two_count<2)+" missing_pair_corroboration="+
   std::to_string(!stats.TwoKeysComplete())+" missing_pair_digital_up="+
   std::to_string(stats.PairDigitalReleased(finalHeld)<2)+" missing_pair_analog_zero="+
   std::to_string(stats.PairAnalogReleased()<2)+" text_delivery_unverified=1");
 if(!finalOffWrite || !finalOffAck){if(!result)result=22;}
 note("end result="+std::to_string(result)+" cancelled="+std::to_string(cancelled.load())+
  " final_stage="+std::to_string(static_cast<unsigned>(stage)));
 digitalEnabled.store(false,std::memory_order_release);
 pairReleaseStage.store(false,std::memory_order_release);
 Show(result?Status::Fault:complete?Status::Done:Status::Incomplete);
 return result;
}
inline int Run(){
 // Exact model and collection only. The prior command12 capture is already complete.
 // The test length is controlled by evidence and the user's Pause/Exit action.
 Show(Status::Searching);
 Emit("HallJoy RedSquare stream probe v5; phase=searching; exact model only");
 GUID g{};HidD_GetHidGuid(&g);auto list=SetupDiGetClassDevsW(&g,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(list==INVALID_HANDLE_VALUE){SupportLog_Event("redsquare.enumeration_failed",0,SupportLog_Win32(GetLastError()));Show(Status::Fault);return 10;}
 struct ListGuard{HDEVINFO h;~ListGuard(){SetupDiDestroyDeviceInfoList(h);}} guard{list};
 std::wstring path,product;unsigned in=0,out=0,matches=0,vidpid=0,productRejected=0,collectionRejected=0;
 for(DWORD i=0;i<256 && !cancelled.load();++i){
  SP_DEVICE_INTERFACE_DATA it{};it.cbSize=sizeof(it);if(!SetupDiEnumDeviceInterfaces(list,nullptr,&g,i,&it))break;
  DWORD size=0;SetupDiGetDeviceInterfaceDetailW(list,&it,nullptr,0,&size,nullptr);if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||size>65536)continue;
  std::vector<unsigned char> mem(size);auto d=(SP_DEVICE_INTERFACE_DETAIL_DATA_W*)mem.data();d->cbSize=sizeof(*d);
  if(!SetupDiGetDeviceInterfaceDetailW(list,&it,d,size,nullptr,nullptr))continue;
  Handle meta{CreateFileW(d->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr)};if(meta.h==INVALID_HANDLE_VALUE)continue;
  HIDD_ATTRIBUTES a{};a.Size=sizeof(a);if(!HidD_GetAttributes(meta.h,&a)||a.VendorID!=0x0c45||a.ProductID!=0x80ac)continue;
  ++vidpid;
  wchar_t name[256]{};if(!HidD_GetProductString(meta.h,name,sizeof(name))){SupportLog_Event("redsquare.identity_unavailable",0,SupportLog_Win32(GetLastError()));continue;}
  if(wcscmp(name,L"Alumix 104 Yotei Magnetic") && wcscmp(name,L"Alumix TKL Horizon")){++productRejected;SupportLog_Event("redsquare.product_not_allowlisted",1);continue;}
  PHIDP_PREPARSED_DATA prep=nullptr;HIDP_CAPS c{};if(!HidD_GetPreparsedData(meta.h,&prep)){SupportLog_Event("redsquare.caps_unavailable",0,SupportLog_Win32(GetLastError()));continue;}
  auto ok=HidP_GetCaps(prep,&c);HidD_FreePreparsedData(prep);
  if(ok!=HIDP_STATUS_SUCCESS||c.UsagePage!=0xff68||c.Usage!=0x61||c.InputReportByteLength!=c.OutputReportByteLength||(c.OutputReportByteLength!=33&&c.OutputReportByteLength!=65)){++collectionRejected;SupportLog_Event("redsquare.collection_rejected",(unsigned(c.UsagePage)<<16)|c.Usage,SupportLog_Data((unsigned(c.InputReportByteLength)<<16)|c.OutputReportByteLength));continue;}
  ++matches;path=d->DevicePath;product=name;in=c.InputReportByteLength;out=c.OutputReportByteLength;
 }
 Emit("admission vidpid="+std::to_string(vidpid)+" product_rejected="+std::to_string(productRejected)+
  " collection_rejected="+std::to_string(collectionRejected)+" matched="+std::to_string(matches));
 if(cancelled.load()){Emit("phase=cancelled_before_open");Show(Status::Done);return 18;}
 if(!matches){Emit("phase=no_exact_device");Show(Status::NotFound);return 0;}
 if(matches!=1){Emit("phase=ambiguous_collection");Show(Status::Fault);return 13;}
 const std::string model=product==L"Alumix 104 Yotei Magnetic"?"Alumix 104 Yotei Magnetic":"Alumix TKL Horizon";
 if(!Emit("model="+model+" vid=0c45 pid=80ac report_bytes="+std::to_string(out))){Show(Status::Fault);return 12;}
 if(model!="Alumix 104 Yotei Magnetic"){Emit("phase=other_shared_pid_model");Show(Status::NotFound);return 0;}
 digitalEnabled.store(true,std::memory_order_release);
 Show(Status::Opening);
 Handle dev{CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr)};
 if(dev.h==INVALID_HANDLE_VALUE){digitalEnabled.store(false,std::memory_order_release);Emit("open_failed="+std::to_string(GetLastError()));Show(Status::Fault);return 14;}
 const bool buffered=HidD_SetNumInputBuffers(dev.h,256)!=FALSE;
 Emit(std::string("input_buffers_256=")+(buffered?"1":"0"));
 return Stream(dev.h,in,out);
}
inline DWORD WINAPI Background(void*) noexcept {
 try {
  if(!cancelled.load()) { auto result=Run(); if(result)SupportLog_Event("redsquare.research_result",result); }
 } catch(...) {digitalEnabled.store(false,std::memory_order_release);pairReleaseStage.store(false,std::memory_order_release);SupportLog_Event("redsquare.research_exception",1);Emit("phase=unhandled_exception");Show(Status::Fault); }
 return 0;
}
inline void Start() noexcept {
 if(attempted)return;
 attempted=true;cancelled.store(false);
 digitalEnabled.store(false);pairReleaseStage.store(false);
 rawMatched.store(0);rawNameFailed.store(0);rawCacheOverflow.store(0);rawRemoved.store(0);
 letterDown.store(0);letterUp.store(0);letterRepeat.store(0);letterOrphan.store(0);
 readyLetterDown.store(0);readyLetterUp.store(0);peakLettersHeld.store(0);
 rawHeldRevision.store(0);rawHeldState.store(0);rawSessionGeneration.fetch_add(1);
 worker=CreateThread(nullptr,0,Background,nullptr,0,nullptr);
 if(!worker){SupportLog_Event("redsquare.research_start_failed",0,SupportLog_Win32(GetLastError()));Show(Status::Fault);}
}
inline bool Stop() noexcept {
 cancelled.store(true);
 if(!worker)return true;
 if(WaitForSingleObject(worker,2500)!=WAIT_OBJECT_0)return false;
 CloseHandle(worker);worker=nullptr;return true;
}
inline bool TryRun(int& result){
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return false;
 bool test=argc==2&&!wcscmp(argv[1],L"--halljoy-redsquare-probe-self-test");
 if(test){auto on=ModePacket(65,0x66),off=ModePacket(33,0x67);result=SelfTest()&&StreamSelfTest()&&RawDigitalSelfTest()&&halljoy::alumix104::SelfTest()&&on[0]==0&&on[1]==0xaa&&on[2]==0x66&&on[7]==0&&off[2]==0x67&&off[7]==0?0:1;}
 LocalFree(argv);return test;
}
}
