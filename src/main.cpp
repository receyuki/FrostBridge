#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#ifndef FROSTBRIDGE_BASIC
#include <HomeSpan.h>
#ifdef FROSTBRIDGE_M1
#include "BleReadout.h"
#endif
#include <NimBLEDevice.h>
#include <Preferences.h>
#ifdef FROSTBRIDGE_GATEWAY
#include "PortalHttpServer.h"
#include "HomeKitBridge.h"
#include <memory>
#else
#include <WebServer.h>
#endif
#include <atomic>
#include <cstring>
#ifdef FROSTBRIDGE_GATEWAY
#include <cerrno>
#include <climits>
#include <time.h>
#include <esp_sntp.h>
#include <nvs.h>
#endif

namespace {
#ifdef FROSTBRIDGE_GATEWAY
std::unique_ptr<frost::PortalHttpServer> portal;
struct HeapMinimum {uint32_t bytes=UINT32_MAX,atMs=0;const char* phase="startup";} heapMinimum;
// Coarse main-loop attribution only: the BLE/Wi-Fi tasks may allocate concurrently.
void noteHeapMinimum(const char* phase){
  const auto bytes=ESP.getMinFreeHeap();
  if(bytes<heapMinimum.bytes)heapMinimum={bytes,millis(),phase};
}
#else
WebServer server(80);
#endif
std::atomic<uint32_t> advertisements{0};
std::atomic<unsigned> wifiDisconnectReason{0};
bool bleReady = false;
bool webStarted = false;
Preferences smokeConfig;

class ScanCallbacks final : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    ++advertisements;
#ifdef FROSTBRIDGE_M1
    m1::observe(device);
#else
    (void)device;
#endif
  }
};
ScanCallbacks scanCallbacks;

void startWeb(int) {
  if (webStarted) return;
#ifdef FROSTBRIDGE_GATEWAY
  auto backend=m1::portalBackend();const auto read=backend.snapshot;
  backend.snapshot=[read] {
    auto s=read();s.wifi=WiFi.status()==WL_CONNECTED;s.ble=bleReady;s.scanning=bleReady && NimBLEDevice::getScan()->isScanning();
    s.advertisements=advertisements.load();s.freeHeap=ESP.getFreeHeap();s.minHeap=ESP.getMinFreeHeap();s.largestBlock=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frost::hap::diagnostics(s,millis());return s;
  };
  portal=std::make_unique<frost::PortalHttpServer>(std::move(backend),[]{return std::vector<std::string>{"frostbridge.local",WiFi.localIP().toString().c_str()};});
  portal->begin();
#else
  server.on("/health", HTTP_GET, [] {
    char body[768];
    char stateHealth[144]="";
    char gatewayHealth[256]="";
#ifdef FROSTBRIDGE_M1
#ifdef FROSTBRIDGE_GATEWAY
    const char* firmware="m2-gateway-readonly";
    m1::GatewayStatus gateway;
    const bool gatewayReady=m1::readGatewayStatus(gateway);
    snprintf(gatewayHealth,sizeof(gatewayHealth),",\"gateway_ready\":%s,\"writes_enabled\":false,\"backup_enabled\":%s,\"time_trusted\":%s,\"manual_override\":%s,\"sample_count\":%u,\"backup_error\":%u",gatewayReady?"true":"false",gateway.backupEnabled?"true":"false",gateway.timeTrusted?"true":"false",gateway.manual?"true":"false",gateway.sampleCount,static_cast<unsigned>(gateway.autoError));
#else
    const char* firmware="m1-readout";
#endif
    const bool fridgeConnected=m1::connected();
    m1::Snapshot snapshot;
    if (m1::readSnapshot(snapshot))
      snprintf(stateHealth,sizeof(stateHealth),",\"state_stale\":%s,\"state_age_ms\":%lu,\"state_epoch\":%lu,\"state_sequence\":%lu",snapshot.stale?"true":"false",static_cast<unsigned long>(snapshot.ageMs),static_cast<unsigned long>(snapshot.epoch),static_cast<unsigned long>(snapshot.sequence));
    else snprintf(stateHealth,sizeof(stateHealth),",\"state_stale\":true,\"state_age_ms\":null");
#else
    const char* firmware="m0-smoke";
    const bool fridgeConnected=false;
#endif
    snprintf(body, sizeof(body),
             "{\"firmware\":\"%s\",\"uptime_ms\":%lu,"
             "\"wifi_connected\":%s,\"ble_initialized\":%s,\"scan_running\":%s,"
             "\"advertisements\":%lu,\"free_heap\":%u,\"min_heap\":%u,"
             "\"largest_block\":%u,\"fridge_connected\":%s%s%s}",
             firmware,
             static_cast<unsigned long>(millis()),
             WiFi.status() == WL_CONNECTED ? "true" : "false",
             bleReady ? "true" : "false",
             bleReady && NimBLEDevice::getScan()->isScanning() ? "true" : "false",
             static_cast<unsigned long>(advertisements.load()), static_cast<unsigned>(ESP.getFreeHeap()),
             static_cast<unsigned>(ESP.getMinFreeHeap()), static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)), fridgeConnected ? "true" : "false",stateHealth,gatewayHealth);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
  });
  server.begin();
