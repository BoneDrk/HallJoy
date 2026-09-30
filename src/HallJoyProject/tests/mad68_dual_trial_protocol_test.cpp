#include "mad68_dual_trial_protocol.h"
#include "keyboard_support_status.h"
#include <cassert>
#include <cstdio>
using namespace halljoy::mad68_dual_trial;
int main() {
    using namespace halljoy::keyboard_support;
    assert(ClassifyFrozen(0x28e9,0x3265,L"")==Mad68DualLimited);
    assert(!(ImplementedModels & Mad68DualLimited));
    assert(ClassifyFrozen(0x28e9,0x31fd,L"")==0);
    auto on=Command(0x36,0,1,1), off=Command(0x36,0,1,0);
    assert(on[0]==6 && on[5]==0x38 && on[8]==1);
    assert(off[5]==0x37 && off[8]==0);
    auto map=Command(0x16,280,56); assert(map[2]==24 && map[3]==1 && map[5]==103);
    std::uint8_t key=0; std::uint16_t raw=0; Decoder d;
    // Actual firmware replay: low-first 1500, small step1510, then release.
    const std::uint8_t a[]{7,0,0x5c}, b[]{7,0,0x57}, zero[]{7,0,0x40};
    assert(!d.Push(a,3,10,key,raw)); assert(d.Push(b,3,11,key,raw));
    assert(key==0 && raw==1500 && Milli(raw)==462);
    assert(!d.Push(zero,3,12,key,raw)); assert(d.Push(zero,3,13,key,raw) && raw==0);
    // Different keys, malformed packets and long gaps must not complete stale fragments.
    const std::uint8_t other[]{7,1,0x5c}, invalid[]{7,0,0xc0}, excess[]{7,0,0x7f};
    assert(!d.Push(a,3,20,key,raw)); assert(!d.Push(other,3,21,key,raw));
    assert(!d.Push(b,3,22,key,raw)); assert(!d.Push(invalid,3,23,key,raw));
    assert(!d.Push(a,2,24,key,raw)); assert(!d.Push(a,3,25,key,raw));
    assert(!d.Push(b,3,126,key,raw)); d={};
    assert(!d.Push(excess,3,130,key,raw)); assert(!d.Push(excess,3,131,key,raw));
    // Exhaust every valid raw depth; no extra deadzone, filter or range truncation.
    for(unsigned v=0;v<=Maximum;++v) {
        Decoder pair; std::uint8_t lo[]{7,17,static_cast<std::uint8_t>(0x40|(v&63))};
        std::uint8_t hi[]{7,17,static_cast<std::uint8_t>(0x40|(v>>6))};
        assert(!pair.Push(lo,3,1,key,raw)); assert(pair.Push(hi,3,2,key,raw));
        assert(raw==v && key==17 && Milli(raw)<=1000);
        if(v) assert(Milli(raw)>=Milli(raw-1));
    }
    assert(Milli(0)==0 && Milli(Maximum)==1000);
    puts("MAD68 Dual trial protocol PASS: firmware packets, release, framing, 3251 values");
}
