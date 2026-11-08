#include "HomeKitControl.h"
#include <cmath>
namespace frost {
namespace {
size_t index(HomeKitChannel channel){return static_cast<size_t>(channel);}
bool isEco(HomeKitChannel channel){return channel==HomeKitChannel::Eco || channel==HomeKitChannel::AutomationEco;}
bool isAutomation(HomeKitChannel channel){return channel==HomeKitChannel::AutomationPower || channel==HomeKitChannel::AutomationEco || channel==HomeKitChannel::AutomationTemperature;}
bool active(WebState state){return state==WebState::Queued || state==WebState::Running || state==WebState::AwaitingReadback;}
bool fresh(const WebSnapshot& s,uint32_t now){return s.ready && s.connected && s.observed && s.valid && now-s.observedMs<=30000;}
}
bool HomeKitControl::rejected(bool automatic,uint32_t now){
  if(automatic && backend_.rejectedAutomation)backend_.rejectedAutomation(clock_?clock_():now);
  return false;
}
bool HomeKitControl::request(HomeKitChannel channel,bool requested,uint32_t now){
  if(channel==HomeKitChannel::Temperature || channel==HomeKitChannel::AutomationTemperature)return rejected(isAutomation(channel),now);
  tick(now);
  if(isAutomation(channel))++automationRequests_;
  auto& slot=slots_[index(channel)];const auto s=backend_.snapshot();if(clock_)now=clock_();
  if(slot.id){slot.error=ApiError::Busy;return rejected(isAutomation(channel),now);}
  if(!fresh(s,now)){slot.error=!s.connected?ApiError::Offline:ApiError::Stale;return rejected(isAutomation(channel),now);}
  if(!s.writes){slot.error=ApiError::WritesDisabled;return rejected(isAutomation(channel),now);}
  const auto error=s.controls[isEco(channel)?2:1];if(error!=Error::None){slot.error=apiError(error);return rejected(isAutomation(channel),now);}
  WebAction action;action.epoch=s.epoch;
  action.intent={isEco(channel)?alpicool::Setting::Mode:alpicool::Setting::Power,requested?1:0,isAutomation(channel)?Source::HomeKitAutomation:Source::HomeKit};
  slot.id=backend_.submit(action,now);if(slot.id)slot.publishAfter.reset();slot.error=slot.id?ApiError::None:ApiError::Busy;return slot.id?true:rejected(isAutomation(channel),now);
}
bool HomeKitControl::requestClimate(std::optional<float> target,std::optional<bool> power,bool automatic,uint32_t now){
  if(!target && !power)return true;
  tick(now);const auto s=backend_.snapshot();if(clock_)now=clock_();
  const auto tc=automatic?HomeKitChannel::AutomationTemperature:HomeKitChannel::Temperature;
  const auto pc=automatic?HomeKitChannel::AutomationPower:HomeKitChannel::Power;
  auto& ts=slots_[index(tc)];auto& ps=slots_[index(pc)];
  if(automatic)automationRequests_+=unsigned(target.has_value())+unsigned(power.has_value());
  ApiError error=ApiError::None;
  if(!fresh(s,now))error=s.connected?ApiError::Stale:ApiError::Offline;
  else if(!s.writes)error=ApiError::WritesDisabled;
  else if(target && (!std::isfinite(*target) || std::floor(*target)!=*target || *target<-20 || *target>20 || (s.status.minC && *target<*s.status.minC) || (s.status.maxC && *target>*s.status.maxC)))error=ApiError::OutOfRange;
  else if(target && s.controls[0]!=Error::None)error=apiError(s.controls[0]);
  else if(power && s.controls[1]!=Error::None)error=apiError(s.controls[1]);
  // Siri may repeat a field while its first accepted write is still in flight.
  // Join only an identical ordinary request; automated deliveries retain their
  // separate counting/Query semantics. Conflicting intentions remain Busy.
  const auto samePending=[&](const Slot& slot,alpicool::Setting setting,int value){
    if(automatic || !slot.id)return rejected(automatic,now);
    const auto r=backend_.lookup(*slot.id);
    return r && active(r->state) && r->intent.source==Source::HomeKit && r->intent.setting==setting && r->intent.value==value;
  };
  if(error==ApiError::None && ((target && ts.id && !samePending(ts,alpicool::Setting::TargetC,static_cast<int>(*target))) ||
      (power && ps.id && !samePending(ps,alpicool::Setting::Power,*power?1:0))))error=ApiError::Busy;
  if(error!=ApiError::None){if(target)ts.error=error;if(power)ps.error=error;return rejected(automatic,now);}
  const bool enqueueTarget=target && !ts.id,enqueuePower=power && !ps.id;
  if(!enqueueTarget && !enqueuePower){if(target)ts.error=ApiError::None;if(power)ps.error=ApiError::None;return true;}
  std::array<WebAction,2> actions;auto& temperature=actions[0];auto& active=actions[1];temperature.epoch=active.epoch=s.epoch;
  const auto source=automatic?Source::HomeKitAutomation:Source::HomeKit;
  if(target)temperature.intent={alpicool::Setting::TargetC,static_cast<int>(*target),source};
  if(power)active.intent={alpicool::Setting::Power,*power?1:0,source};
  if(enqueueTarget && enqueuePower){
    const auto ids=backend_.submitPair?backend_.submitPair(actions,now):std::nullopt;
    if(!ids){ts.error=ps.error=ApiError::Busy;return rejected(automatic,now);}
    ts.id=(*ids)[0];ps.id=(*ids)[1];ts.publishAfter.reset();ps.publishAfter.reset();ts.error=ps.error=ApiError::None;
  }else{
    auto& slot=enqueueTarget?ts:ps;slot.id=backend_.submit(enqueueTarget?temperature:active,now);slot.error=slot.id?ApiError::None:ApiError::Busy;
    if(!slot.id)return rejected(automatic,now);
    slot.publishAfter.reset();
  }
  if(target)ts.error=ApiError::None;
  if(power)ps.error=ApiError::None;
  return true;
}
HomeKitClimate HomeKitControl::climate(bool automatic,uint32_t now) const{
  const auto s=backend_.snapshot();if(clock_)now=clock_();HomeKitClimate r;r.fresh=fresh(s,now);
  if(s.observed){r.current=s.status.currentC;r.target=s.status.targetC;r.power=s.status.power;}
  r.targetPending=awaitingPublication(slots_[index(automatic?HomeKitChannel::AutomationTemperature:HomeKitChannel::Temperature)],s,now);
  r.powerPending=awaitingPublication(slots_[index(automatic?HomeKitChannel::AutomationPower:HomeKitChannel::Power)],s,now);r.pending=r.targetPending || r.powerPending;return r;
}
bool HomeKitControl::awaitingPublication(const Slot& slot,const WebSnapshot& s,uint32_t now){
  // Results and snapshots use separate mailbox publications. A confirmed result
  // can precede its snapshot by one worker iteration. Gate on consumption time,
  // not the earlier BLE receipt time. Reuse the getter's snapshot
  // to avoid another large copy on HomeSpan's stack. Bound the presentation hold.
  return slot.id.has_value() || (slot.publishAfter && s.boot==slot.publicationBoot &&
    now-*slot.publishAfter<1000 && static_cast<int32_t>(s.capturedMs-*slot.publishAfter)<=0);
}
void HomeKitControl::tick(uint32_t now){
  for(size_t i=0;i<slots_.size();++i){auto& slot=slots_[i];if(slot.id){
    const auto r=backend_.lookup(*slot.id);
    if(!r){slot.id.reset();slot.publishAfter.reset();slot.error=ApiError::Expired;}
    else if(!active(r->state)){if(isAutomation(static_cast<HomeKitChannel>(i)) && r->state==WebState::Confirmed)++automationConfirmed_;
      slot.publishAfter=r->state==WebState::Confirmed?std::optional<uint32_t>(clock_?clock_():now):std::nullopt;
      slot.publicationBoot=r->id.boot;slot.error=r->error;slot.id.reset();}
  }}
  if(backend_.reportHealth){
    const auto at=clock_?clock_():now;const auto link=health(at);
    backend_.reportHealth(link==LinkHealth::Healthy?AutomationHealth::Healthy:link==LinkHealth::Expired?AutomationHealth::Expired:AutomationHealth::Unknown,at,ackAge(at));
  }

}
HomeKitValue HomeKitControl::value(HomeKitChannel channel,uint32_t now) const{
  const auto s=backend_.snapshot();if(clock_)now=clock_();HomeKitValue result;result.fresh=fresh(s,now);
  if(s.observed){if(isEco(channel)){if(s.status.mode)result.observed=*s.status.mode==alpicool::Mode::Eco;}else result.observed=s.status.power;}
  const auto& slot=slots_[index(channel)];result.pending=awaitingPublication(slot,s,now);result.error=slot.error;
  if(!result.fresh && result.error==ApiError::None)result.error=s.connected?ApiError::Stale:ApiError::Offline;
  return result;
}
void HomeKitControl::diagnostics(WebSnapshot& s,uint32_t now) const{
  s.automationHealth=static_cast<uint8_t>(health(now));s.healthAckAge=ackAge(now);s.healthAcks=acks_;
  s.homekitPending=pending();s.automationRequests=automationRequests_;s.automationConfirmed=automationConfirmed_;
  for(size_t i=0;i<slots_.size();++i){s.homekitErrors[i]=slots_[i].error;if(!fresh(s,now) && s.homekitErrors[i]==ApiError::None)s.homekitErrors[i]=s.connected?ApiError::Stale:ApiError::Offline;}
}
bool HomeKitControl::probeDue(uint32_t now,bool connected){
  if(!connected){waiting_=false;return false;}
  if(sent_ && now-sentMs_<60000)return false;
  sent_=waiting_=true;sentMs_=now;return true;
}
bool HomeKitControl::acknowledge(uint32_t now){
  if(!waiting_ || now-sentMs_>15000)return false;
  waiting_=false;lastAck_=now;++acks_;return true;
}
LinkHealth HomeKitControl::health(uint32_t now) const{return !acks_?LinkHealth::Unconfigured:now-lastAck_<=180000?LinkHealth::Healthy:LinkHealth::Expired;}
std::optional<uint32_t> HomeKitControl::ackAge(uint32_t now) const{return acks_?std::optional<uint32_t>(now-lastAck_):std::nullopt;}
unsigned HomeKitControl::pending() const{unsigned result=0;for(const auto& slot:slots_)result+=slot.id.has_value();return result;}
}
