#include "Portal.h"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace frost {
static_assert(std::is_trivially_copyable<WebAction>::value && std::is_trivially_copyable<WebSnapshot>::value && std::is_trivially_copyable<WebResult>::value,"Mailbox critical sections may only copy fixed data");
namespace {
struct Guard { Critical lock;explicit Guard(Critical c):lock(c){if(lock.enter)lock.enter(lock.context);}~Guard(){if(lock.leave)lock.leave(lock.context);} };
bool active(WebState state) { return state==WebState::Queued || state==WebState::Running || state==WebState::AwaitingReadback; }
bool automated(const WebAction& action){return action.kind==ActionKind::Command && action.intent.source==Source::HomeKitAutomation;}
bool same(const DailyAction& a,const DailyAction& b){return a.second==b.second && a.field==b.field && a.value==b.value && a.order==b.order;}
}
void PortalMailbox::reset(uint32_t boot) { Guard guard(critical_);boot_=boot;next_=0;head_=count_=0;results_={};snapshot_={};snapshot_.boot=boot;evidence_={};loss_=false;lossFirstMs_=lossLastMs_=0;sample_={};candidate_={};sampleRevision_=todayRevision_=0; }
std::optional<Ticket> PortalMailbox::submit(WebAction action,uint32_t nowMs) {
  Guard guard(critical_);const auto now=now_?now_():nowMs;
  if(!room(1)){if(automated(action))markLoss(now);return std::nullopt;}return push(action,now);
}
std::optional<std::array<Ticket,2>> PortalMailbox::submitPair(const std::array<WebAction,2>& actions,uint32_t nowMs) {
  Guard guard(critical_);const auto now=now_?now_():nowMs;
  if(!room(2)){if(automated(actions[0]) || automated(actions[1]))markLoss(now);return std::nullopt;}
  return std::array<Ticket,2>{{push(actions[0],now),push(actions[1],now)}};
}
bool PortalMailbox::room(size_t amount) const {
  if(count_+amount>queue_.size() || next_>std::numeric_limits<uint32_t>::max()-amount)return false;
  size_t available=0;for(const auto& r:results_)if(!r.id.sequence || !active(r.state))++available;
  return available>=amount;
}
Ticket PortalMailbox::push(const WebAction& action,uint32_t nowMs) {
  // Caller holds the lock and has already reserved capacity with room().
  WebResult* slot=nullptr;
  for(auto& r:results_) if(!r.id.sequence) {slot=&r;break;}
  if(!slot) for(auto& r:results_) if(!active(r.state) && (!slot || r.id.sequence<slot->id.sequence)) slot=&r;
  const Ticket id{boot_,++next_};*slot={};slot->id=id;slot->kind=action.kind;slot->createdMs=nowMs;slot->intent=action.intent;
  auto& queued=queue_[(head_+count_)%queue_.size()];queued=action;queued.id=id;queued.admittedMs=nowMs;++count_;return id;
}
std::optional<WebAction> PortalMailbox::pop() {
  if(!count_) return std::nullopt;
  const auto action=queue_[head_];head_=(head_+1)%queue_.size();--count_;
  for(auto& r:results_) if(r.id==action.id) r.state=WebState::Running;
  return action;
}
void PortalMailbox::put(const WebResult& result) { Guard guard(critical_);for(auto& r:results_) if(r.id==result.id) {r=result;return;} }
std::optional<WebResult> PortalMailbox::lookup(Ticket id) const { Guard guard(critical_);for(const auto& r:results_)if(r.id.sequence && r.id==id)return r;return std::nullopt; }
std::array<WebResult,16> PortalMailbox::results() const { Guard guard(critical_);return results_; }
std::optional<WebResult> PortalMailbox::resultAt(size_t index)const{Guard guard(critical_);return index<results_.size() && results_[index].id.sequence?std::optional<WebResult>(results_[index]):std::nullopt;}
void PortalMailbox::publish(const WebSnapshot& snapshot) { Guard guard(critical_);snapshot_=snapshot;snapshot_.boot=boot_; }
WebSnapshot PortalMailbox::snapshot() const { Guard guard(critical_);return snapshot_; }
std::optional<WebAction> PortalMailbox::take(){Guard guard(critical_);return pop();}
IngressEnvelope PortalMailbox::receive(uint32_t nowMs){
  Guard guard(critical_);IngressEnvelope input;input.action=pop();input.frontierMs=count_?queue_[head_].admittedMs:now_?now_():nowMs;
  input.loss=loss_;input.lossFirstMs=lossFirstMs_;input.lossLastMs=lossLastMs_;loss_=false;return input;
}
void PortalMailbox::markLoss(uint32_t nowMs){if(!loss_)lossFirstMs_=nowMs;lossLastMs_=nowMs;loss_=true;}
void PortalMailbox::rejectAutomation(uint32_t nowMs){Guard guard(critical_);markLoss(now_?now_():nowMs);}
void PortalMailbox::reportHealth(AutomationHealth health,uint32_t observedMs,std::optional<uint32_t> age){
  Guard guard(critical_);const auto now=now_?now_():observedMs;
  if(age){const uint64_t elapsed=uint64_t(*age)+static_cast<uint32_t>(now-observedMs);age=static_cast<uint32_t>(std::min<uint64_t>(elapsed,std::numeric_limits<uint32_t>::max()));}
  if(health==AutomationHealth::Healthy){if(!age)health=AutomationHealth::Unknown;else if(*age>180000)health=AutomationHealth::Expired;}
  evidence_={health,now,age};
}
AutomationEvidence PortalMailbox::healthEvidence()const{Guard guard(critical_);return evidence_;}
void PortalMailbox::publish(const WebSnapshot& snapshot,const DailyReplay& daily){
  // The caller owns daily on this worker. Under the lock this only compares
  // and copies bounded POD; no clock, NVS, callback or allocation is invoked.
  Guard guard(critical_);const auto& state=daily.state();const auto today=daily.today();const auto last=daily.lastClosed();
  bool sampleChanged=sample_.valid!=state.sample.valid || sample_.day!=state.sample.day || sample_.count!=state.sample.count;
  for(uint8_t n=0;n<state.sample.count && !sampleChanged;++n)sampleChanged=!same(sample_.actions[n],state.sample.actions[n]);
  if(sampleChanged){sample_=state.sample;++sampleRevision_;}
  bool todayChanged=candidate_.day!=today.day || candidate_.gap!=today.gap || candidate_.count!=today.count;
  for(uint8_t n=0;n<today.count;++n){const auto row=*daily.record(n,false);if(!same(records_[n].action,row.action) || records_[n].outcome!=row.outcome)todayChanged=true;records_[n]=row;}
  candidate_=today;if(todayChanged)++todayRevision_;
  snapshot_=snapshot;snapshot_.boot=boot_;snapshot_.generation=state.generation;auto& view=snapshot_.backup;
  view.enabled=state.enabled;view.sampleValid=state.sample.valid;view.sampleDay=state.sample.day;view.sampleCount=state.sample.count;view.sampleRevision=sampleRevision_;
  view.todayDay=today.day;view.todayCount=today.count;view.todayGap=today.gap;view.todayRevision=todayRevision_;
  view.hasLastDay=last.has_value();view.lastDay=last?last->day:0;view.lastCount=last?last->count:0;view.lastGap=last?last->gap:DayGap::PartialStart;
  if(!daily.storageHealthy())snapshot_.storageError=Error::PersistenceFailed;
}
DailyPage PortalMailbox::page(bool sample,uint8_t offset,uint32_t revision)const{
  Guard guard(critical_);DailyPage result;result.boot=boot_;result.sample=sample;result.offset=offset;
  result.revision=sample?sampleRevision_:todayRevision_;result.day=sample?sample_.day:candidate_.day;result.total=sample?sample_.count:candidate_.count;result.gap=sample?DayGap::None:candidate_.gap;
  if(revision!=result.revision){result.state=PageState::Changed;return result;}
  if((sample?!sample_.valid:!candidate_.day) || offset%8 || offset>=64 || (offset>=result.total && offset!=0))return result;
  result.state=PageState::Ok;result.count=std::min<uint8_t>(8,result.total-offset);
  for(uint8_t n=0;n<result.count;++n)result.rows[n]=sample?DailyRecord{sample_.actions[offset+n],DailyOutcome::Confirmed}:records_[offset+n];
  return result;
}

}
