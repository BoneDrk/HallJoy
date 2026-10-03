#include "sparklink_model_profiles.h"
#include "sparklink_key_codes.h"
#include "support_notice_catalog.h"
#include "keyboard_support_status.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace halljoy;
 assert(!sparklink::ProbeIdentity(0x1038,0x1610));
 assert(!sparklink::ProbeInterface(0x1038,0x1610,0xffc0,1));
 assert(!sparklink::ProbeInterface(0x1038,0x1610,0xffb0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x529,0xffc0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x529,0xffb0,2));
 assert(!sparklink::ProbeIdentity(0x1ca6,0xffff));
 assert(sparklink::ProbeInterface(0x1ca6,0x529,0xffb0,1));
 for(unsigned pid=0;pid<65536;++pid)if(sparklink::ExperimentalToken(0x1ca6,pid,0xffb0))assert(sparklink::ProbeInterface(0x1ca6,pid,0xffb0,1));

 for(unsigned pid:{0x528u,0x52au,0x52bu,0x52du,0x52cu,0x531u,0x540u}) {
  auto token=sparklink::ExperimentalToken(0x1ca6,pid,0xffb0);
  assert(token && keyboard_support::NativeNotice(4,token,true,pid)==keyboard_support::SparkLinkV2);
  assert(!keyboard_support::NativeNotice(4,token,false,pid));
  assert(!sparklink::ExperimentalToken(0x1ca5,pid,0xffb0));
  assert(!sparklink::ExperimentalToken(0x1ca6,pid,0xffa0));
 }
 assert(!sparklink::ExperimentalToken(0x1ca6,0x529,0xffb0)); // confirmed MG75 Max
 assert(!sparklink::ExperimentalToken(0x1ca6,0x3002,0xffb0)); // Supported SU75 Pro
 assert(sparklink::ProbeInterface(0x1ca6,0x3002,0xffb0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x3002,0xffa0,1));
 assert(!keyboard_support::NativeNotice(4,0,true));
 assert(sparklink::DecodeKey(0xf101)==0x409 && sparklink::DecodeKey(0xf102)==0);
 for(unsigned key=4;key<256;++key)assert(sparklink::DecodeKey(key)==key);
 std::cout<<"SPARKLINK_MODEL_PROFILES=PASS exact identities notices confirmed-model exclusion Fn\n";
}