#endif
  webStarted = true;
#ifdef FROSTBRIDGE_GATEWAY
  esp_sntp_set_time_sync_notification_cb([](struct timeval* received) { if(received) m1::noteNetworkTime(received->tv_sec,received->tv_usec/1000); });
  configTime(0,0,"time.apple.com","pool.ntp.org");
#endif
  Serial.println("M0 web ready on port 80; HAP port 1201");
}

#ifdef FROSTBRIDGE_GATEWAY
bool parseInteger(const char* text,long long& value) {
  if(!((*text>='0' && *text<='9') || (*text=='-' && text[1]>='0' && text[1]<='9'))) return false;
  errno=0;char* end=nullptr;value=strtoll(text,&end,10);
  return errno!=ERANGE && end && *end==0;
}
#endif

// Physical USB setup only: bounded, nonblocking, no credential echo.
// Delimiter and byte limits are enforced by both ends of scripts/provision.py.
void handleSetupSerial() {
  static char input[112]{};
  static size_t used = 0;
  static bool overflow = false;
  for (unsigned count = 0; count < 32 && Serial.available(); ++count) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (used < sizeof(input) - 1) input[used++] = c;
      else overflow = true;
      continue;
    }
    input[used] = '\0';
    if (!overflow && strncmp(input, "WIFI\t", 5) == 0) {
      char* ssid = input + 5;
      char* password = strchr(ssid, '\t');
      if (password) {
        *password++ = '\0';
        if (strlen(ssid) >= 1 && strlen(ssid) <= 32 && strlen(password) >= 8 &&
            strlen(password) <= 63 && strchr(password, '\t') == nullptr) {
          homeSpan.setWifiCredentials(ssid, password);
          memset(input, 0, sizeof(input));
          Serial.println("M0_CONFIG_SAVED");
          Serial.flush();
          ESP.restart();
        }
      }
      Serial.println("M0_CONFIG_REJECTED");
    } else if (!overflow && strncmp(input, "SSID\t", 5) == 0) {
      const char* ssid = input + 5;
      // HomeSpan 2.1.8 persists this exact public Network_HS::wifiData type.
      // Read on-device only; the password is never sent back over USB.
      decltype(Network_HS::wifiData) saved{};
      Preferences wifiStore;
      bool valid = strlen(ssid) >= 1 && strlen(ssid) <= MAX_SSID &&
                   strchr(ssid, '\t') == nullptr && wifiStore.begin("WIFI", true);
      if (valid) {
        valid = wifiStore.getBytesLength("WIFIDATA") == sizeof(saved) &&
                wifiStore.getBytes("WIFIDATA", &saved, sizeof(saved)) == sizeof(saved);
        wifiStore.end();
      }
      if (valid) {
        valid = memchr(saved.pwd, '\0', sizeof(saved.pwd)) != nullptr;
      }
      if (valid) {
        const size_t length = strlen(saved.pwd);
        valid = length >= 8 && length <= 63;
      }
      if (valid) {
        homeSpan.setWifiCredentials(ssid, saved.pwd);
        memset(&saved, 0, sizeof(saved));
        memset(input, 0, sizeof(input));
        Serial.println("M0_CONFIG_SAVED");
        Serial.flush();
        ESP.restart();
      }
      memset(&saved, 0, sizeof(saved));
      Serial.println("M0_CONFIG_REJECTED");
    } else if (!overflow && strcmp(input, "PAIR") == 0) {
#ifdef FROSTBRIDGE_GATEWAY
      const auto code=smokeConfig.getString("code");HapQR qr;
      Serial.printf("FROST_PAIR %s %s\n",code.c_str(),qr.get(strtoul(code.c_str(),nullptr,10),"FRST",static_cast<uint8_t>(Category::Bridges)));
#else
      Serial.printf("M0_PAIR %s\n", smokeConfig.getString("code").c_str());
#endif
    } else if (!overflow && strcmp(input, "IP") == 0) {
      // Deliberately requested local information; keep out of shared logs.
      Serial.printf("M0_IP %s\n", WiFi.localIP().toString().c_str());
    }
