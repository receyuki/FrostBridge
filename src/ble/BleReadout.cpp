#ifdef FROSTBRIDGE_M1
#include "BleReadout.h"
#include "AlpicoolProtocol.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#ifdef FROSTBRIDGE_GATEWAY
#include "ControlRuntime.h"
#include "NvsDailyStore.h"
#endif

namespace m1 {
namespace {
struct Candidate {
  char address[18]{};
  char name[40]{};
  uint8_t type=0;
  int16_t rssi=0;
  bool service=false;
};
constexpr unsigned capacity=48;
Candidate candidates[capacity];
unsigned count=0;
portMUX_TYPE candidateMux=portMUX_INITIALIZER_UNLOCKED;
struct Command { bool stop; Candidate target; };
struct Packet { uint32_t epoch;alpicool::Receipt receipt;uint16_t size;uint8_t bytes[258]; };
QueueHandle_t commands=nullptr, packets=nullptr;
std::atomic<bool> selected{false}, link{false};
std::atomic<bool> statusReady{false};
std::atomic<uint32_t> connectionEpoch{0};
Snapshot observed;
portMUX_TYPE observedMux=portMUX_INITIALIZER_UNLOCKED;
std::atomic<uint32_t> dropped{0};
std::atomic<uint32_t> receiveSequence{0};
std::atomic<int> disconnectReason{-1};
#ifdef FROSTBRIDGE_GATEWAY
enum class RequestKind { Manual,Clock,Status,VerifyStore };
struct RuntimeRequest {
  RequestKind kind=RequestKind::Status;
  alpicool::Setting setting=alpicool::Setting::TargetC;
  int value=0;int64_t unixSeconds=0;
};
QueueHandle_t runtimeRequests=nullptr;
GatewayStatus gatewayStatus;
bool gatewayPublished=false;
portMUX_TYPE gatewayMux=portMUX_INITIALIZER_UNLOCKED;
frost::NetworkTimeSample networkTime;
portMUX_TYPE networkTimeMux=portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE portalMux=portMUX_INITIALIZER_UNLOCKED;
void enterPortal(void*) { portENTER_CRITICAL(&portalMux); }
void leavePortal(void*) { portEXIT_CRITICAL(&portalMux); }
frost::PortalMailbox portalMail({nullptr,enterPortal,leavePortal},[]{return millis();});
#endif

void printHex(const char* kind, const uint8_t* data, size_t length) {
  char text[258*2+1];
  for (size_t i=0;i<length;++i) snprintf(text+2*i,3,"%02X",data[i]);
  text[2*length]=0;
  Serial.printf("M1_%s ms=%lu hex=%s\n",kind,static_cast<unsigned long>(millis()),text);
}
class ClientCallbacks final : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int reason) override {
    link=false;
    disconnectReason=reason;
  }
};
ClientCallbacks callbacks;
std::string fullUuid(NimBLEUUID uuid) { return uuid.to128().toString(); }

