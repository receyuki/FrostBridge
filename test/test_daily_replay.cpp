#include "DailyReplay.h"
#include "DailyStorage.h"
#include "DailyJournal.h"
#include <cassert>
#include <iostream>
#include <algorithm>
using namespace frost;
void confirmed_automation_only(){
 constexpr int64_t midnight=1794067200000LL;unsigned saves=0;
 DailyReplay replay([&](const DailyPersistedState&){++saves;return true;});DailyInputs in{0,midnight-1000,AutomationHealth::Healthy,true};assert(replay.configure(true,in));in.nowMs=1000;in.wallMs=midnight;replay.advance(in);
 for(unsigned second=1;second<=86400;++second){
  in.nowMs=(second+1)*1000;in.wallMs=midnight+int64_t(second)*1000;
  if(second==12*3600){
   assert(!replay.capture({alpicool::Setting::TargetC,10,Source::Web},in));
   const auto target=replay.capture({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},in);assert(target);
   const auto power=replay.capture({alpicool::Setting::Power,1,Source::HomeKitAutomation},in);assert(power);
   assert(replay.complete(*target,true) && replay.complete(*power,true));
  }
  replay.advance(in);
 }
 const auto& sample=replay.state().sample;assert(sample.valid && sample.day==20765 && sample.count==2);
 assert(sample.actions[0].second==43200 && sample.actions[0].field==DailyField::Temperature && sample.actions[0].value==4);
 assert(sample.actions[1].field==DailyField::Power && sample.actions[1].value==1 && sample.actions[0].order<sample.actions[1].order);
 assert(saves==2);std::cout<<"PASS: only confirmed automation fields seal, preserving same-second order"<<std::endl;
}
void failed_day_retains_sample_and_reason(){
 constexpr int64_t midnight=1794067200000LL;DailyReplay replay([](const DailyPersistedState&){return true;});
 DailyInputs in{0,midnight-1000,AutomationHealth::Healthy,true};assert(replay.configure(true,in));in.nowMs=1000;in.wallMs=midnight;replay.advance(in);
 for(unsigned second=1;second<=2*86400;++second){
  in.nowMs=(second+1)*1000;in.wallMs=midnight+int64_t(second)*1000;
  if(second==3600 || second==86400+3600){const auto t=replay.capture({alpicool::Setting::TargetC,second==3600?4:9,Source::HomeKitAutomation},in);assert(t);assert(replay.complete(*t,second==3600));}
  replay.advance(in);
 }
 assert(replay.state().sample.valid && replay.state().sample.day==20765 && replay.state().sample.actions[0].value==4);
 const auto closed=replay.lastClosed();assert(closed && closed->day==20766 && closed->gap==DayGap::Failed);
 std::cout<<"PASS: failed day preserves previous sample and exposes why it was not replaced"<<std::endl;
}
void restore_keeps_sample_but_not_time_coverage(){
 DailyPersistedState stored;stored.enabled=true;stored.generation=7;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=1;stored.sample.actions[0]={64800,DailyField::Temperature,-5,0};
 DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(stored));
 DailyInputs in{0,1794067200000LL+3600000,AutomationHealth::Healthy,true};replay.advance(in);
 assert(replay.state().sample.day==20764 && replay.record(0,true)->action.value==-5 && replay.today().gap==DayGap::PartialStart);
 auto bad=stored;bad.sample.count=65;assert(!replay.restore(bad));assert(replay.state().sample.count==1);
 bad=stored;bad.sample.actions[0].value=-21;assert(!replay.restore(bad));assert(replay.state().sample.actions[0].value==-5);
 std::cout<<"PASS: persisted sample survives; reboot cannot restore trusted time/day coverage; invalid state rejected"<<std::endl;
}
void storage_matches_portable_fixture_and_rejects_corruption(){
 DailyPersistedState state;state.enabled=true;state.generation=7;state.sample.valid=true;state.sample.day=20764;state.sample.count=1;state.sample.actions[0]={64800,DailyField::Temperature,-5,0};
 DailyBytes bytes{};const auto size=encodeDailyState(state,bytes);assert(size && *size==76);
 assert(bytes[0]=='F' && bytes[1]=='B' && bytes[2]=='D' && bytes[3]==1 && bytes[4]==3 && bytes[5]==1 && bytes[8]==7 && bytes[12]==0x1c && bytes[13]==0x51);
 assert(bytes[64]==0x20 && bytes[65]==0xfd && bytes[68]==0 && bytes[69]==0xfb);
 // Independently calculated once using Python zlib.crc32 over the explicit fixture.
 assert(bytes[72]==0xc6 && bytes[73]==0xf6 && bytes[74]==0x6d && bytes[75]==0x0f);
 DailyPersistedState decoded;assert(decodeDailyState(bytes.data(),*size,decoded));assert(decoded.generation==7 && decoded.sample.actions[0].value==-5);
 bytes[69]^=1;assert(!decodeDailyState(bytes.data(),*size,decoded));assert(decoded.sample.actions[0].value==-5);
 assert(!decodeDailyState(nullptr,0,decoded));assert(!decodeDailyState(bytes.data(),*size-1,decoded));
 state.sample.count=64;for(unsigned n=0;n<64;++n)state.sample.actions[n]={n*60,DailyField::Temperature,static_cast<int8_t>(int(n%41)-20),static_cast<uint16_t>(n)};
 const auto full=encodeDailyState(state,bytes);assert(full && *full==580 && decodeDailyState(bytes.data(),*full,decoded) && decoded.sample.count==64);
 state.sample.count=65;assert(!encodeDailyState(state,bytes));
 std::cout<<"PASS: portable bounded FBD1 fixture/CRC, maximum sample and no partial restore on corrupt input"<<std::endl;
}
void journal_survives_torn_update_without_losing_previous_sample(){
 std::array<DailyBytes,2> slots{};std::array<std::optional<size_t>,2> lengths{};bool tear=false;
 auto read=[&](uint8_t slot,DailyBytes& out)->std::optional<size_t>{if(!lengths[slot])return std::nullopt;out=slots[slot];return lengths[slot];};
 auto write=[&](uint8_t slot,const DailyBytes& bytes,size_t size){lengths[slot]=size;if(tear){std::copy_n(bytes.begin(),5,slots[slot].begin());return false;}slots[slot]=bytes;return true;};
 DailyJournal journal(read,write);DailyReplay replay([](const DailyPersistedState&){return true;});assert(journal.load(replay)==DailyLoad::Empty);
 DailyPersistedState stored;stored.enabled=true;stored.generation=7;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=1;stored.sample.actions[0]={64800,DailyField::Temperature,-5,0};
 assert(journal.save(stored));auto update=stored;update.generation=8;update.sample.actions[0].value=10;tear=true;assert(!journal.save(update));
 DailyJournal reopened(read,write);assert(reopened.load(replay)==DailyLoad::Loaded);assert(replay.state().generation==7 && replay.state().sample.actions[0].value==-5);
 tear=false;update.generation=9;assert(reopened.save(update));DailyJournal latest(read,write);assert(latest.load(replay)==DailyLoad::Loaded && replay.state().generation==9 && replay.state().sample.actions[0].value==10);
 slots[1][69]^=1;DailyJournal fallback(read,write);assert(fallback.load(replay)==DailyLoad::Loaded && replay.state().generation==7);
 std::cout<<"PASS: two-slot journal retains prior valid sample across torn/invalid newer slot"<<std::endl;
}
void replay_projects_current_cycle_without_replaying_missed_actions(){
 constexpr int64_t midnight=1794067200000LL;DailyPersistedState stored;stored.enabled=true;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=4;
 stored.sample.actions[0]={0,DailyField::Power,0,0};stored.sample.actions[1]={64800,DailyField::Temperature,4,1};stored.sample.actions[2]={64800,DailyField::Mode,1,2};stored.sample.actions[3]={64800,DailyField::Power,1,3};
 DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(stored));
 DailyInputs in{200000,midnight+12*3600000,AutomationHealth::Expired,false};auto d=replay.decision(in);
 assert(d.mode==ReplayMode::Replaying && d.mask==7 && d.values[0]==4 && d.values[1]==0 && d.values[2]==1 && d.nextActionMs==midnight+18*3600000);
 in.wallMs=midnight+19*3600000;d=replay.decision(in);assert(d.values[1]==1 && d.nextActionMs==midnight+86400000);
 in.link=AutomationHealth::Healthy;in.networkReady=true;d=replay.decision(in);assert(d.mode==ReplayMode::HomeKitPrimary && !d.mask);
 in.link=AutomationHealth::Unknown;in.nowMs=10000;assert(replay.decision(in).mode==ReplayMode::CheckingLink);
 in.nowMs=200000;assert(replay.decision(in).mode==ReplayMode::Replaying);in.wallMs.reset();assert(replay.decision(in).mode==ReplayMode::WaitingForTime);
 stored.sample.count=0;assert(replay.restore(stored));in.wallMs=midnight+19*3600000;d=replay.decision(in);assert(d.mode==ReplayMode::Replaying && !d.mask && !d.nextActionMs);
 std::cout<<"PASS: current cyclic profile, real-link lease and cold-start grace; empty sample never changes settings"<<std::endl;
}
void manual_override_survives_reboot_until_next_action(){
 constexpr int64_t midnight=1794067200000LL;DailyPersistedState stored;stored.enabled=true;stored.generation=7;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=2;
 stored.sample.actions[0]={0,DailyField::Power,0,0};stored.sample.actions[1]={64800,DailyField::Power,1,1};unsigned saves=0;
 DailyReplay replay([&](const DailyPersistedState&){++saves;return true;});assert(replay.restore(stored));DailyInputs in{200000,midnight+20*3600000,AutomationHealth::Expired,false};
 assert(replay.manualOverride(in));assert(replay.state().manual && replay.state().overrideUntilMs==midnight+86400000 && replay.decision(in).mode==ReplayMode::ManualOverride);
 DailyBytes bytes{};const auto size=encodeDailyState(replay.state(),bytes);assert(size);DailyReplay rebooted([](const DailyPersistedState&){return true;});assert(rebooted.restoreEncoded(bytes.data(),*size));
 in.nowMs=1000;in.wallMs.reset();assert(rebooted.decision(in).mode==ReplayMode::WaitingForTime);
 in.wallMs=midnight+21*3600000;assert(rebooted.decision(in).mode==ReplayMode::ManualOverride && !rebooted.decision(in).mask);
 in.wallMs=midnight+86400000;assert(rebooted.decision(in).mode==ReplayMode::Replaying && rebooted.decision(in).values[1]==0);assert(saves==1);
 std::cout<<"PASS: persistent manual override survives cold boot/time wait and ends at next sample action"<<std::endl;
}
void manual_without_time_gets_a_boundary_after_clock_returns(){
 constexpr int64_t midnight=1794067200000LL;DailyPersistedState stored;stored.enabled=true;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=1;stored.sample.actions[0]={64800,DailyField::Temperature,4,0};
 unsigned saves=0;bool disk=true;DailyReplay replay([&](const DailyPersistedState&){++saves;return disk;});assert(replay.restore(stored));DailyInputs in{10,std::nullopt,AutomationHealth::Expired,false};
 assert(replay.manualOverride(in) && replay.state().manual && !replay.state().overrideUntilMs);
 in.wallMs=midnight+12*3600000;in.nowMs=100;disk=false;replay.advance(in);assert(!replay.state().overrideUntilMs && replay.decision(in).mode==ReplayMode::ManualOverride);
 for(unsigned n=101;n<5100;++n){in.nowMs=n;in.wallMs=midnight+12*3600000+n-100;replay.advance(in);}assert(saves==2);
 disk=true;in.nowMs=5100;in.wallMs=midnight+12*3600000+5000;replay.advance(in);assert(saves==3 && replay.state().overrideUntilMs==midnight+18*3600000);
 in.wallMs=midnight+18*3600000;assert(replay.decision(in).mode==ReplayMode::Replaying);
 std::cout<<"PASS: unknown-time override acquires a persistent deadline with bounded storage retry"<<std::endl;
}
void network_loss_cannot_keep_homekit_primary(){
 DailyPersistedState state;state.enabled=true;state.sample.valid=true;state.sample.day=20764;state.sample.count=1;state.sample.actions[0]={0,DailyField::Temperature,4,0};DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(state));
 DailyInputs in{200000,1794067200000LL,AutomationHealth::Healthy,false};assert(replay.decision(in).mode==ReplayMode::Replaying);
 std::cout<<"PASS: Wi-Fi loss cannot preserve an otherwise healthy HomeKit lease"<<std::endl;
}
void bounded_records_and_midnight_pending_preserve_previous_sample(){
 constexpr int64_t midnight=1794067200000LL;
 for(bool overflow:{false,true}){
  DailyPersistedState old;old.enabled=true;old.sample.valid=true;old.sample.day=20764;old.sample.count=1;old.sample.actions[0]={0,DailyField::Temperature,3,0};
  DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(old));DailyInputs in{0,midnight,AutomationHealth::Healthy,true};replay.advance(in);
  if(overflow){for(unsigned n=0;n<64;++n){const auto t=replay.capture({alpicool::Setting::TargetC,3,Source::HomeKitAutomation},in);assert(t && replay.complete(*t,true));}assert(!replay.capture({alpicool::Setting::Power,1,Source::HomeKitAutomation},in));assert(replay.today().count==64 && !replay.record(64,false));}
  std::optional<LearningToken> pending;
  for(unsigned sec=1;sec<=86400;++sec){in.nowMs=sec*1000;in.wallMs=midnight+int64_t(sec)*1000;if(!overflow && sec==86399)pending=replay.capture({alpicool::Setting::TargetC,9,Source::HomeKitAutomation},in);replay.advance(in);}
  assert(replay.state().sample.day==20764 && replay.state().sample.actions[0].value==3);
  assert(replay.lastClosed()->gap==(overflow?DayGap::Overflow:DayGap::PendingAtBoundary));
  if(pending)assert(!replay.complete(*pending,true));
 }
 std::cout<<"PASS: record overflow and midnight pending actions never replace the valid sample"<<std::endl;
}
void only_confirmed_automation_ends_manual_override(){
 DailyPersistedState stored;stored.enabled=true;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=1;stored.sample.actions[0]={0,DailyField::Temperature,3,0};bool disk=true;
 DailyReplay replay([&](const DailyPersistedState&){return disk;});assert(replay.restore(stored));DailyInputs in{0,1794067200000LL+3600000,AutomationHealth::Healthy,true};
 assert(replay.manualOverride(in));auto token=replay.capture({alpicool::Setting::TargetC,5,Source::HomeKitAutomation},in);assert(token && replay.complete(*token,false) && replay.state().manual);
 token=replay.capture({alpicool::Setting::TargetC,3,Source::HomeKitAutomation},in);assert(token && replay.complete(*token,true) && !replay.state().manual);assert(!replay.complete(*token,true));
 disk=false;assert(!replay.manualOverride(in) && !replay.state().manual && !replay.storageHealthy());assert(!replay.manualOverride(in,true) && replay.state().manual && !replay.storageHealthy());
 disk=true;in.nowMs=5000;*in.wallMs+=5000;replay.advance(in);assert(replay.storageHealthy() && replay.state().manual);
 std::cout<<"PASS: confirmed automation ends overrides; persistence failure is visible and observed manual change stays respected"<<std::endl;
}
void older_confirmation_cannot_end_a_newer_manual_override(){
 DailyPersistedState state;state.enabled=true;DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(state));DailyInputs in{100,1794067200000LL+1000,AutomationHealth::Healthy,true};
 const auto older=replay.capture({alpicool::Setting::TargetC,3,Source::HomeKitAutomation},in);assert(older);assert(replay.manualOverride(in));assert(replay.complete(*older,true));assert(replay.state().manual);
 const auto newer=replay.capture({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},in);assert(newer && replay.complete(*newer,true));assert(!replay.state().manual);
 std::cout<<"PASS: a delayed older automation confirmation cannot release a newer manual override"<<std::endl;
}
void midnight_clock_correction_cannot_erase_confirmed_actions(){
 constexpr int64_t midnight=1794067200000LL;
 for(int correction:{1000,3000}){
  DailyReplay replay([](const DailyPersistedState&){return true;});DailyInputs in{1000,midnight+100,AutomationHealth::Healthy,true};assert(replay.configure(true,in));
  const auto t=replay.capture({alpicool::Setting::TargetC,3,Source::HomeKitAutomation},in);assert(t && replay.complete(*t,true));
  in.nowMs=1100;in.wallMs=midnight+200-correction;replay.advance(in);in.nowMs=1100+correction;in.wallMs=midnight+200;replay.advance(in);
  assert(replay.today().count==1 && replay.record(0,false)->action.value==3 && replay.today().gap!=DayGap::None);
  for(unsigned sec=1;sec<=86400;++sec){in.nowMs=1100+correction+sec*1000;in.wallMs=midnight+200+int64_t(sec)*1000;replay.advance(in);}assert(!replay.state().sample.valid);
 }
 std::cout<<"PASS: backward midnight corrections cannot discard actions then seal a false complete day"<<std::endl;
}
void cold_start_at_midnight_still_requires_the_next_complete_day(){
 constexpr int64_t midnight=1794067200000LL;DailyPersistedState stored;stored.enabled=true;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=1;stored.sample.actions[0]={0,DailyField::Temperature,3,0};DailyReplay replay([](const DailyPersistedState&){return true;});assert(replay.restore(stored));DailyInputs in{0,midnight,AutomationHealth::Healthy,true};replay.advance(in);
 for(unsigned sec=1;sec<=2*86400;++sec){in.nowMs=sec*1000;in.wallMs=midnight+int64_t(sec)*1000;replay.advance(in);if(sec==86400)assert(replay.state().sample.day==20764 && replay.lastClosed()->gap==DayGap::PartialStart);}
 assert(replay.state().sample.day==20766 && replay.state().sample.count==0);
 std::cout<<"PASS: an exact-midnight cold start cannot certify unseen startup coverage; the next complete empty day replaces it"<<std::endl;
}
void reconfirmed_config_overrides_an_uncertain_committed_write(){
 constexpr int64_t midnight=1794067200000LL;
 for(bool enabled:{false,true}){
  std::array<DailyBytes,2> slots{};std::array<std::optional<size_t>,2> sizes{};bool hideRead=false;
  auto read=[&](uint8_t n,DailyBytes& out)->std::optional<size_t>{if(hideRead || !sizes[n])return std::nullopt;out=slots[n];return sizes[n];};
  auto write=[&](uint8_t n,const DailyBytes& bytes,size_t size){slots[n]=bytes;sizes[n]=size;return true;};
  DailyJournal journal(read,write);DailyReplay daily([&](const DailyPersistedState& s){return journal.save(s);});assert(journal.load(daily)==DailyLoad::Empty);DailyInputs in{0,midnight-1000,AutomationHealth::Healthy,true};
  if(enabled){assert(daily.configure(true,in));in.nowMs=1000;in.wallMs=midnight;daily.advance(in);const auto token=daily.capture({alpicool::Setting::TargetC,3,Source::HomeKitAutomation},in);assert(token && daily.complete(*token,true));}
  hideRead=true;assert(!daily.configure(!enabled,in) && daily.state().enabled==enabled && !daily.storageHealthy()); // bytes were committed, but readback could not confirm them
  hideRead=false;assert(daily.configure(enabled,in) && daily.storageHealthy());assert(daily.today().count==(enabled?1:0));
  DailyJournal reopened(read,write);DailyReplay rebooted([](const DailyPersistedState&){return true;});assert(reopened.load(rebooted)==DailyLoad::Loaded && rebooted.state().enabled==enabled);
 }
 std::cout<<"PASS: an explicit reconfirmation overrides uncertain flash state and keeps ongoing day records"<<std::endl;
}
void unavailable_midnight_boundary_explains_why_the_old_sample_was_kept(){
 constexpr int64_t midnight=1794067200000LL;
 for(bool deviceGap:{false,true}){
  DailyPersistedState stored;stored.enabled=true;stored.sample.valid=true;stored.sample.day=20764;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(stored));DailyInputs in{0,midnight-1000,AutomationHealth::Healthy,true};daily.advance(in);in.nowMs=1000;in.wallMs=midnight;daily.advance(in);
  for(unsigned sec=1;sec<=86400;++sec){in.nowMs=(sec+1)*1000;in.wallMs=midnight+int64_t(sec)*1000;if(sec==86400){if(deviceGap)in.deviceReady=false;else in.networkReady=false;}daily.advance(in);}
  assert(daily.state().sample.day==20764 && daily.lastClosed()->gap==(deviceGap?DayGap::DeviceUnavailable:DayGap::LinkUnavailable));
 }
 std::cout<<"PASS: an unavailable midnight boundary cannot leave an unsealed old day labelled complete"<<std::endl;
}
void a_gap_recovering_just_after_midnight_cannot_certify_the_new_day(){
 constexpr int64_t midnight=1794067200000LL;
 for(bool deviceGap:{false,true}){
  DailyReplay replay([](const DailyPersistedState&){return true;});DailyInputs in{0,midnight-1000,AutomationHealth::Healthy,true};assert(replay.configure(true,in));
  in.nowMs=980;in.wallMs=midnight-20;in.networkReady=deviceGap;in.deviceReady=!deviceGap;replay.advance(in);
  in.nowMs=1020;in.wallMs=midnight+20;in.networkReady=true;in.deviceReady=true;replay.advance(in);assert(replay.today().gap!=DayGap::None);
  // Recovery well before the following midnight must permit that next day.
  in.nowMs=2000;in.wallMs=midnight+1000;replay.advance(in);
  for(unsigned second=2;second<=86400;++second){in.nowMs=(second+1)*1000;in.wallMs=midnight+int64_t(second)*1000;replay.advance(in);}assert(!replay.state().sample.valid && replay.today().gap==DayGap::None);
 }
 std::cout<<"PASS: network/device recovery after midnight keeps that day incomplete without poisoning the next day"<<std::endl;
}
int main(){a_gap_recovering_just_after_midnight_cannot_certify_the_new_day();
 unavailable_midnight_boundary_explains_why_the_old_sample_was_kept();
 reconfirmed_config_overrides_an_uncertain_committed_write();
 cold_start_at_midnight_still_requires_the_next_complete_day();
 midnight_clock_correction_cannot_erase_confirmed_actions();
 older_confirmation_cannot_end_a_newer_manual_override();
 bounded_records_and_midnight_pending_preserve_previous_sample();
 only_confirmed_automation_ends_manual_override();
 network_loss_cannot_keep_homekit_primary();
 manual_without_time_gets_a_boundary_after_clock_returns();
 manual_override_survives_reboot_until_next_action();
 replay_projects_current_cycle_without_replaying_missed_actions();
 journal_survives_torn_update_without_losing_previous_sample();
 storage_matches_portable_fixture_and_rejects_corruption();
 restore_keeps_sample_but_not_time_coverage();
 failed_day_retains_sample_and_reason();
 confirmed_automation_only();
 constexpr int64_t nov8=1794067200000LL,day=86400000;
 unsigned saves=0;DailyReplay replay([&](const DailyPersistedState&){++saves;return true;});
 assert(!replay.state().enabled && !replay.state().sample.valid);
 DailyInputs in{0,nov8+4*3600000,AutomationHealth::Healthy,true};
 assert(replay.configure(true,in));assert(!replay.state().sample.valid);
 in.nowMs=20*3600000;in.wallMs=nov8+day;replay.advance(in);
 assert(!replay.state().sample.valid && replay.today().day==20766);
 for(unsigned seconds=1;seconds<=86400;++seconds){in.nowMs=20*3600000+seconds*1000;in.wallMs=nov8+day+int64_t(seconds)*1000;replay.advance(in);}
 assert(replay.state().sample.valid && replay.state().sample.day==20766 && replay.state().sample.count==0);
 assert(saves==2); // enabling once and sealing once, never on each tick
 std::cout<<"PASS: partial first day is not sealed; complete empty Beijing day replaces sample exactly once\n";
}
