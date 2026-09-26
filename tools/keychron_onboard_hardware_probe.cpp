// Explicit hardware probe for the production channel/client. Never in CI.
#include "keychron_onboard_channel.h"
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>
#include <vector>
using namespace halljoy::k4_onboard;
struct ProbeChannel : Channel {
    WindowsChannel inner;
    bool legacy=false;
    explicit ProbeChannel(const Device& d):inner(d){}
    bool Connect(){return inner.Connect();}
    bool Read(Packet& p) override {return inner.Read(p);}
    bool Reconnect(uint16_t r) override {return inner.Reconnect(r);}
    uint64_t NowMs() const override {return inner.NowMs();}
    bool Cancelled() const override {return inner.Cancelled();}
    bool Exchange(const Packet& q,Packet& r) override {
        if(!inner.Exchange(q,r))return false;
        if(legacy && q[1]==0x70)r[17]&=static_cast<uint8_t>(~16u);
        return true;
    }
};
int main(int argc,char** argv) {
    if((argc!=2 && argc!=3) || (std::string(argv[1])!="status" && std::string(argv[1])!="cycle")) return 2;
    const auto devices=EnumerateDevices();
    std::cout<<"devices="<<devices.size()<<std::endl;
    if(devices.size()!=1) return 3;
    ProbeChannel channel(devices[0]);channel.legacy=argc==3 && std::string(argv[2])=="legacy"; if(!channel.Connect()) return 4;
    Client client(channel); Packet reply{};
    if(!client.Status(reply)) return 5;
    std::cout<<"phase="<<unsigned(reply[3])<<" native="<<unsigned(reply[16])
        <<" capabilities="<<unsigned(reply[17])<<" scan_us="<<hjk4_u32(reply.data()+24)<<std::endl;
    if(std::string(argv[1])=="status") return 0;
    hjo_profile profile{}; std::memset(profile.mapping.axes,255,8); std::memset(profile.mapping.triggers,255,2);
    profile.mapping.sensitivity=.02f;
    for(auto& curve:profile.curves) curve={{0,.3f,.7f,1},{0,.3f,.7f,1},{1,1},1,0};
    if(!client.Open(profile)) {std::cout<<"open_failed close="<<client.Close()<<std::endl;return 6;}
    std::cout<<"active_neutral=1"<<std::endl;
    // Exercise a changed profile while the active 500ms firmware lease runs.
    // No source is bound, so both profiles always send neutral gamepad input.
    profile.mapping.flags=HJO_SNAP;
    if(!client.Update(profile)) {client.Close();return 7;}
    std::array<uint16_t,HJO_SLOTS> depth{};
    std::vector<double> intervals;
    if(!client.Status(reply)){client.Close();return 11;}
    const auto firstScan=hjk4_u32(reply.data()+20);
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<1200;++i) {
        const auto before=std::chrono::steady_clock::now();
        if(!client.Depth(depth)) {client.Close();return 8;}
        intervals.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count());
    }
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::sort(intervals.begin(),intervals.end());
    std::cout<<"depth_hz="<<1200/elapsed<<" median_ms="<<intervals[600]<<" p95_ms="<<intervals[1140]<<std::endl;
    if(!client.Status(reply)){client.Close();return 12;}
    std::cout<<"scan_hz_during_preview="<<(hjk4_u32(reply.data()+20)-firstScan)/elapsed
        <<" last_scan_us="<<hjk4_u32(reply.data()+24)<<std::endl;
    for(unsigned i=0;i<20;++i) {Sleep(50);if(!client.KeepAlive()){client.Close();return 9;}}
    const bool closed=client.Close();
    std::cout<<"updated_neutral=1 depth_valid=1 close="<<closed<<std::endl;
    return closed?0:10;
}
