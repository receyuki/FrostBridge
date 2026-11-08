#pragma once
#include "Portal.h"
namespace frost {
enum class HomeKitChannel : uint8_t { Power,Eco,AutomationPower,AutomationEco,Temperature,AutomationTemperature };
struct HomeKitValue { std::optional<bool> observed;bool fresh=false,pending=false;ApiError error=ApiError::None; };
struct HomeKitClimate {std::optional<float> current,target;std::optional<bool> power;bool fresh=false,pending=false,targetPending=false,powerPending=false;};
enum class LinkHealth : uint8_t { Unconfigured,Healthy,Expired };
class HomeKitControl {
 public:
  // Single owner: HomeSpan's main loop, never the BLE worker. Backend calls must
  // be bounded/nonblocking mailbox operations. true means queued acceptance,
  // not device success (requestClimate also accepts empty metadata-only calls).
  // Per-field pending includes a bounded result-to-snapshot publication hold.
  // tick consumes terminal results, value exposes observed
  // device state (never intent), freshness, pending status and the last error.
  // Call tick before replacing completed requests; request also does this so a
  // HAP callback arriving before the next loop cannot discard its predecessor.
  // Concurrent backends must supply the same monotonic clock used by the worker.
  // It is sampled after terminal lookup and snapshot reads, never before a
  // possible preemption that could make an observation newer than the caller.
  // Empty clock is for synchronous host backends, using deterministic tick(now).
  explicit HomeKitControl(ApiBackend backend,std::function<uint32_t()> clock={}):backend_(std::move(backend)),clock_(std::move(clock)){}
  bool request(HomeKitChannel channel,bool value,uint32_t now);
  // Native cooler writes may contain temperature and power together. Mailbox
  // admission is atomic; device commands still execute/confirm individually.
  // target must be finite integral Celsius in -20..20 and the observed device
  // limits. Fresh/connected/writable/unlocked Celsius state is required.
  // Neither field: true without queueing. Otherwise true means all fields were
  // queued or joined an identical in-flight ordinary request; false queues none.
  // Automated requests are not coalesced. Per-field errors are in diagnostics;
  // climate returns last observed values/freshness/pending, never requested ones.
  bool requestClimate(std::optional<float> target,std::optional<bool> power,bool automatic,uint32_t now);
  HomeKitClimate climate(bool automatic,uint32_t now) const;
  void tick(uint32_t now);
  HomeKitValue value(HomeKitChannel channel,uint32_t now) const;
  void diagnostics(WebSnapshot& snapshot,uint32_t now) const;
  bool probeDue(uint32_t now,bool connected);
  bool acknowledge(uint32_t now);
  LinkHealth health(uint32_t now) const;
  std::optional<uint32_t> ackAge(uint32_t now) const;
  uint32_t acknowledgements() const {return acks_;}
  uint32_t automationRequests() const {return automationRequests_;}
  uint32_t automationConfirmed() const {return automationConfirmed_;}
  unsigned pending() const;
 private:
  struct Slot { std::optional<Ticket> id;ApiError error=ApiError::None;
    std::optional<uint32_t> publishAfter;uint32_t publicationBoot=0;
  };
  static bool awaitingPublication(const Slot& slot,const WebSnapshot& snapshot,uint32_t now);
  bool rejected(bool automatic,uint32_t now);
  ApiBackend backend_;std::function<uint32_t()> clock_;std::array<Slot,6> slots_{};
  bool sent_=false,waiting_=false;uint32_t sentMs_=0,lastAck_=0,acks_=0;
  uint32_t automationRequests_=0,automationConfirmed_=0;
};
}
