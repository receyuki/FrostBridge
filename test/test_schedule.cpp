#include "WeeklySchedule.h"
#include "ScheduleStorage.h"
#include <cassert>
#include <iostream>
using namespace frost;
Plan fridayPlan() { return {true,{{0x10,1080,0,4,alpicool::Mode::Max}}}; }
void validates_weekly_intervals() {
  WeeklySchedule schedule([](const ScheduleState&) { return true; });
  assert(schedule.preview(fridayPlan(),0).error==PlanError::None);
  auto overlapping=fridayPlan();overlapping.periods.push_back({0x10,1140,1200,5,alpicool::Mode::Eco});
  assert(schedule.preview(overlapping,0).error==PlanError::Overlap);
  auto adjacent=fridayPlan();adjacent.periods.push_back({0x20,0,120,5,alpicool::Mode::Eco});
  assert(schedule.preview(adjacent,0).error==PlanError::None);
  auto invalid=fridayPlan();invalid.periods[0].endMinute=1080;
  assert(schedule.preview(invalid,0).error==PlanError::InvalidTime);
  invalid=fridayPlan();invalid.periods[0].mode.reset();assert(schedule.preview(invalid,0).error==PlanError::InvalidMode);
  invalid=fridayPlan();invalid.periods.resize(17,invalid.periods[0]);assert(schedule.preview(invalid,0).error==PlanError::TooMany);
}
void follows_current_period_with_trusted_time() {
  WeeklySchedule s([](const ScheduleState&) { return true; });
  assert(s.apply(s.preview(fridayPlan(),0),0)==PlanError::None);
  assert(s.decision(10).kind==DecisionKind::WaitingForTime);
  assert(s.sync(1793959199,100)); // Friday 17:59:59 Beijing, independent datetime literal.
  assert(s.decision(100).kind==DecisionKind::Off);
  auto start=s.decision(1100);assert(start.kind==DecisionKind::Cooling && start.targetC==4 && start.mode==alpicool::Mode::Max);
  // No network input: monotonic time alone reaches midnight.
  assert(s.decision(21601100).kind==DecisionKind::Off);
  assert(s.sync(1793959200+3600,21601101)); // Recover in Friday's current interval.
  assert(s.decision(21601101).kind==DecisionKind::Cooling);
  Plan sunday{true,{{0x40,1080,120,4,alpicool::Mode::Eco}}};
  assert(s.apply(s.preview(sunday,21601101),21601101)==PlanError::None);
  assert(s.sync(1794153600,21601102)); // Monday midnight, belongs to Sunday.
  assert(s.decision(21601102).kind==DecisionKind::Cooling);
}
void manual_override_survives_reboot_until_boundary() {
  WeeklySchedule s([](const ScheduleState&) { return true; });
  s.sync(1793962800,0);s.apply(s.preview(fridayPlan(),0),0); // Friday 19:00.
  assert(s.manual(1)==PlanError::None);
  assert(s.state().overrideUntil==1793980800);
  assert(s.decision(2).kind==DecisionKind::ManualOverride);
  WeeklySchedule reboot([](const ScheduleState&) { return true; });
  assert(reboot.restore(s.state()));assert(reboot.decision(3).kind==DecisionKind::WaitingForTime);
  reboot.sync(1793966400,4);assert(reboot.decision(4).kind==DecisionKind::ManualOverride);
  assert(reboot.decision(14400004).kind==DecisionKind::Off);
  assert(!reboot.state().manual);
  // Clock correction backwards: the next new boundary must still end override.
  s.sync(1793955600,3);assert(s.state().overrideUntil==1793959200); // Friday 17:00 -> 18:00.
  assert(s.decision(3600003).kind==DecisionKind::Cooling);
}
void unknown_time_does_not_guess_override_expiry() {
  WeeklySchedule s([](const ScheduleState&) { return true; });
  s.apply(s.preview(fridayPlan(),0),0);assert(s.manual(1)==PlanError::None);
  assert(!s.state().overrideUntil);assert(s.decision(100).kind==DecisionKind::WaitingForTime);
  s.sync(1793966400,101);assert(s.decision(101).kind==DecisionKind::ManualOverride);
  assert(s.state().overrideUntil==1793980800);
}
void preview_and_save_are_transactional() {
  bool writable=true;WeeklySchedule s([&](const ScheduleState&) { return writable; });
  s.sync(1793959199,0);const auto before=s.preview(fridayPlan(),0);
  assert(s.apply(before,1000)==PlanError::StalePreview);assert(!s.state().plan.enabled);
  s.apply(s.preview(fridayPlan(),1000),1000);s.manual(1001);
  auto disabled=s.state().plan;disabled.enabled=false;
  assert(s.apply(s.preview(disabled,1002),1002)==PlanError::None);
  assert(s.decision(1003).kind==DecisionKind::Hold); // no implicit off.
  assert(s.apply(s.preview(fridayPlan(),1004),1004)==PlanError::None);
  assert(!s.state().manual && s.decision(1004).kind==DecisionKind::Cooling);
  writable=false;assert(s.manual(1005)==PlanError::PersistenceFailed);assert(!s.state().manual);
  assert(s.apply(s.preview(disabled,1006),1006)==PlanError::PersistenceFailed);assert(s.state().plan.enabled);
  writable=true;const auto old=s.preview(disabled,1007);s.manual(1008);
  assert(s.apply(old,1009)==PlanError::StalePreview);
  assert(!s.sync(0,1010));
}
void serializes_only_plan_and_override_with_corruption_checks() {
  const auto empty=encodeScheduleState(ScheduleState{});assert(empty && empty->size()==22);
  // Independent Python zlib CRC32 of the published 18-byte empty-state body.
  assert((*empty)[18]==0xe5 && (*empty)[19]==0x51 && (*empty)[20]==0xfa && (*empty)[21]==0x02);
  WeeklySchedule s([](const ScheduleState&) { return true; });
  s.sync(1793962800,0);s.apply(s.preview(fridayPlan(),0),0);s.manual(1);
  auto bytes=encodeScheduleState(s.state());assert(bytes && bytes->size()<=134);
  auto restored=decodeScheduleState(*bytes);assert(restored && restored->plan.enabled && restored->manual && restored->overrideUntil==1793980800);
  assert(restored->plan.periods.size()==1 && restored->plan.periods[0].days==0x10 && restored->plan.periods[0].mode==alpicool::Mode::Max);
  WeeklySchedule reboot([](const ScheduleState&) { return true; });assert(reboot.restore(*restored));
  assert(reboot.decision(1).kind==DecisionKind::WaitingForTime);
  (*bytes)[12]^=1;assert(!decodeScheduleState(*bytes));bytes->pop_back();assert(!decodeScheduleState(*bytes));
  ScheduleState maximum;maximum.plan.enabled=true;
  for(unsigned i=0;i<16;++i) maximum.plan.periods.push_back({1,static_cast<uint16_t>(i*60),static_cast<uint16_t>(i*60+30),-20+int(i),alpicool::Mode::Eco});
  const auto maxBytes=encodeScheduleState(maximum);assert(maxBytes && maxBytes->size()==134);
  assert(decodeScheduleState(*maxBytes)->plan.periods.size()==16);
}
void preview_cannot_be_edited_or_replayed() {
  WeeklySchedule s([](const ScheduleState&) { return true; });
  s.sync(1793955600,0);
  auto preview=s.preview(fridayPlan(),0);
  // This extra Tuesday period does not alter Friday's immediate preview effect.
  preview.plan.periods.push_back({0x02,600,660,8,alpicool::Mode::Eco});
  assert(s.apply(preview,0)==PlanError::StalePreview);
  const auto real=s.preview(fridayPlan(),0);assert(s.apply(real,0)==PlanError::None);
  assert(s.apply(real,0)==PlanError::StalePreview);
}
void adjacent_periods_and_monotonic_wrap() {
  WeeklySchedule s([](const ScheduleState&) { return true; });
  Plan plan{true,{{0x10,1080,1200,4,alpicool::Mode::Max},{0x10,1200,0,5,alpicool::Mode::Eco}}};
  s.sync(1793966399,0xffffff00);s.apply(s.preview(plan,0xffffff00),0xffffff00);s.manual(0xffffff01);
  const auto adjacent=s.decision(static_cast<uint32_t>(0xffffff00u+1000u));
  assert(adjacent.kind==DecisionKind::Cooling && adjacent.targetC==5 && adjacent.mode==alpicool::Mode::Eco);
  WeeklySchedule offline([](const ScheduleState&) { return true; });
  uint32_t mono=100;offline.sync(1793959199,mono);offline.apply(offline.preview(fridayPlan(),mono),mono);
  for(unsigned i=0;i<60;++i) { mono+=86400000;offline.decision(mono); }
  assert(offline.decision(mono).nextBoundary==1799402400);
  auto overlap=fridayPlan();overlap.periods={{0x40,1320,240,4,alpicool::Mode::Max},{0x01,120,300,5,alpicool::Mode::Eco}};
  assert(offline.preview(overlap,mono).error==PlanError::Overlap);
}
int main() { validates_weekly_intervals();follows_current_period_with_trusted_time();manual_override_survives_reboot_until_boundary();unknown_time_does_not_guess_override_expiry();preview_and_save_are_transactional();serializes_only_plan_and_override_with_corruption_checks();preview_cannot_be_edited_or_replayed();adjacent_periods_and_monotonic_wrap();std::cout<<"PASS: weekly boundaries, manual override, preview/save, storage integrity, 60 offline days and clock wrap\n"; }
