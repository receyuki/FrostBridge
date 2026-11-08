#pragma once
#ifdef FROSTBRIDGE_M1
#include "AlpicoolProtocol.h"
#ifdef FROSTBRIDGE_GATEWAY
#include "ControlRuntime.h"
#include "Portal.h"
#endif
class NimBLEAdvertisedDevice;
namespace m1 {
struct Snapshot {
  alpicool::Status status;
  uint32_t observedMs=0, ageMs=0, epoch=0, sequence=0;
  bool stale=true;
};
// False means no complete valid status has ever been received.
bool readSnapshot(Snapshot& snapshot);
void begin();
void observe(const NimBLEAdvertisedDevice* device);
void listDevices();
bool selectTarget(unsigned index);
void disconnectTarget();
bool connected();
bool scanAllowed();
#ifdef FROSTBRIDGE_GATEWAY
struct GatewayStatus {
  bool backupEnabled=false,manual=false,timeTrusted=false,writesEnabled=false;
  uint8_t sampleCount=0,storeStatus=0;
  frost::ReplayMode decision=frost::ReplayMode::Disabled;
  frost::Error autoError=frost::Error::None;
  frost::Error storageError=frost::Error::None;
  uint32_t workerStackLowWater=0;
  uint32_t generation=0;
  int64_t nextBoundary=0;
};
bool readGatewayStatus(GatewayStatus& status);
bool postControl(alpicool::Setting setting,int value);
bool postClock(int64_t unixSeconds);
bool postGatewayDiagnostic(bool verifyStore=false);
void noteNetworkTime(int64_t unixSeconds,uint16_t fractionMs=0);
frost::ApiBackend portalBackend();
#endif
}
#endif