#ifdef FROSTBRIDGE_M1
    else if (!overflow && strcmp(input,"BLE_STATUS")==0) {
      m1::Snapshot snapshot;const bool fresh=m1::readSnapshot(snapshot) && !snapshot.stale;
      Serial.printf("FROST_BLE_STATUS selected=%u connected=%u fresh=%u\n",m1::scanAllowed()?0u:1u,m1::connected()?1u:0u,fresh?1u:0u);
    }
    else if (!overflow && strcmp(input,"BLE_LIST")==0) m1::listDevices();
    else if (!overflow && strncmp(input,"BLE_TARGET\t",11)==0) {
      char* end=nullptr;const unsigned long index=strtoul(input+11,&end,10);
      const bool valid=input[11]>='0' && input[11]<='9' && end && *end==0 && index<48;
      Serial.println(valid && m1::selectTarget(index)?"M1_TARGET_ACCEPTED":"M1_TARGET_REJECTED");
    }
    else if (!overflow && strcmp(input,"BLE_DISCONNECT")==0) m1::disconnectTarget();
#ifdef FROSTBRIDGE_GATEWAY
    else if(!overflow && strcmp(input,"RESOURCE_STATS")==0) {
      nvs_stats_t stats{};const auto error=nvs_get_stats(nullptr,&stats);
      m1::GatewayStatus gateway{};const bool worker=m1::readGatewayStatus(gateway);
      Serial.printf("RESOURCE_STATS heap=%u min_heap=%u largest=%u main_stack_low_water=%u worker_ready=%u worker_stack_low_water=%lu nvs_ok=%u nvs_used=%u nvs_available=%u nvs_total=%u min_phase=%s min_phase_ms=%lu\n",
        static_cast<unsigned>(ESP.getFreeHeap()),static_cast<unsigned>(ESP.getMinFreeHeap()),static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),worker?1u:0u,static_cast<unsigned long>(gateway.workerStackLowWater),error==ESP_OK?1u:0u,
        static_cast<unsigned>(stats.used_entries),static_cast<unsigned>(stats.available_entries),static_cast<unsigned>(stats.total_entries),heapMinimum.phase,static_cast<unsigned long>(heapMinimum.atMs));
    }
    else if(!overflow && strcmp(input,"HAP_INFO")==0) {
      // Explicit physical-USB diagnostics only; never expose these over HTTP.
      homeSpan.setLogLevel(0);homeSpan.processSerialCommand("i");homeSpan.processSerialCommand("d");homeSpan.setLogLevel(-1);
    }
    else if(!overflow && strcmp(input,"HAP_TRACE")==0) {homeSpan.setLogLevel(1);Serial.println("HAP_TRACE_ENABLED");}
    else if(!overflow && strcmp(input,"HAP_QUIET")==0) {homeSpan.setLogLevel(-1);Serial.println("HAP_TRACE_DISABLED");}
    else if(!overflow && strcmp(input,"HAP_NEW_ID")==0) {
      // Recover a failed onboarding identity only when no controller is paired.
      // HomeSpan H clears only its HAP namespace; WIFI/SRP/fb-* are preserved.
      if(homeSpan.controllerListBegin()!=homeSpan.controllerListEnd())Serial.println("HAP_ID_REJECTED_PAIRED");
      else {Serial.println("HAP_ID_RECREATING");Serial.flush();homeSpan.processSerialCommand("H");}
    }
    else if(!overflow && strncmp(input,"CLOCK\t",6)==0) {
      long long value=0;const bool valid=parseInteger(input+6,value);
      Serial.println(valid && m1::postClock(value)?"M2_REQUEST_QUEUED":"M2_REQUEST_REJECTED");
    }
    else if(!overflow && (strncmp(input,"CTRL_TEMP\t",10)==0 || strncmp(input,"CTRL_MODE\t",10)==0 || strncmp(input,"CTRL_POWER\t",11)==0)) {
      const bool power=strncmp(input,"CTRL_POWER\t",11)==0;
      const auto setting=power?alpicool::Setting::Power:(input[5]=='T'?alpicool::Setting::TargetC:alpicool::Setting::Mode);
      long long value=0;const bool valid=parseInteger(input+(power?11:10),value) && value>=INT_MIN && value<=INT_MAX && (setting==alpicool::Setting::TargetC || value==0 || value==1);
      Serial.println(valid && m1::postControl(setting,static_cast<int>(value))?"M2_REQUEST_QUEUED":"M2_REQUEST_REJECTED");
    }
    else if(!overflow && (strcmp(input,"BACKUP_STATUS")==0 || strcmp(input,"BACKUP_STORE_CHECK")==0)) {
      Serial.println(m1::postGatewayDiagnostic(strcmp(input,"BACKUP_STORE_CHECK")==0)?"M2_REQUEST_QUEUED":"M2_REQUEST_REJECTED");
    }
