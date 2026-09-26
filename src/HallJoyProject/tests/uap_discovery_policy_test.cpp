#include "halljoy_uap_discovery_policy.h"
#include <cassert>
using namespace halljoy::uap_discovery;
int main(){
 assert(!MayUsePath(L"hid#vid_1038&pid_1610&mi_01#instance"));
 assert(!MayUsePath(L"HID#VID_1038&PID_12B3&MI_03#instance"));
 assert(!MayUsePath(L"HID#VID_045E&PID_028E#virtual-gamepad"));
 for(auto p:{L"hid#vid_31E3&pid_1230",L"hid#vid_03EB&pid_FF01",L"hid#vid_1532&pid_0266",L"hid#vid_352D&pid_2382",L"hid#vid_3434&pid_0e40",L"hid#vid_362D&pid_0610",L"hid#vid_19F5&pid_6130",L"hid#vid_373B&pid_1055"})assert(MayUsePath(p));
 assert(MayUsePath(L"hid#unusual"));assert(MayUsePath(L"hid#vid_103G&pid_1610"));assert(MayUsePath(L"hid#vid_10380&pid_1610"));
}
