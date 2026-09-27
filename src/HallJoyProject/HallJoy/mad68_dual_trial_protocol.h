#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace halljoy::mad68_dual_trial {
constexpr std::uint16_t Vendor = 0x28e9, Product = 0x3265, Maximum = 3250;
using Control = std::array<std::uint8_t, 64>;
inline Control Command(std::uint8_t opcode, std::uint16_t offset, std::uint8_t length, std::uint8_t value = 0) {
    Control r{}; r[0]=6; r[1]=opcode; r[2]=static_cast<std::uint8_t>(offset);
    r[3]=static_cast<std::uint8_t>(offset>>8); r[4]=length; r[8]=value;
    const unsigned sum=r[1]+r[2]+r[3]+r[4]+r[8];
    r[5]=static_cast<std::uint8_t>(sum); r[6]=static_cast<std::uint8_t>(sum>>8); return r;
}
inline std::uint16_t Milli(std::uint16_t raw) {
    return static_cast<std::uint16_t>((static_cast<unsigned>(raw)*1000+Maximum/2)/Maximum);
}
// Firmware sends low six bits then high six bits, with no sequence bit.
// Only contiguous same-key fragments are paired; losses remain a firmware limit.
struct Decoder {
    bool pending=false; std::uint8_t key=0, low=0; std::uint64_t at=0;
    bool Push(const std::uint8_t* b, std::size_t n, std::uint64_t now,
              std::uint8_t& outKey, std::uint16_t& raw) {
        if(n!=3 || b[0]!=7 || (b[2]&0xc0)!=0x40) { pending=false; return false; }
        if(!pending || key!=b[1] || now<at || now-at>100) {
            pending=true; key=b[1]; low=b[2]&63; at=now; return false;
        }
        pending=false; outKey=key; raw=static_cast<std::uint16_t>(low | ((b[2]&63)<<6));
        return raw<=Maximum;
    }
};
}