#endif
#endif
    else Serial.println("M0_CONFIG_REJECTED");
    memset(input, 0, sizeof(input));
    used = 0;
    overflow = false;
  }
}
}  // namespace
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("FrostBridge M0; chip=%s rev=%d flash=%u psram=%u reset=%d\n",
                ESP.getChipModel(), ESP.getChipRevision(), static_cast<unsigned>(ESP.getFlashChipSize()),
                static_cast<unsigned>(ESP.getPsramSize()), static_cast<int>(esp_reset_reason()));
#ifndef FROSTBRIDGE_BASIC
  Network.onEvent([](arduino_event_t* event) {
    if (event->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifiDisconnectReason.store(event->event_info.wifi_sta_disconnected.reason);
    }
  });
  homeSpan.setLogLevel(-1);
  homeSpan.setSerialInputDisable(true);
  homeSpan.setPortNum(1201);
  homeSpan.setConnectionCallback(startWeb);
#ifdef FROSTBRIDGE_GATEWAY
  homeSpan.setHostNameSuffix("");
  homeSpan.setQRID("FRST");
  homeSpan.begin(Category::Bridges, "FrostBridge", "frostbridge");
#else
  homeSpan.begin(Category::Switches, "FrostBridge M0 Test", "frostbridge-m0");
#endif
  if (!smokeConfig.begin("fb-m0", false)) {
    Serial.println("M0 config storage unavailable; halted before pairing/scanning");
    while (true) delay(1000);
  }
  if (smokeConfig.getString("code").isEmpty()) {
    char code[9];
    // Different final digit guarantees it is not an all-identical forbidden code;
    // leading 4 also excludes the two forbidden ascending/descending sequences.
    const uint32_t middle = esp_random() % 1000000;
    snprintf(code, sizeof(code), "4%06lu7", static_cast<unsigned long>(middle));
    homeSpan.setPairingCode(code);
    if (smokeConfig.putString("code", code) == 0) {
      memset(code, 0, sizeof(code));
      Serial.println("M0 pairing persistence failed; halted");
      while (true) delay(1000);
    }
    memset(code, 0, sizeof(code));
  }
#ifdef FROSTBRIDGE_GATEWAY
  frost::hap::begin(m1::portalBackend());
#else
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name("FrostBridge M0 Test");
  new Service::Switch();
  new Characteristic::On(false);
  // No production actuator: this Switch is an isolated compatibility probe.
#endif
  bleReady = NimBLEDevice::init("");
  if (bleReady) {
    auto* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks, true);
#ifdef FROSTBRIDGE_M1
    scan->setActiveScan(true);
#else
    scan->setActiveScan(false);
#endif
    scan->setMaxResults(0);
    scan->start(30000);
  }
#ifdef FROSTBRIDGE_M1
  m1::begin();
  Serial.println("M1_SCAN_ONLY select_target_required=1");
#endif
  Serial.printf("M0 coexistence initialized; ble=%d; USB setup available\n", bleReady);
#endif
}

void loop() {
#ifndef FROSTBRIDGE_BASIC
#ifdef FROSTBRIDGE_GATEWAY
  noteHeapMinimum("background");
#endif
  homeSpan.poll();
#ifdef FROSTBRIDGE_GATEWAY
  noteHeapMinimum("homekit_poll");
#endif
  handleSetupSerial();
#ifdef FROSTBRIDGE_GATEWAY
  frost::hap::poll(millis());
  if(webStarted){portal->poll(millis());noteHeapMinimum("web_poll");}
#else
  if(webStarted)server.handleClient();
#endif
  static uint32_t lastScanAttempt = 0;
  if (bleReady
#ifdef FROSTBRIDGE_M1
      && m1::scanAllowed()
#endif
      && !NimBLEDevice::getScan()->isScanning() &&
      millis() - lastScanAttempt >= 1000) {
    lastScanAttempt = millis();
    NimBLEDevice::getScan()->start(30000);
  }
#endif
  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 5000) {
    lastReport = millis();
    Serial.printf("uptime_ms=%lu heap=%u min_heap=%u largest=%u",
                  static_cast<unsigned long>(millis()), static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMinFreeHeap()), static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
#ifndef FROSTBRIDGE_BASIC
    Serial.printf(" wifi=%d wifi_status=%d disconnect_reason=%u scan=%d advertisements=%lu",
                  WiFi.status() == WL_CONNECTED, static_cast<int>(WiFi.status()), wifiDisconnectReason.load(),
                  bleReady && NimBLEDevice::getScan()->isScanning(),
                  static_cast<unsigned long>(advertisements.load()));
#endif
    Serial.printf(" loop_stack_low_water=%u\n",static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  }
  delay(1);
}
