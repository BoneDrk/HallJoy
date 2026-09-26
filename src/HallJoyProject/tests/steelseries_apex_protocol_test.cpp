#include "steelseries_apex_protocol.h"
#include <cassert>
#include "support_notice_catalog.h"
#include <cstring>
using namespace halljoy::apex;
static void put(Report& p,unsigned off,unsigned v){p[off]=v&255;p[off+1]=v>>8;}
int main(){
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1610)==0);
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1614)==1048576);
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1640)==1048576);
 assert(halljoy::keyboard_support::NativeNotice(25,0,false,0x1640)==0);

 for(unsigned pid:{0x1610u,0x1614u,0x1640u})assert(SupportedIdentity(0x1038,pid));
 for(unsigned pid:{0x1612u,0x1618u,0x161eu,0x1628u,0x1642u,0x1648u})assert(!SupportedIdentity(0x1038,pid));
 assert(!SupportedIdentity(0x1ca6,0x1614));
 assert(SupportedCollection(0xffc0,1,65,65,643));
 assert(!SupportedCollection(0xffb0,1,65,65,643));
 assert(!SupportedCollection(0xffc0,1,65,65,65));

 assert(ReadOnlyRequest(VersionRequest()));Report version{};std::memcpy(version.data()+1,"4.16.8",7);assert(KnownVersion(version));version[6]='9';assert(!KnownVersion(version));
 for(unsigned bank=1;bank<=5;++bank){
  assert(ReadOnlyRequest(DepthRequest(bank)));Report p{};Matrix m{};
  for(unsigned i=0;i<14;++i){put(p,1+2*i,1100+(bank-1)*14+i);put(p,29+2*i,1200+(bank-1)*14+i);}
  assert(ParseDepth(p,bank,m));for(unsigned i=0;i<14;++i)assert(m[(bank-1)*14+i]==1200+(bank-1)*14+i);
  const auto previous=m;p[64]=1;assert(!ParseDepth(p,bank,m));assert(m==previous);p[64]=0;put(p,1,4096);assert(!ParseDepth(p,bank,m));assert(m==previous);
 }
 assert(!ReadOnlyRequest(DepthRequest(0)));assert(!ReadOnlyRequest(DepthRequest(6)));
 Ranges ranges{};
 for(unsigned start=0;start<68;start+=12){
  const auto count=std::min(12u,68-start);auto req=RangeRequest(start,count);assert(ReadOnlyRequest(req));
  auto unsafe=req;unsafe[1]&=0x7f;assert(!ReadOnlyRequest(unsafe));
  Report p{};for(unsigned i=0;i<count;++i){p[1+i*5]=static_cast<unsigned char>(kMap[start+i]);put(p,2+i*5,3000+start+i);put(p,4+i*5,1000+start+i);}
  assert(ParseRanges(p,start,count,ranges));for(unsigned i=0;i<count;++i){assert(ranges[start+i].high==3000+start+i);assert(ranges[start+i].low==1000+start+i);}
  p[1]^=1;assert(!ParseRanges(p,start,count,ranges));
 }
 assert(!ReadOnlyRequest(RangeRequest(0,13)));assert(!ReadOnlyRequest(RangeRequest(67,2)));
 assert(Normalize(1000,{1000,3000})==0);assert(Normalize(2000,{1000,3000})==500);assert(Normalize(4000,{1000,3000})==1000);assert(Normalize(2000,{3000,1000})==0);
 unsigned previous=0;for(unsigned raw=0;raw<=4095;++raw){unsigned v=Normalize(raw,{1000,3000});assert(v>=previous && v<=1000);previous=v;}
 assert(kMap[16]==26 && kMap[29]==4 && kMap[30]==22 && kMap[31]==7);
}
