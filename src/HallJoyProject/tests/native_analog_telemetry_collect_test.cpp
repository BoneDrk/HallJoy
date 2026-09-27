#define HALLJOY_ATTACKSHARK_NATIVE
#define HALLJOY_AULA_MINI60_NATIVE
#define HALLJOY_KEYCHRON_ONBOARD_EXPERIMENTAL
#include "native_analog_telemetry_collect.h"
#include <stdexcept>
#include <iostream>
void Check(bool v){if(!v)throw std::runtime_error("telemetry contract regression");}
int main(){
 static_assert(kBackendMaxNativeProtocols>16);
 static_assert(kBackendMaxNativeProtocols==kNativeAnalogBackendMaxCount);
 NativeAnalogBackendDescriptor d{};d.id="fixture";d.displayName=L"fixture";
 int reads=0,lifecycleReads=0;bool unavailable=false;
 auto descriptor=[&](std::size_t){return &d;};
 auto telemetry=[&](std::size_t i,NativeAnalogBackendTelemetry* n){
   ++reads; if(unavailable && i==0)return false;
   n->present=n->connected=i==kNativeAnalogCatalogSize-1;
   if(n->connected){n->successfulUpdates=42;n->lastUpdateAgeMs=60000;}
   return true;
 };
 auto lifecycle=[&](std::size_t i,NativeAnalogBackendLifecycleSnapshot* s){
   ++lifecycleReads;s->state=halljoy::lifecycle::WorkerState::Running;return i!=0;
 };
 BackendAnalogTelemetry t{};
 CollectNativeAnalogTelemetry(t,true,kNativeAnalogCatalogSize,descriptor,telemetry,lifecycle);
 Check(t.nativeTelemetryComplete && t.nativeProtocolCount==kBackendMaxNativeProtocols);
 Check(reads==kBackendMaxNativeProtocols && lifecycleReads==reads);
 Check(t.nativeConnectedCount==1 && t.nativeProtocols[kBackendMaxNativeProtocols-1].connected);
 Check(!t.nativeProtocols[0].lifecycleAvailable);
 // Idle event stream remains connected; no age-based invented timeout.
 Check(t.nativeProtocols[kBackendMaxNativeProtocols-1].lastUpdateAgeMs==60000);
 reads=lifecycleReads=0;t={};
 CollectNativeAnalogTelemetry(t,false,kNativeAnalogCatalogSize,descriptor,telemetry,lifecycle);
 Check(t.nativeProtocolCount==1 && t.nativeConnectedCount==1 && lifecycleReads==0);
 unavailable=true;t={};
 CollectNativeAnalogTelemetry(t,true,kNativeAnalogCatalogSize,descriptor,telemetry,lifecycle);
 Check(!t.nativeTelemetryComplete && t.nativeTelemetryFailures==1 && !t.nativeProtocols[0].telemetryAvailable);
 t={};
 CollectNativeAnalogTelemetry(t,true,kNativeAnalogCatalogSize+1,descriptor,telemetry,lifecycle);
 Check(!t.nativeTelemetryComplete && t.nativeVisitedCount<t.nativeCatalogCount);
 std::cout<<"NATIVE_TELEMETRY_CONTRACT=PASS tail_protocol absent unavailable capacity lifecycle idle_stream\n";
}
