#include "ControlRuntime.h"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <fstream>
using namespace frost;
void checksum(alpicool::Frame& frame){unsigned sum=0;for(size_t i=0;i+2<frame.size();++i)sum+=frame[i];frame[frame.size()-2]=sum>>8;frame.back()=sum;}
alpicool::Frame simulatedSingleZone(){alpicool::Frame f{0xfe,0xfe,0x15,1,0,0,0,2,4,20,0xec,2,0,0,0xfd,0xfd,0xfd,0,20,100,14,3,0,0};checksum(f);return f;}
void confirmed_automation_uses_admission_time(){
 constexpr int64_t midnight=1794067200000LL;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.configure(true,{0,midnight,AutomationHealth::Healthy,true}));
 std::vector<alpicool::Frame> writes;ControlRuntime core(1,[&](const alpicool::Frame& f){writes.push_back(f);return true;},daily,true);
 assert(core.syncTime(midnight/1000,0));core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);assert(core.observe(device,1,1,0));
 auto accepted=core.submit({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},3000,1000);assert(accepted.id);core.tick(3001);assert(writes.size()==1 && writes[0][3]==1);
 assert(core.observe(device,1,2,3002));auto row=daily.record(0,false);assert(row && row->action.second==1 && row->outcome==DailyOutcome::Confirmed);assert(!daily.state().sample.valid);
 std::cout<<"PASS: confirmed automation is learned once at original admission time, including same-value Query"<<std::endl;
}
void ordinary_control_establishes_persistent_override_without_learning(){
 constexpr int64_t midday=1794067200000LL+20*3600000;DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=2;saved.sample.actions[0]={0,DailyField::Power,0,0};saved.sample.actions[1]={64800,DailyField::Power,1,1};unsigned saves=0;
 DailyReplay daily([&](const DailyPersistedState&){++saves;return true;});assert(daily.restore(saved));ControlRuntime core(2,[](const alpicool::Frame&){return true;},daily,true);assert(core.syncTime(midday/1000,0));core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 auto command=core.submit({alpicool::Setting::TargetC,10,Source::Web},1000);assert(command.id && daily.state().manual && daily.state().overrideUntilMs==1794153600000LL && daily.today().count==0 && saves==1);
 core.tick(1001);device[8]=10;checksum(device);core.observe(device,1,2,1002);assert(core.command(*command.id)->state==CommandState::Confirmed && daily.today().count==0);
 core.connection(false,1,1003);core.connection(true,2,1004);core.observe(device,2,1,1005);assert(daily.state().manual && daily.today().count==0);
 std::cout<<"PASS: ordinary control persists an override, never enters the template, and survives BLE reconnect"<<std::endl;
}
void observed_manual_change_survives_failed_save_but_reconnect_does_not_guess(){
 DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return false;});assert(daily.restore(saved));ControlRuntime core(3,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 device[8]=10;checksum(device);core.observe(device,1,2,1000);assert(daily.state().manual && !daily.storageHealthy() && daily.today().count==0);
 DailyReplay other([](const DailyPersistedState&){return true;});assert(other.restore(saved));ControlRuntime reconnect(4,[](const alpicool::Frame&){return true;},other,true);reconnect.syncTime(1794067200,0);reconnect.connection(true,1,0);reconnect.observe(simulatedSingleZone(),1,1,0);reconnect.connection(false,1,1);reconnect.connection(true,2,2);reconnect.observe(device,2,1,3);assert(!other.state().manual);
 std::cout<<"PASS: observed panel changes retain RAM override on save failure; disconnected changes are not guessed as manual"<<std::endl;
}
void expired_link_recovers_current_profile_without_replaying_history(){
 constexpr int64_t evening=1794067200000LL+20*3600000;DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=4;
 saved.sample.actions[0]={0,DailyField::Power,0,0};saved.sample.actions[1]={64800,DailyField::Temperature,3,1};saved.sample.actions[2]={64800,DailyField::Mode,1,2};saved.sample.actions[3]={64800,DailyField::Power,1,3};
 DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));std::vector<alpicool::Frame> writes;ControlRuntime core(5,[&](const alpicool::Frame& f){writes.push_back(f);return true;},daily,true);core.syncTime(evening/1000,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);core.automationLink(AutomationHealth::Healthy,true,0);core.tick(100);assert(writes.empty());
 core.automationLink(AutomationHealth::Expired,false,200);uint32_t now=200,sequence=1;
 for(unsigned step=0;step<3;++step){core.tick(++now);core.tick(++now);assert(writes.size()==step+1);const auto& sent=writes.back();if(sent[3]==5)device[8]=sent[4];else{assert(sent[3]==2);std::copy(sent.begin()+4,sent.begin()+18,device.begin()+4);}checksum(device);assert(core.observe(device,1,++sequence,++now));}
 core.tick(++now);assert(device[8]==3 && device[6]==1 && device[5]==1 && daily.today().count==0 && !daily.state().manual);
 assert(core.decision(now).mode==ReplayMode::Replaying);for(const auto& c:core.commands())assert(c.intent.source==Source::LocalReplay);
 std::cout<<"PASS: HomeKit expiry aligns only the current profile through Controller, without self-learning or missed on/off history"<<std::endl;
}
void normal_boundary_replays_only_its_actions_in_stable_order(){
 constexpr int64_t evening=1794067200000LL+20*3600000;DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=6;
 saved.sample.actions[0]={0,DailyField::Power,0,0};saved.sample.actions[1]={64800,DailyField::Temperature,3,1};saved.sample.actions[2]={64800,DailyField::Mode,1,2};saved.sample.actions[3]={64800,DailyField::Power,1,3};saved.sample.actions[4]={72005,DailyField::Temperature,10,4};saved.sample.actions[5]={72005,DailyField::Temperature,8,5};
 DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));std::vector<alpicool::Frame> writes;ControlRuntime core(6,[&](const alpicool::Frame& f){writes.push_back(f);return true;},daily,true);core.syncTime(evening/1000,0);core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);uint32_t now=0,seq=1;
 for(unsigned n=0;n<3;++n){core.tick(++now);core.tick(++now);const auto& sent=writes.back();if(sent[3]==5)device[8]=sent[4];else std::copy(sent.begin()+4,sent.begin()+18,device.begin()+4);checksum(device);core.observe(device,1,++seq,++now);}assert(writes.size()==3);
 core.automationLink(AutomationHealth::Expired,false,5000);
 for(unsigned n=0;n<2;++n){now=5000+n*3;core.tick(now);core.tick(now+1);assert(writes.size()==4+n && writes.back()[3]==5 && writes.back()[4]==(n==0?10:8));device[8]=writes.back()[4];checksum(device);core.observe(device,1,++seq,now+2);}
 core.tick(5006);core.tick(5007);assert(writes.size()==5 && daily.today().count==0);
 std::cout<<"PASS: normal daily boundary preserves same-second order and does not resend unrelated profile fields"<<std::endl;
}
void queued_older_automation_cannot_release_newer_manual_override(){
 DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(7,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 assert(core.submit({alpicool::Setting::Power,1,Source::Web},2000).id);assert(daily.state().manual);
 assert(core.submit({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},2001,1000).id);
 core.tick(2002);device[5]=1;checksum(device);core.observe(device,1,2,2003);core.tick(2004);core.observe(device,1,3,2005);
 assert(daily.record(0,false)->outcome==DailyOutcome::Confirmed && daily.state().manual);
 std::cout<<"PASS: an older admitted automation processed later cannot release a newer manual override"<<std::endl;
}
void health_evidence_expires_at_its_real_lease(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,3,0};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(8,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,200000);core.automationLink(AutomationHealth::Healthy,true,200000);
 assert(core.decision(206001).mode==ReplayMode::HomeKitPrimary);assert(core.decision(380000).mode==ReplayMode::HomeKitPrimary);assert(core.decision(380001).mode==ReplayMode::Replaying);
 core.automationLink(AutomationHealth::Healthy,true,380002);assert(core.decision(380003).mode==ReplayMode::HomeKitPrimary);
 std::cout<<"PASS: cached HomeKit health remains leased for 180 seconds, expires, and recovers on new evidence"<<std::endl;
}
void confirmation_after_midnight_cannot_certify_the_previous_day(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,4,0};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(9,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067199,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);uint32_t seq=1;core.observe(device,1,seq,0);core.learnThrough(0,0);
 for(uint32_t sec=1;sec<=86400;++sec){const auto now=sec*1000;core.automationLink(AutomationHealth::Healthy,true,now);if(sec%10==0)core.observe(device,1,++seq,now);core.learnThrough(now,now);if(sec==86400){assert(daily.today().gap==DayGap::None);assert(core.submit({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},now).id);core.tick(now+1);}}
 core.observe(device,1,++seq,86401001);core.automationLink(AutomationHealth::Healthy,true,86401002);core.learnThrough(86401002,86401002);
 if(daily.state().sample.day!=20764 || daily.lastClosed()->gap!=DayGap::PendingAtBoundary)std::cerr<<"late confirmation: sample="<<daily.state().sample.day<<" gap="<<int(daily.lastClosed()->gap)<<std::endl;
 assert(daily.state().sample.day==20764 && daily.lastClosed()->gap==DayGap::PendingAtBoundary);
 std::cout<<"PASS: post-midnight readback cannot turn an old-day pending action into a complete sample"<<std::endl;
}
void queued_manual_intent_expires_before_the_next_boundary_dispatch(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=2;saved.sample.actions[0]={0,DailyField::Power,0,0};saved.sample.actions[1]={64800,DailyField::Power,1,1};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));std::vector<alpicool::Frame> sent;ControlRuntime core(10,[&](const alpicool::Frame& f){sent.push_back(f);return true;},daily,true);core.syncTime(1794153599,0);core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 auto manual=core.submit({alpicool::Setting::Power,1,Source::Web},100);assert(manual.id);core.learnThrough(1001,1001);core.tick(1001);core.tick(1002);
 assert(core.command(*manual.id)->state==CommandState::Cancelled);assert(sent.size()==1 && sent[0][3]==1); // OFF is already observed: only a fresh Query, never a brief ON.
 std::cout<<"PASS: a manual intent queued before its override boundary cannot power on after expiry"<<std::endl;
}
void a_bluetooth_gap_cannot_produce_a_complete_empty_day(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,4,0};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(11,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067199,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();uint32_t epoch=1,seq=1;core.connection(true,epoch,0);core.observe(device,epoch,seq,0);core.learnThrough(0,0);
 for(uint32_t sec=1;sec<=86401;++sec){const auto now=sec*1000;core.automationLink(AutomationHealth::Healthy,true,now);if(sec==3600)core.connection(false,epoch,now);else if(sec==3601){++epoch;seq=0;core.connection(true,epoch,now);core.observe(device,epoch,++seq,now);}else if(sec%10==0)core.observe(device,epoch,++seq,now);core.learnThrough(now,now);}
 assert(daily.state().sample.day==20764 && daily.lastClosed()->gap!=DayGap::None);
 std::cout<<"PASS: a known Bluetooth gap keeps the previous sample even when no automation action was received"<<std::endl;
}
void rejected_admissions_invalidate_only_intersecting_days_before_sealing(){
 for(bool spansMidnight:{false,true}){
  DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,4,0};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(12,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067199,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);uint32_t seq=1;core.observe(device,1,seq,0);core.learnThrough(0,0);
  for(uint32_t sec=1;sec<=86400;++sec){const auto now=sec*1000;core.automationLink(AutomationHealth::Healthy,true,now);if(sec%10==0)core.observe(device,1,++seq,now);core.learnThrough(now,now);}
  assert(daily.today().gap==DayGap::None);core.inputLoss(spansMidnight?86400998:86401001,86401002);
  core.learnThrough(86400999,86401003);core.learnThrough(86401003,86401003);
  assert(daily.state().sample.day==(spansMidnight?20764:20765));assert(daily.today().gap==DayGap::Rejected);
 }
 std::cout<<"PASS: rejected admissions mark exactly the intersecting days, including future loss before older inputs drain"<<std::endl;
}
void failed_replay_waits_for_fresh_state_then_recomputes_the_current_goal(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,3,0};DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));bool fail=true;unsigned sent=0;ControlRuntime core(13,[&](const alpicool::Frame&){++sent;return !fail;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);core.tick(1);core.tick(2);core.tick(3);assert(sent==1 && !core.observed().valid);
 fail=false;core.observe(device,1,2,10);core.tick(11);core.tick(12);core.tick(4000);assert(sent==1);core.tick(6001);core.tick(6002);assert(sent==2 && core.commands().back().state==CommandState::AwaitingReadback && core.commands().front().state==CommandState::Failed);
 std::cout<<"PASS: failed replay is not blindly retried; fresh state permits a new current-goal command"<<std::endl;
}
void confirmed_automation_can_end_an_override_without_a_learnable_timestamp(){
 DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(14,[](const alpicool::Frame&){return true;},daily,true);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);assert(core.submit({alpicool::Setting::TargetC,10,Source::Web},100).id);core.tick(101);device[8]=10;checksum(device);core.observe(device,1,2,102);assert(daily.state().manual);
 assert(core.submit({alpicool::Setting::TargetC,10,Source::HomeKitAutomation},200).id);core.tick(201);core.observe(device,1,3,202);assert(!daily.state().manual && daily.today().count==0 && !core.timeTrusted());
 std::cout<<"PASS: a confirmed real automation ends manual override even while time is unknown; no timestamp or sample is invented"<<std::endl;
}
void existing_control_contracts_remain_intact(){
 auto sample=[](){DailyPersistedState s;s.enabled=true;s.sample.valid=true;s.sample.day=20764;s.sample.count=3;s.sample.actions[0]={0,DailyField::Temperature,3,0};s.sample.actions[1]={0,DailyField::Mode,1,1};s.sample.actions[2]={0,DailyField::Power,1,2};return s;};
 {
  unsigned sent=0;DailyReplay daily([](const DailyPersistedState&){return true;});ControlRuntime core(20,[&](const alpicool::Frame&){++sent;return true;},daily);assert(core.submit({alpicool::Setting::TargetC,10},0).error==Error::WritesDisabled);core.tick(1);assert(sent==0);
 }
 {
  DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(sample()));unsigned sent=0;ControlRuntime core(21,[&](const alpicool::Frame&){++sent;return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Expired,false,0);
  std::ifstream file("test/fixtures/c15-query-01.hex");assert(file.is_open());alpicool::Frame frame;unsigned byte;while(file>>std::hex>>byte)frame.push_back(static_cast<uint8_t>(byte));frame[32]=2;checksum(frame); // Derived unsupported model, not a capture.
  core.connection(true,1,0);core.observe(frame,1,1,0);core.tick(1);core.tick(2);assert(sent==0 && core.lastAutoError()==Error::UnsupportedLayout);
 }
 {
  DailyReplay daily([](const DailyPersistedState&){return false;});assert(daily.restore(sample()));ControlRuntime core(22,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);auto manual=core.submit({alpicool::Setting::TargetC,10,Source::Web},1);assert(!manual.id && manual.error==Error::PersistenceFailed && !daily.state().manual);
 }
 {
  DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(sample()));unsigned sent=0;ControlRuntime core(23,[&](const alpicool::Frame&){++sent;return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);core.tick(1);core.automationLink(AutomationHealth::Healthy,true,2);core.tick(2);assert(sent==0 && core.commands().back().state==CommandState::Cancelled);
  core.automationLink(AutomationHealth::Expired,false,3);core.tick(4);core.connection(false,1,5);core.connection(true,2,6);core.observe(device,2,1,7);core.tick(8);core.tick(9);assert(sent==1 && core.commands().size()==3 && core.commands()[1].state==CommandState::Cancelled && core.commands().back().state==CommandState::AwaitingReadback);
 }
 {
  DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(24,[](const alpicool::Frame& f){assert(f[3]==1);return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);uint32_t seq=1;core.observe(device,1,seq,0);
  for(unsigned n=0;n<65;++n){const auto now=n*3+1;auto cmd=core.submit({alpicool::Setting::Power,0,Source::HomeKitAutomation},now);assert(cmd.id);core.tick(now+1);core.observe(device,1,++seq,now+2);assert(core.command(*cmd.id)->state==CommandState::Confirmed);}
  assert(core.commands().size()==16 && daily.today().count==64 && daily.today().gap==DayGap::Overflow && daily.record(63,false)->outcome==DailyOutcome::Confirmed);
 }
 {
  DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));ControlRuntime core(25,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);auto first=simulatedSingleZone();core.connection(true,1,0);core.observe(first,1,1,0);core.tick(100);auto second=first;second[8]=6;checksum(second);first.insert(first.end(),second.begin(),second.end());alpicool::Decoder decoder;uint32_t processed=100;
  for(const auto& f:decoder.feedStamped(first.data(),first.size(),{100,1}))assert(core.observe(f.bytes,1,f.first.sequence,f.first.receivedMs,++processed));
  assert(daily.state().manual && core.observed().status.targetC==6 && core.unixTime(103)==1794067200);
 }
 {
  DailyReplay daily([](const DailyPersistedState&){return true;});ControlRuntime core(26,[](const alpicool::Frame&){return true;},daily);assert(!core.syncNetworkTime({1794067200,0,1},40001));assert(!core.timeTrusted());assert(core.syncNetworkTime({1794067200,0xfffffff0,2},16));assert(core.unixTime(16)==1794067200);assert(!core.syncNetworkTime({1794067201,17,2},17));
 }
 std::cout<<"PASS: write gate, grouped-layout preflight, failed-save rejection, healthy handoff, reconnect, 65 controls/64 records, coalesced receipts and network clock regressions"<<std::endl;
}
void enabling_preview_is_read_only_and_shows_the_current_effect(){
 DailyPersistedState saved;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=2;saved.sample.actions[0]={0,DailyField::Temperature,3,0};saved.sample.actions[1]={0,DailyField::Power,1,1};unsigned saves=0;
 DailyReplay daily([&](const DailyPersistedState&){++saves;return true;});assert(daily.restore(saved));ControlRuntime core(27,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Expired,false,0);const auto preview=core.previewBackup(true,1000);
 assert(preview.enabled && preview.token && preview.effect.mode==ReplayMode::Replaying && preview.effect.mask==3 && preview.effect.values[0]==3 && preview.effect.values[1]==1);assert(saves==0 && !daily.state().enabled && daily.today().day==0);
 std::cout<<"PASS: enabling preview predicts the current effect without saving, recording or sending a command"<<std::endl;
}
void confirmed_config_ends_old_override_and_rejects_stale_previews(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=2;saved.sample.actions[0]={0,DailyField::Temperature,3,0};saved.sample.actions[1]={0,DailyField::Power,1,1};unsigned saves=0,sent=0;bool disk=true;DailyReplay daily([&](const DailyPersistedState&){++saves;return disk;});assert(daily.restore(saved));ControlRuntime core(28,[&](const alpicool::Frame&){++sent;return true;},daily,true);core.syncTime(1794067199,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);core.learnThrough(0,0);core.learnThrough(1000,1000);
 assert(core.submit({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},1001).id);core.tick(1002);core.observe(device,1,2,1003);assert(core.submit({alpicool::Setting::TargetC,10,Source::Web},1100).id);core.tick(1101);device[8]=10;checksum(device);core.observe(device,1,3,1102);assert(daily.state().manual && daily.today().count==1 && saves==1);
 core.automationLink(AutomationHealth::Expired,false,1103);const auto preview=core.previewBackup(true,1104);assert(preview.effect.mode==ReplayMode::Replaying && saves==1);assert(core.applyBackup(preview,1105)==BackupError::None);assert(!daily.state().manual && daily.today().count==1 && daily.state().enabled && saves==2 && sent==2 && core.observed().status.targetC==10);
 assert(core.applyBackup(preview,1106)==BackupError::StalePreview);
 const auto stale=core.previewBackup(false,1107);core.syncTime(1794067200,1108);assert(core.applyBackup(stale,1109)==BackupError::StalePreview && daily.state().enabled && saves==2);
 const auto disabled=core.previewBackup(false,1110);assert(core.applyBackup(disabled,1111)==BackupError::None && !daily.state().enabled && sent==2);
 disk=false;const auto unchanged=core.previewBackup(false,1112);assert(core.applyBackup(unchanged,1112)==BackupError::PersistenceFailed && !daily.state().enabled);const auto failed=core.previewBackup(true,1112);assert(core.applyBackup(failed,1113)==BackupError::PersistenceFailed && !daily.state().enabled && sent==2);
 std::cout<<"PASS: confirmed config clears old override without dropping records; stale or unconfirmed saves do not become effective"<<std::endl;
}
void stale_queued_admissions_fail_without_reaching_the_device(){
 DailyPersistedState saved;saved.enabled=true;DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));unsigned sent=0;ControlRuntime core(29,[&](const alpicool::Frame&){++sent;return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,2,0);core.observe(device,2,1,0);
 const auto disconnected=core.submit({alpicool::Setting::TargetC,5,Source::HomeKitAutomation},1000,0,1);assert(!disconnected.id && disconnected.error==Error::Disconnected && daily.record(0,false)->outcome==DailyOutcome::Failed);
 const auto expired=core.submit({alpicool::Setting::TargetC,5,Source::HomeKitAutomation},7000,0,2);assert(!expired.id && expired.error==Error::RequestExpired && daily.record(1,false)->outcome==DailyOutcome::Failed && core.commands().empty() && sent==0);
 std::cout<<"PASS: disconnected-epoch and over-age queued admissions are recorded as failures without device dispatch"<<std::endl;
}
void network_time_keeps_fraction_and_queue_age(){
 DailyReplay daily([](const DailyPersistedState&){return true;});ControlRuntime core(1,[](const alpicool::Frame&){return true;},daily);
 assert(core.syncNetworkTime({1794067200,100,1,875},350));assert(core.unixTime(350)==1794067201);
 assert(!core.syncNetworkTime({1794067200,100,1,875},400));
 std::cout<<"PASS: network time retains subsecond sample and full queue age across midnight"<<std::endl;
}
void a_rejected_manual_save_does_not_cancel_queued_backup_alignment(){
 DailyPersistedState saved;saved.enabled=true;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=1;saved.sample.actions[0]={0,DailyField::Temperature,3,0};bool save=false;
 DailyReplay daily([&](const DailyPersistedState&){return save;});assert(daily.restore(saved));unsigned writes=0;ControlRuntime core(1,[&](const alpicool::Frame&){++writes;return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Expired,false,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 core.tick(1);assert(writes==0);const auto rejected=core.submit({alpicool::Setting::TargetC,10,Source::Web},2,2);assert(rejected.error==Error::PersistenceFailed && !rejected.id && !daily.state().manual);save=true;core.tick(3);core.tick(4);assert(writes==1);
 std::cout<<"PASS: rejecting a manual request leaves queued daily alignment live"<<std::endl;
}
void a_later_admitted_automation_ends_a_delayed_manual_override(){
 DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.configure(true,{0,1794067200000LL,AutomationHealth::Healthy,true}));
 ControlRuntime core(1,[](const alpicool::Frame&){return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);auto device=simulatedSingleZone();core.connection(true,1,0);core.observe(device,1,1,0);
 assert(core.submit({alpicool::Setting::Power,1,Source::Web},2000,1000).id);assert(daily.state().manual);
 assert(core.submit({alpicool::Setting::TargetC,4,Source::HomeKitAutomation},2001,1001).id);
 core.tick(2002);device[5]=1;checksum(device);core.observe(device,1,2,2003);core.tick(2004);core.observe(device,1,3,2005);core.tick(2006);assert(!daily.state().manual);
 std::cout<<"PASS: FIFO admission order lets a newer confirmed automation end a delayed manual override"<<std::endl;
}
int main(){a_later_admitted_automation_ends_a_delayed_manual_override();a_rejected_manual_save_does_not_cancel_queued_backup_alignment();network_time_keeps_fraction_and_queue_age();stale_queued_admissions_fail_without_reaching_the_device();confirmed_config_ends_old_override_and_rejects_stale_previews();enabling_preview_is_read_only_and_shows_the_current_effect();existing_control_contracts_remain_intact();confirmed_automation_can_end_an_override_without_a_learnable_timestamp();failed_replay_waits_for_fresh_state_then_recomputes_the_current_goal();rejected_admissions_invalidate_only_intersecting_days_before_sealing();a_bluetooth_gap_cannot_produce_a_complete_empty_day();queued_manual_intent_expires_before_the_next_boundary_dispatch();confirmation_after_midnight_cannot_certify_the_previous_day();health_evidence_expires_at_its_real_lease();queued_older_automation_cannot_release_newer_manual_override();normal_boundary_replays_only_its_actions_in_stable_order();expired_link_recovers_current_profile_without_replaying_history();observed_manual_change_survives_failed_save_but_reconnect_does_not_guess();ordinary_control_establishes_persistent_override_without_learning();confirmed_automation_uses_admission_time();}
