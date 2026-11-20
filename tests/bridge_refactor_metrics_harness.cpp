#include <QtGlobal>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
class DecodiumBridge { public:
static void noteDecodeReadySlotStart(); static void noteDecodeReadySlotEnd();
static void noteDecodeModelEmitStart(); static void noteDecodeModelEmitEnd();
static qint64 msSinceLastDecodeReadySlotStart(); static qint64 msSinceLastDecodeReadySlotEnd();
static qint64 msSinceLastDecodeModelEmitStart(); static qint64 msSinceLastDecodeModelEmitEnd(); };
static qint64 bridgeMonotonicMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
#include "bridge_refactor_under_test.inc"

int main() {
using F=qint64(*)(); using N=void(*)();
F reads[]={DecodiumBridge::msSinceLastDecodeReadySlotStart,DecodiumBridge::msSinceLastDecodeReadySlotEnd,DecodiumBridge::msSinceLastDecodeModelEmitStart,DecodiumBridge::msSinceLastDecodeModelEmitEnd};
N writes[]={DecodiumBridge::noteDecodeReadySlotStart,DecodiumBridge::noteDecodeReadySlotEnd,DecodiumBridge::noteDecodeModelEmitStart,DecodiumBridge::noteDecodeModelEmitEnd};
for(auto f:reads) if(f()!=-1) return 1; puts("unmarked:4");
for(int i=0;i<4;i++){ writes[i](); for(int j=0;j<4;j++){ auto v=reads[j](); if(j<=i ? v<0 || v>1000 : v!=-1) return 2; } } puts("independent:4");
std::this_thread::sleep_for(std::chrono::milliseconds(20)); for(auto f:reads) if(f()<15 || f()>1000) return 3; puts("elapsed:4");
writes[1](); if(reads[1]()>10 || reads[0]()<15) return 4; puts("reset:1"); return 0; }