void worker(void*) {
  NimBLEClient* client=nullptr;
  NimBLERemoteCharacteristic* write=nullptr;
  Candidate target{};
  uint32_t epoch=0,nextAttempt=0,lastQuery=0,lastPacket=0;
  unsigned retry=0;
  bool active=false, ready=false;
  bool wasFresh=false;
  alpicool::Decoder decoder;
#ifdef FROSTBRIDGE_GATEWAY
  frost::NvsDailyStore store;
  frost::DailyReplay daily([&](const frost::DailyPersistedState& state){return store.save(state);},millis());
  static_assert(sizeof(frost::DailyReplay)+sizeof(frost::NvsDailyStore)<=2304,"Daily owner/store worker stack budget");
  const auto loaded=store.load(daily);
  const uint32_t boot=esp_random();portalMail.reset(boot);
  // Normal control uses Controller validation; unsupported layouts remain rejected.
  frost::ControlRuntime core(boot,[&](const alpicool::Frame& frame) {
    if(!ready || !link.load() || !write) return false;
    const bool query=frame.size()>3 && frame[3]==1;
    printHex(query?"TX_CONFIRM_QUERY":"TX_SETTING",frame.data(),frame.size());
    const bool sent=write->writeValue(frame.data(),frame.size(),write->canWrite());
    lastQuery=query?millis():millis()-10000;return sent;
  },daily,true);
  frost::PortalWorker portalWorker(core,portalMail,[]{return millis();});
  std::optional<frost::CommandId> usbCommand;std::optional<frost::CommandState> usbPrinted;
  uint32_t stackSampleMs=0,workerStackLowWater=0;
  auto publish=[&] {
    const auto now=millis();const auto decision=core.decision(now);const auto wall=core.unixTime(now);const auto& state=daily.state();
    if(!workerStackLowWater || now-stackSampleMs>=5000){workerStackLowWater=uxTaskGetStackHighWaterMark(nullptr);stackSampleMs=now;}
    GatewayStatus snapshot;snapshot.workerStackLowWater=workerStackLowWater;
    snapshot.writesEnabled=core.writesEnabled();snapshot.backupEnabled=state.enabled;snapshot.manual=state.manual && (!state.overrideUntilMs || !wall || *wall*1000<*state.overrideUntilMs);snapshot.timeTrusted=core.timeTrusted();
    snapshot.sampleCount=state.sample.count;snapshot.storeStatus=static_cast<uint8_t>(store.status());
    snapshot.decision=decision.mode;snapshot.autoError=core.lastAutoError();snapshot.storageError=daily.storageHealthy() && store.status()!=frost::DailyStoreStatus::Invalid && store.status()!=frost::DailyStoreStatus::Unavailable?frost::Error::None:frost::Error::PersistenceFailed;
    snapshot.generation=state.generation;snapshot.nextBoundary=decision.nextActionMs.value_or(0);
    portENTER_CRITICAL(&gatewayMux);gatewayStatus=snapshot;gatewayPublished=true;portEXIT_CRITICAL(&gatewayMux);
    const auto& observed=core.observed();frost::WebSnapshot web;
    web.boot=boot;web.capturedMs=now;web.ready=true;web.connected=link.load() && statusReady.load();web.observed=!observed.frame.empty();web.valid=observed.valid;
    web.status=observed.status;web.observedMs=observed.receivedMs;web.epoch=observed.epoch;web.sequence=observed.sequence;
    web.writes=core.writesEnabled();web.timeTrusted=core.timeTrusted();web.wall=wall.value_or(0);
    web.generation=state.generation;web.manual=snapshot.manual;web.overrideUntilMs=state.overrideUntilMs.value_or(0);web.decision=decision;
    web.automaticError=core.lastAutoError();web.storageError=snapshot.storageError;web.storeStatus=static_cast<uint8_t>(store.status());
    const int current=static_cast<int>(observed.status.targetC.value_or(0));const int high=std::min(20,static_cast<int>(observed.status.maxC.value_or(20)));const int low=std::max(-20,static_cast<int>(observed.status.minC.value_or(-20)));
    web.controls[0]=core.checkControl({alpicool::Setting::TargetC,current<high?current+1:current>low?current-1:current},web.capturedMs);
    web.controls[1]=core.checkControl({alpicool::Setting::Power,observed.status.power.value_or(false)?0:1},web.capturedMs);
    web.controls[2]=core.checkControl({alpicool::Setting::Mode,observed.status.mode==alpicool::Mode::Eco?0:1},web.capturedMs);
    const auto commands=core.commands();for(auto it=commands.rbegin();it!=commands.rend() && web.automaticCount<4;++it) if(it->intent.source==frost::Source::LocalReplay)web.automatic[web.automaticCount++]=*it;
    portalMail.publish(web,daily);
  };
  Serial.printf("M2_START writes_enabled=1 time_trusted=0 store_status=%u\n",static_cast<unsigned>(loaded));publish();
  Preferences binding;
  if(binding.begin("fb-c15",true)) {
    const auto address=binding.getString("address");const auto type=binding.getUChar("type",255);
    bool valid=address.length()==17 && type<=3;
    for(unsigned i=0;i<address.length() && valid;++i) {
      const char c=address[i];valid=i%3==2?c==':':((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'));
    }
    if(valid) { snprintf(target.address,sizeof(target.address),"%s",address.c_str());target.type=type;active=true;selected=true;Serial.println("M2_BOUND_TARGET_LOADED"); }
    binding.end();
  }
#endif
  for (;;) {
    Command command{};
    if (xQueueReceive(commands,&command,0)==pdTRUE) {
      if (client && client->isConnected()) client->disconnect();
      active=!command.stop;
      target=command.target;
      ready=false; link=false; write=nullptr;
      statusReady=false;
#ifdef FROSTBRIDGE_GATEWAY
      core.connection(false,epoch,millis());
#endif
      decoder.reset(); xQueueReset(packets);
      nextAttempt=millis(); retry=0;
      selected=active;
    }
    const int reason=disconnectReason.exchange(-1);
    if (reason>=0) Serial.printf("M1_DISCONNECTED reason=%d\n",reason);
    if (ready && !link.load()) {
      ready=false; decoder.reset(); xQueueReset(packets); nextAttempt=millis()+2000;
      statusReady=false;
#ifdef FROSTBRIDGE_GATEWAY
      core.connection(false,epoch,millis());
#endif
    }
    const uint32_t now=millis();
    if (active && !link.load() && static_cast<int32_t>(now-nextAttempt)>=0) {
      NimBLEDevice::getScan()->stop();
      if (!client) {
        client=NimBLEDevice::createClient();
        if (client) { client->setClientCallbacks(&callbacks,false); client->setConnectTimeout(5000); }
      }
      decoder.reset(); xQueueReset(packets); ready=false; write=nullptr;
      statusReady=false;
      Serial.println("M1_CONNECTING");
      if (!client || !client->connect(NimBLEAddress(std::string(target.address),target.type))) {
        constexpr uint32_t delays[]{2000,5000,10000,30000,60000};
        nextAttempt=millis()+delays[retry<4?retry++:4];
        Serial.printf("M1_CONNECT_FAILED retry_in_ms=%lu\n",static_cast<unsigned long>(nextAttempt-millis()));
      } else {
        link=true;
        for (auto* service:client->getServices(true)) {
          Serial.printf("M1_SERVICE uuid=%s\n",fullUuid(service->getUUID()).c_str());
          for (auto* characteristic:service->getCharacteristics(true)) {
            Serial.printf("M1_CHARACTERISTIC service=%s uuid=%s read=%d write=%d write_no_response=%d notify=%d indicate=%d\n",
              fullUuid(service->getUUID()).c_str(),fullUuid(characteristic->getUUID()).c_str(),
              characteristic->canRead(),characteristic->canWrite(),characteristic->canWriteNoResponse(),
              characteristic->canNotify(),characteristic->canIndicate());
            for (auto* descriptor:characteristic->getDescriptors(true))
              Serial.printf("M1_DESCRIPTOR characteristic=%s uuid=%s\n",fullUuid(characteristic->getUUID()).c_str(),fullUuid(descriptor->getUUID()).c_str());
          }
        }
        auto* service=client->getService(NimBLEUUID(uint16_t(0x1234)));
        write=service?service->getCharacteristic(NimBLEUUID(uint16_t(0x1235))):nullptr;
        auto* notify=service?service->getCharacteristic(NimBLEUUID(uint16_t(0x1236))):nullptr;
        if (!write || (!write->canWrite() && !write->canWriteNoResponse()) || !notify || (!notify->canNotify() && !notify->canIndicate())) {
          Serial.println("M1_PROTOCOL_MISMATCH no_query_sent");
          active=false; selected=false; client->disconnect(); link=false;
        } else {
          const uint32_t session=++epoch;
          connectionEpoch=session;
          const bool subscribed=notify->subscribe(notify->canNotify(),[session](NimBLERemoteCharacteristic*,uint8_t* bytes,size_t length,bool) {
            if (length>258) { ++dropped; return; }
            Packet p{};p.epoch=session;p.receipt={receiveSequence.fetch_add(length)+1,millis()};p.size=length;memcpy(p.bytes,bytes,length);
            if (xQueueSend(packets,&p,0)!=pdTRUE) ++dropped;
          },true);
          if (!subscribed) {
            Serial.println("M1_SUBSCRIBE_FAILED no_query_sent");
            client->disconnect();link=false;nextAttempt=millis()+5000;
          } else {
            ready=true;retry=0;lastQuery=millis()-10000;
            statusReady=true;
#ifdef FROSTBRIDGE_GATEWAY
            core.connection(true,epoch,millis());
#endif
            Preferences binding;
            bool bindingSaved=binding.begin("fb-c15",false);
            if (bindingSaved) {
              if (binding.getString("address")!=target.address) bindingSaved=binding.putString("address",target.address)==strlen(target.address);
              if (binding.getUChar("type",255)!=target.type) bindingSaved=(binding.putUChar("type",target.type)==1) && bindingSaved;
              binding.end();
            }
            Serial.println(bindingSaved?"FROST_BIND_SAVED":"FROST_BIND_SAVE_FAILED");
    #ifdef FROSTBRIDGE_GATEWAY
        Serial.println("M1_TARGET_READY controller_ready=1");
#else
        Serial.println("M1_TARGET_READY query_only=1");
#endif
          }
        }
      }
    }
    if (active && ready && link.load()) {
      if (static_cast<uint32_t>(millis()-lastQuery)>=10000) {
        const auto query=alpicool::query();
        printHex("TX",query.data(),query.size());
        if (!write->writeValue(query.data(),query.size(),write->canWrite())) Serial.println("M1_QUERY_WRITE_FAILED");
        lastQuery=millis();
      }
      Packet packet{};
      while (xQueueReceive(packets,&packet,0)==pdTRUE) {
        if (packet.epoch!=epoch || !link.load()) continue;
        if (packet.receipt.receivedMs-lastPacket>2000) decoder.reset();
        lastPacket=packet.receipt.receivedMs;
        printHex("RX",packet.bytes,packet.size);
        for (const auto& received:decoder.feedStamped(packet.bytes,packet.size,packet.receipt)) {
          const auto& frame=received.bytes;
          printHex("FRAME",frame.data(),frame.size());
          const auto state=alpicool::decodeStatus(frame);
          if (!state) continue;
#ifdef FROSTBRIDGE_GATEWAY
          core.observe(frame,epoch,received.first.sequence,received.first.receivedMs,millis());
#endif
          portENTER_CRITICAL(&observedMux);
          observed.status=*state;
          observed.observedMs=received.first.receivedMs;observed.epoch=epoch;++observed.sequence;
          const uint32_t sequence=observed.sequence;
          portEXIT_CRITICAL(&observedMux);
          wasFresh=true;
          char targetText[20]="unknown",currentText[20]="unknown",voltage[20]="unknown";
          if (state->targetC) snprintf(targetText,sizeof(targetText),"%.2f",*state->targetC);
          if (state->currentC) snprintf(currentText,sizeof(currentText),"%.2f",*state->currentC);
          if (state->voltage) snprintf(voltage,sizeof(voltage),"%.1f",*state->voltage);
          Serial.printf("M1_STATE ms=%lu epoch=%lu sequence=%lu stale=0 power=%s mode=%s unit_raw=%u target_c=%s current_c=%s voltage_v=%s rx_dropped=%lu\n",
            static_cast<unsigned long>(millis()),static_cast<unsigned long>(epoch),static_cast<unsigned long>(sequence),state->power?(*state->power?"on":"off"):"unknown",
            state->mode?(*state->mode==alpicool::Mode::Eco?"eco":"max"):"unknown",state->unit,
            targetText,currentText,voltage,static_cast<unsigned long>(dropped.load()));
        }
      }
    }
#ifdef FROSTBRIDGE_GATEWAY
    RuntimeRequest request;
    for(unsigned i=0;i<4 && xQueueReceive(runtimeRequests,&request,0)==pdTRUE;++i) {
      if(request.kind==RequestKind::Manual) {
        const auto result=core.submit({request.setting,request.value,frost::Source::Manual},millis());
        if(result.id){usbCommand=result.id;usbPrinted.reset();}
        if(result.error==frost::Error::WritesDisabled) Serial.println("M2_CONTROL_REJECTED reason=writes-disabled");
        else Serial.printf("M2_CONTROL_RESULT error=%u id=%lu\n",static_cast<unsigned>(result.error),static_cast<unsigned long>(result.id?result.id->sequence:0));
      } else if(request.kind==RequestKind::Clock) {
        const bool ok=core.syncTime(request.unixSeconds,millis());
        Serial.println(ok?"M2_TIME_SYNC source=usb":"M2_TIME_REJECTED");
      } else if(request.kind==RequestKind::VerifyStore) {
        Serial.printf("M4_STORE_CHECK max_bytes=%u owner_bytes=%u store_bytes=%u result=%s\n",static_cast<unsigned>(frost::DailyBytes{}.size()),static_cast<unsigned>(sizeof(frost::DailyReplay)),static_cast<unsigned>(sizeof(frost::NvsDailyStore)),frost::NvsDailyStore::verifyCapacity()?"PASS":"FAIL");
      } else {
        publish();const auto& s=gatewayStatus;
        Serial.printf("M2_STATUS writes_enabled=%d backup_enabled=%d sample_count=%u time_trusted=%d manual=%d decision=%u storage_error=%u auto_error=%u\n",s.writesEnabled,s.backupEnabled,s.sampleCount,s.timeTrusted,s.manual,static_cast<unsigned>(s.decision),static_cast<unsigned>(s.storageError),static_cast<unsigned>(s.autoError));
      }
    }
    frost::NetworkTimeSample sample;
    portENTER_CRITICAL(&networkTimeMux);sample=networkTime;portEXIT_CRITICAL(&networkTimeMux);
    if(core.syncNetworkTime(sample,millis())) Serial.println("M2_TIME_SYNC source=ntp");
    const auto health=portalMail.healthEvidence();
    core.automationLink(health.health,WiFi.status()==WL_CONNECTED,health.sampledMs,health.ackAgeMs);
    portalWorker.tick();
    if(usbCommand)if(const auto result=core.command(*usbCommand);result && usbPrinted!=result->state) {
      usbPrinted=result->state;Serial.printf("M2_USB_RESULT id=%lu state=%u error=%u target=%d sent_ms=%lu finished_ms=%lu\n",static_cast<unsigned long>(result->id.sequence),static_cast<unsigned>(result->state),static_cast<unsigned>(result->error),result->intent.value,static_cast<unsigned long>(result->sentMs),static_cast<unsigned long>(result->finishedMs));
    }
    publish();
#endif
    Snapshot snapshot;
    if (readSnapshot(snapshot) && snapshot.stale && wasFresh) {
      Serial.printf("M1_STATUS_STALE epoch=%lu sequence=%lu age_ms=%lu\n",static_cast<unsigned long>(snapshot.epoch),static_cast<unsigned long>(snapshot.sequence),static_cast<unsigned long>(snapshot.ageMs));
      wasFresh=false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}
void begin() {
  commands=xQueueCreate(2,sizeof(Command));packets=xQueueCreate(8,sizeof(Packet));
#ifdef FROSTBRIDGE_GATEWAY
  runtimeRequests=xQueueCreate(4,sizeof(RuntimeRequest));
  constexpr unsigned stackBytes=12288;
  const bool runtimeReady=runtimeRequests!=nullptr;
#else
  constexpr unsigned stackBytes=8192;
  const bool runtimeReady=true;
#endif
  if (!commands || !packets || !runtimeReady || xTaskCreate(worker,"c15-worker",stackBytes,nullptr,1,nullptr)!=pdPASS) {
    Serial.println("M1_RESOURCE_FAILURE");
    while (true) delay(1000);
  }
}
void observe(const NimBLEAdvertisedDevice* device) {
  if (selected.load()) return;
  const bool service=device->isAdvertisingService(NimBLEUUID(uint16_t(0x1234)));
  if (!device->haveName() && !service) return;
  Candidate candidate{};
  snprintf(candidate.address,sizeof(candidate.address),"%s",device->getAddress().toString().c_str());
  snprintf(candidate.name,sizeof(candidate.name),"%s",device->getName().c_str());
  candidate.rssi=device->getRSSI();candidate.type=device->getAddressType();candidate.service=service;
  portENTER_CRITICAL(&candidateMux);
  unsigned index=0;
  while (index<count && (strcmp(candidates[index].address,candidate.address)!=0 || candidates[index].type!=candidate.type)) ++index;
  if (index<capacity) {
    if (index==count) ++count;
    candidates[index]=candidate;
  }
  portEXIT_CRITICAL(&candidateMux);
}
void listDevices() {
  Candidate copy[capacity];unsigned size;
  portENTER_CRITICAL(&candidateMux);size=count;memcpy(copy,candidates,sizeof(copy));portEXIT_CRITICAL(&candidateMux);
  Serial.printf("M1_LIST count=%u\n",size);
  for (unsigned i=0;i<size;++i) {
    char name[sizeof(copy[i].name)*2+1];size_t n=strlen(copy[i].name);
    for(size_t j=0;j<n;++j) snprintf(name+2*j,3,"%02X",static_cast<unsigned char>(copy[i].name[j]));
    name[2*n]=0;
    Serial.printf("M1_DEVICE index=%u address=%s type=%u rssi=%d service1234=%d name_hex=%s\n",i,copy[i].address,copy[i].type,copy[i].rssi,copy[i].service,name);
  }
  Serial.println("M1_LIST_END");
}
bool selectTarget(unsigned index) {
  Command command{};
  portENTER_CRITICAL(&candidateMux);
  bool valid=index<count;
  if (valid) command.target=candidates[index];
  portEXIT_CRITICAL(&candidateMux);
  if (!valid || !commands) return false;
  const bool previous=selected.exchange(true);
  if (xQueueSend(commands,&command,0)!=pdTRUE) { selected=previous;return false; }
  return true;
}
void disconnectTarget() {
  Command command{};command.stop=true;
  if (!commands || xQueueSend(commands,&command,0)!=pdTRUE) Serial.println("M1_BUSY");
}
bool connected() { return link.load(); }
bool readSnapshot(Snapshot& snapshot) {
  portENTER_CRITICAL(&observedMux);snapshot=observed;portEXIT_CRITICAL(&observedMux);
  if (!snapshot.sequence) return false;
  snapshot.ageMs=millis()-snapshot.observedMs;
  snapshot.stale=!link.load() || !statusReady.load() || snapshot.epoch!=connectionEpoch.load() || snapshot.ageMs>30000;
  return true;
}
bool scanAllowed() { return !selected.load(); }
#ifdef FROSTBRIDGE_GATEWAY
bool readGatewayStatus(GatewayStatus& status) {
  portENTER_CRITICAL(&gatewayMux);status=gatewayStatus;const bool valid=gatewayPublished;portEXIT_CRITICAL(&gatewayMux);return valid;
}
bool postControl(alpicool::Setting setting,int value) {
  RuntimeRequest r;r.kind=RequestKind::Manual;r.setting=setting;r.value=value;
  return runtimeRequests && xQueueSend(runtimeRequests,&r,0)==pdTRUE;
}
bool postClock(int64_t unixSeconds) {
  RuntimeRequest r;r.kind=RequestKind::Clock;r.unixSeconds=unixSeconds;
  return runtimeRequests && xQueueSend(runtimeRequests,&r,0)==pdTRUE;
}
bool postGatewayDiagnostic(bool verifyStore) {
  RuntimeRequest r;r.kind=verifyStore?RequestKind::VerifyStore:RequestKind::Status;
  return runtimeRequests && xQueueSend(runtimeRequests,&r,0)==pdTRUE;
}
void noteNetworkTime(int64_t seconds,uint16_t fractionMs) {
  if(seconds<1577836800 || seconds>4102444800LL) return;
  portENTER_CRITICAL(&networkTimeMux);
  networkTime.seconds=static_cast<uint32_t>(seconds);networkTime.fractionMs=fractionMs;networkTime.receivedMs=millis();++networkTime.sequence;
  portEXIT_CRITICAL(&networkTimeMux);
}
frost::ApiBackend portalBackend() {
  return {[]{auto s=portalMail.snapshot();s.connected=s.connected && link.load() && statusReady.load() && s.epoch==connectionEpoch.load();return s;},
    [](frost::WebAction action,uint32_t now){return portalMail.submit(action,now);},
    [](frost::Ticket id){return portalMail.lookup(id);},[]{return portalMail.results();},
    [](const std::array<frost::WebAction,2>& actions,uint32_t now){return portalMail.submitPair(actions,now);},
    [](uint32_t now){portalMail.rejectAutomation(now);},
    [](frost::AutomationHealth health,uint32_t now,std::optional<uint32_t> age){portalMail.reportHealth(health,now,age);},
    [](bool sample,uint8_t offset,uint32_t revision){return portalMail.page(sample,offset,revision);}};
}
#endif
}
#endif
