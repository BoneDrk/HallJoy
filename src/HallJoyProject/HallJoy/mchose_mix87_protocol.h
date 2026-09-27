#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace halljoy::mix87 {
using Report = std::array<std::uint8_t,64>;
using Settings = std::array<std::uint8_t,256>;
using SettingsPage = std::array<std::uint8_t,8192>;
using Base = std::array<std::uint8_t,8>;
constexpr std::uint16_t Vid=0x3837, Pid=0x300d;
constexpr std::uint32_t BaseAddress=0x2a000, SettingsAddress=0x2c000;
inline std::uint8_t Checksum(const Report& p) noexcept {
    unsigned sum=p[4];
    if(p[4]>56)return 0;
    for(unsigned i=5;i<8u+p[4];++i)sum+=p[i];
    return static_cast<std::uint8_t>(sum);
}
inline Report Read(std::uint32_t address,std::uint8_t size) noexcept {
    Report p{};
    if(size>56 || address>=0x80000 || address+size>0x80000)return p;
    p[0]=0x55;p[1]=0xe0;p[4]=size;
    p[5]=static_cast<std::uint8_t>(address);p[6]=static_cast<std::uint8_t>(address>>8);p[7]=static_cast<std::uint8_t>(address>>16);
    p[3]=Checksum(p);return p;
}
inline bool Reply(const Report& request,const Report& reply) noexcept {
    return reply[0]==0xaa && reply[1]==request[1] && reply[2]==0 &&
        reply[4]<=56 && reply[4]==request[4] &&
        std::equal(reply.begin()+5,reply.begin()+8,request.begin()+5) && reply[3]==Checksum(reply);
}
inline bool Profile(const Base& b,unsigned& profile) noexcept {
    if(!b[1] || b[1]>4 || b[0]>=b[1] || b[2+b[0]]>=4)return false;
    profile=b[2+b[0]];return true;
}
inline Report FlagWrite(unsigned profile,std::uint8_t previous,bool enabled) noexcept {
    Report p{};if(profile>=4)return p;
    p[0]=0x55;p[1]=6;p[4]=1;p[5]=static_cast<std::uint8_t>(profile*64+7);
    p[8]=enabled?static_cast<std::uint8_t>(previous|8):static_cast<std::uint8_t>(previous&~8u);
    p[3]=Checksum(p);return p;
}
// Immutable factory descriptor, NOT the user's remapped key assignment.
inline unsigned Hid(std::uint8_t type,std::uint8_t modifier,std::uint8_t code) noexcept {
    if(type!=0x10)return 0;
    if(!modifier)return code>=4 && code<=0x73?code:0;
    if(code || (modifier&(modifier-1)))return 0;
    unsigned bit=0;while((modifier>>bit)!=1)++bit;
    return 0xe0+bit;
}
struct Sample {unsigned hid=0;std::uint16_t milli=0;};
inline bool Decode(const Report& p,const std::array<bool,256>& allowed,Sample& s) noexcept {
    if(p[0]!=0xa0)return false;
    const auto hid=Hid(p[1],p[2],p[3]);
    const unsigned depth=(p[6]<<8)|p[7], maximum=(p[14]<<8)|p[15];
    if(!hid || !allowed[hid] || maximum!=341 || depth>maximum)return false;
    s={hid,static_cast<std::uint16_t>((depth*1000u+maximum/2)/maximum)};return true;
}
enum class ChangeResult { Verified, Unchanged, StaleProfile, ReadFailed, ReservedData, Uncertain };
// One guarded flag transaction; lifecycle policy is owned by the caller.
// No retry or calibration inside this transaction.
// read(address,dst,len) and exchange(request,reply) must use the SAME open handle.
template<class ReadBytes,class Exchange>
ChangeResult ChangeFlag(const Base& consentBase,bool enabled,ReadBytes read,Exchange exchange) {
    Base base{};SettingsPage before{},after{};unsigned profile=0;
    if(!read(BaseAddress,base.data(),base.size()) || !read(SettingsAddress,before.data(),before.size()))return ChangeResult::ReadFailed;
    if(base!=consentBase || !Profile(base,profile))return ChangeResult::StaleProfile;
    // Firmware aligns the erase address to 8 KiB and rewrites 256 settings bytes.
    // Reserved tail is zero padding in the pinned factory image, or erased FF
    // after a normal settings save. Reject any other contents, not these two
    // known states. Erasing factory padding is normal firmware behavior.
    const bool erased=std::all_of(before.begin()+256,before.end(),[](auto v){return v==0xff;});
    const bool factoryPadding=std::all_of(before.begin()+256,before.end(),[](auto v){return v==0;});
    if(!erased && !factoryPadding)return ChangeResult::ReservedData;
    const auto offset=profile*64+7;
    if(bool(before[offset]&8)==enabled)return ChangeResult::Unchanged;
    Base check{};
    if(!read(BaseAddress,check.data(),check.size()))return ChangeResult::ReadFailed;
    if(check!=base)return ChangeResult::StaleProfile;
    const auto request=FlagWrite(profile,before[offset],enabled);Report ack{};
    const bool accepted=exchange(request,ack); // exactly ONE write attempt
    if(!read(SettingsAddress,after.data(),after.size()) || !read(BaseAddress,check.data(),check.size()))return ChangeResult::Uncertain;
    before[offset]=request[8];
    const bool settingsMatch=std::equal(before.begin(),before.begin()+256,after.begin());
    bool paddingSafe=true;
    for(std::size_t i=256;i<after.size();++i)
        if(after[i]!=before[i] && after[i]!=0xff)paddingSafe=false;
    return accepted && settingsMatch && paddingSafe && check==base?ChangeResult::Verified:ChangeResult::Uncertain;
}
}
