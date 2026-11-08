#include "WeeklySchedule.h"
#include <algorithm>
#include <utility>
namespace frost {
namespace {
constexpr int weekMinutes=7*1440;
constexpr int64_t weekSeconds=7*86400;
constexpr int64_t earliestTime=1577836800,latestTime=4102444800;
using Segment=std::pair<int,int>;
std::vector<Segment> segments(const Plan& plan) {
  std::vector<Segment> result;
  for(const auto& p:plan.periods) for(int day=0;day<7;++day) if(p.days&(1<<day)) {
    const int start=day*1440+p.startMinute;
    const int end=day*1440+p.endMinute+(p.endMinute<=p.startMinute?1440:0);
    result.emplace_back(start,std::min(end,weekMinutes));
    if(end>weekMinutes) result.emplace_back(0,end-weekMinutes);
  }
  return result;
}
}
WeeklySchedule::WeeklySchedule(Save save):save_(std::move(save)) {}
PlanError WeeklySchedule::validate(const Plan& plan) {
  if(plan.periods.size()>16) return PlanError::TooMany;
  for(const auto& p:plan.periods) {
    if(!p.days || (p.days&0x80)) return PlanError::InvalidDays;
    if(p.startMinute>=1440 || p.endMinute>=1440 || p.startMinute==p.endMinute) return PlanError::InvalidTime;
    if(p.targetC < -20 || p.targetC > 20) return PlanError::InvalidTarget;
    if(!p.mode || (*p.mode!=alpicool::Mode::Max && *p.mode!=alpicool::Mode::Eco)) return PlanError::InvalidMode;
  }
  auto ranges=segments(plan);std::sort(ranges.begin(),ranges.end());
  for(size_t i=1;i<ranges.size();++i) if(ranges[i].first<ranges[i-1].second) return PlanError::Overlap;
  return PlanError::None;
}
void WeeklySchedule::advance(uint32_t nowMs) {
  if(trusted_) {
    const uint64_t elapsed=static_cast<uint32_t>(nowMs-lastMono_)+static_cast<uint64_t>(remainder_);
    wall_+=elapsed/1000;remainder_=elapsed%1000;
    if(wall_>latestTime) trusted_=false;
  }
  lastMono_=nowMs;
}
bool WeeklySchedule::sync(int64_t seconds,uint32_t nowMs) {
  if(seconds<earliestTime || seconds>latestTime) return false;
  advance(nowMs);trusted_=true;wall_=seconds;remainder_=0;++state_.generation;
  const auto next=effect(state_.plan).nextBoundary;
  if(state_.manual && state_.overrideUntil && *state_.overrideUntil>wall_ && next && *next<*state_.overrideUntil) {
    auto candidate=state_;candidate.overrideUntil=next;++candidate.generation;
    if(!commit(std::move(candidate))) { trusted_=false;return false; }
  }
  return true;
}
Decision WeeklySchedule::effect(const Plan& plan) const {
  if(!plan.enabled) return {};
  if(!trusted_) return {DecisionKind::WaitingForTime,{},{},{}};
  const int64_t local=wall_+8*3600;
  const int64_t position=((local/86400+3)%7)*86400+local%86400;
  const int64_t base=wall_-position;
  Decision d{DecisionKind::Off,{},{},{}};
  for(const auto& p:plan.periods) for(int day=0;day<7;++day) if(p.days&(1<<day)) {
    const int64_t start=(day*1440+p.startMinute)*60;
    const int64_t end=(day*1440+p.endMinute+(p.endMinute<=p.startMinute?1440:0))*60;
    if((position>=start && position<end) || (position+weekSeconds>=start && position+weekSeconds<end)) {
      d.kind=DecisionKind::Cooling;d.targetC=p.targetC;d.mode=p.mode;
    }
    for(const auto boundary:{start,end}) {
      auto candidate=base+boundary%weekSeconds;
      if(candidate<=wall_) candidate+=weekSeconds;
      if(!d.nextBoundary || candidate<*d.nextBoundary) d.nextBoundary=candidate;
    }
  }
  return d;
}
PlanPreview WeeklySchedule::preview(const Plan& plan,uint32_t nowMs) {
  advance(nowMs);
  const auto error=validate(plan);
  PlanPreview preview{error==PlanError::None?plan:Plan{},error,state_.generation,error==PlanError::None?effect(plan):Decision{},++nextPreview_};
  if(error==PlanError::None) pendingPreview_=preview;else pendingPreview_.reset();
  return preview;
}
PlanError WeeklySchedule::apply(const PlanPreview& preview,uint32_t nowMs) {
  advance(nowMs);
  const auto error=validate(preview.plan);
  if(preview.error!=PlanError::None) return preview.error;
  if(error!=PlanError::None) return error;
  if(!pendingPreview_ || preview.token!=pendingPreview_->token || !(preview.plan==pendingPreview_->plan) ||
     preview.generation!=pendingPreview_->generation || !(preview.effect==pendingPreview_->effect)) return PlanError::StalePreview;
  if(preview.generation!=state_.generation || !(preview.effect==effect(preview.plan))) return PlanError::StalePreview;
  auto candidate=state_;candidate.plan=preview.plan;++candidate.generation;
  if(candidate.plan.enabled) { candidate.manual=false;candidate.overrideUntil.reset(); }
  if(!commit(std::move(candidate))) return PlanError::PersistenceFailed;
  pendingPreview_.reset();return PlanError::None;
}
Decision WeeklySchedule::decision(uint32_t nowMs) {
  advance(nowMs);auto d=effect(state_.plan);
  if(!state_.plan.enabled || !trusted_ || !state_.manual) return d;
  if(!state_.overrideUntil && d.nextBoundary && (!retryPending_ || static_cast<int32_t>(nowMs-retryAfter_)>=0)) {
    auto candidate=state_;candidate.overrideUntil=d.nextBoundary;++candidate.generation;
    retryPending_=!commit(std::move(candidate));retryAfter_=nowMs+5000;
  }
  if(state_.overrideUntil && wall_>=*state_.overrideUntil) {
    auto expired=state_;expired.manual=false;expired.overrideUntil.reset();++expired.generation;
    // Expiry is a fact of trusted time. A saved expired marker is also expired
    // after the next boot/sync; retaining it on disk cannot replay a command.
    if(!commit(expired)) state_=std::move(expired);
    return d;
  }
  d.kind=DecisionKind::ManualOverride;d.targetC.reset();d.mode.reset();d.nextBoundary=state_.overrideUntil;return d;
}
bool WeeklySchedule::commit(ScheduleState candidate) {
  if(!save_ || !save_(candidate)) { error_=PlanError::PersistenceFailed;return false; }
  state_=std::move(candidate);error_=PlanError::None;retryPending_=false;return true;
}
PlanError WeeklySchedule::manual(uint32_t nowMs,bool alreadyApplied) {
  advance(nowMs);auto candidate=state_;candidate.manual=true;candidate.overrideUntil=effect(state_.plan).nextBoundary;++candidate.generation;
  if(commit(candidate)) return PlanError::None;
  if(alreadyApplied) state_=std::move(candidate); // Respect observed manual change even if persistence failed.
  return PlanError::PersistenceFailed;
}
bool WeeklySchedule::restore(const ScheduleState& state) {
  if(!validState(state)) return false;
  state_=state;trusted_=false;remainder_=0;retryPending_=false;error_=PlanError::None;pendingPreview_.reset();return true;
}
bool WeeklySchedule::validState(const ScheduleState& state) {
  return validate(state.plan)==PlanError::None && (!state.overrideUntil ||
    (state.manual && *state.overrideUntil>=earliestTime && *state.overrideUntil<=latestTime+weekSeconds));
}
}
