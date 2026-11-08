#pragma once
#include "AlpicoolProtocol.h"
#include <functional>
#include <optional>
#include <vector>
namespace frost {
struct Period {
  uint8_t days=0; // Monday bit 0 through Sunday bit 6.
  uint16_t startMinute=0,endMinute=0;
  int targetC=4;
  std::optional<alpicool::Mode> mode;
};
struct Plan { bool enabled=false;std::vector<Period> periods; };
inline bool operator==(const Plan& a,const Plan& b) {
  if(a.enabled!=b.enabled || a.periods.size()!=b.periods.size()) return false;
  for(size_t i=0;i<a.periods.size();++i) {
    const auto& p=a.periods[i];const auto& q=b.periods[i];
    if(p.days!=q.days || p.startMinute!=q.startMinute || p.endMinute!=q.endMinute || p.targetC!=q.targetC || p.mode!=q.mode) return false;
  }
  return true;
}
struct ScheduleState { Plan plan;bool manual=false;std::optional<int64_t> overrideUntil;uint32_t generation=0; };
enum class PlanError { None,TooMany,InvalidDays,InvalidTime,InvalidTarget,InvalidMode,Overlap,StalePreview,PersistenceFailed };
enum class DecisionKind { Hold,WaitingForTime,ManualOverride,Cooling,Off };
struct Decision {
  DecisionKind kind=DecisionKind::Hold;
  std::optional<int> targetC;
  std::optional<alpicool::Mode> mode;
  std::optional<int64_t> nextBoundary;
};
inline bool operator==(const Decision& a,const Decision& b) {
  return a.kind==b.kind && a.targetC==b.targetC && a.mode==b.mode && a.nextBoundary==b.nextBoundary;
}
struct PlanPreview { Plan plan;PlanError error=PlanError::None;uint32_t generation=0;Decision effect;uint32_t token=0; };
class WeeklySchedule {
 public:
  using Save=std::function<bool(const ScheduleState&)>;
  explicit WeeklySchedule(Save save);
  PlanPreview preview(const Plan& plan,uint32_t nowMs);
  PlanError apply(const PlanPreview& preview,uint32_t nowMs);
  bool sync(int64_t unixSeconds,uint32_t nowMs);
  Decision decision(uint32_t nowMs);
  PlanError manual(uint32_t nowMs,bool alreadyApplied=false);
  bool restore(const ScheduleState& state);
  PlanError lastError() const { return error_; }
  bool timeTrusted() const { return trusted_; }
  std::optional<int64_t> unixTime(uint32_t nowMs) { advance(nowMs);return trusted_?std::optional<int64_t>(wall_):std::nullopt; }
  const ScheduleState& state() const { return state_; }
  static PlanError validate(const Plan& plan);
  static bool validState(const ScheduleState& state);
 private:
  Save save_;
  ScheduleState state_;
  bool trusted_=false;
  int64_t wall_=0;
  uint32_t lastMono_=0;
  uint16_t remainder_=0;
  PlanError error_=PlanError::None;
  bool retryPending_=false;
  uint32_t retryAfter_=0;
  uint32_t nextPreview_=0;
  std::optional<PlanPreview> pendingPreview_;
  void advance(uint32_t nowMs);
  Decision effect(const Plan& plan) const;
  bool commit(ScheduleState candidate);
};
}
