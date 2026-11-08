#include "Portal.h"
#include <cassert>
#include <iostream>
using namespace frost;
void admission_timestamps_and_loss_frontiers_are_linearized(){
 bool locked=false;uint32_t now=100;
 Critical critical{&locked,[](void* p){assert(!*static_cast<bool*>(p));*static_cast<bool*>(p)=true;},[](void* p){assert(*static_cast<bool*>(p));*static_cast<bool*>(p)=false;}};
 PortalMailbox mail(critical,[&]{assert(locked);return now++;});mail.reset(7);
 WebAction action;action.intent={alpicool::Setting::TargetC,4,Source::HomeKitAutomation};const auto first=mail.submit(action,1);assert(first && mail.lookup(*first)->createdMs==100);
 now=101;std::array<WebAction,2> pair{{action,action}};pair[1].intent={alpicool::Setting::Power,1,Source::HomeKitAutomation};const auto ids=mail.submitPair(pair,2);assert(ids && mail.lookup((*ids)[0])->createdMs==101 && mail.lookup((*ids)[1])->createdMs==101);
 now=102;WebAction clock;clock.kind=ActionKind::Clock;assert(mail.submit(clock,3));now=103;assert(!mail.submit(action,4));now=104;mail.rejectAutomation(5);
 now=105;auto input=mail.receive(6);assert(input.action && input.action->admittedMs==100 && input.frontierMs==101 && input.loss && input.lossFirstMs==103 && input.lossLastMs==104);
 now=106;input=mail.receive(7);assert(input.action->admittedMs==101 && input.frontierMs==101 && !input.loss);
 now=107;input=mail.receive(8);assert(input.action->admittedMs==101 && input.frontierMs==102);
 now=108;input=mail.receive(9);assert(input.action->kind==ActionKind::Clock && input.frontierMs==108);
 now=109;input=mail.receive(10);assert(!input.action && input.frontierMs==109 && !input.loss);
 std::cout<<"PASS: mailbox admission uses the locked live clock; pairs share time, loss is retained and fences never overtake queued input"<<std::endl;
}
void health_forwarding_cannot_renew_old_evidence(){
 uint32_t now=10050;PortalMailbox mail({},[&]{return now;});mail.reset(7);mail.reportHealth(AutomationHealth::Healthy,10000,1000);auto evidence=mail.healthEvidence();assert(evidence.health==AutomationHealth::Healthy && evidence.sampledMs==10050 && evidence.ackAgeMs==1050);
 now=200000;mail.reportHealth(AutomationHealth::Healthy,10000,1000);evidence=mail.healthEvidence();assert(evidence.health==AutomationHealth::Expired && evidence.sampledMs==200000 && evidence.ackAgeMs==191000);
 mail.reportHealth(AutomationHealth::Unknown,now,std::nullopt);assert(mail.healthEvidence().health==AutomationHealth::Unknown && !mail.healthEvidence().ackAgeMs);
 std::cout<<"PASS: health forwarding adds processing delay instead of extending an old 180-second lease"<<std::endl;
}
void daily_pages_are_bounded_versioned_and_distinguish_empty_from_absent(){
 DailyPersistedState stored;stored.enabled=true;stored.generation=7;stored.sample.valid=true;stored.sample.day=20764;stored.sample.count=9;
 for(uint16_t n=0;n<9;++n)stored.sample.actions[n]={uint32_t(3600+n*60),DailyField::Temperature,3,n};
 DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(stored));PortalMailbox mail;mail.reset(7);WebSnapshot web;web.boot=7;web.ready=true;mail.publish(web,daily);auto snapshot=mail.snapshot();const auto version=snapshot.backup.sampleRevision;
 assert(snapshot.backup.sampleCount==9 && snapshot.generation==7 && version);auto page=mail.page(true,0,version);assert(page.state==PageState::Ok && page.boot==7 && page.count==8 && page.total==9 && page.day==20764 && page.rows[7].action.second==4020);
 page=mail.page(true,8,version);assert(page.count==1 && page.rows[0].action.second==4080);assert(mail.page(true,0,version+1).state==PageState::Changed && mail.page(true,7,version).state==PageState::Absent);
 DailyInputs in{1000,1794067201000LL,AutomationHealth::Healthy,true};const auto token=daily.capture({alpicool::Setting::TargetC,10,Source::HomeKitAutomation},in);assert(token);mail.publish(web,daily);const auto pending=mail.snapshot().backup.todayRevision;assert(mail.page(false,0,pending).rows[0].outcome==DailyOutcome::Pending);
 assert(daily.complete(*token,true));mail.publish(web,daily);const auto confirmed=mail.snapshot().backup.todayRevision;assert(confirmed!=pending && mail.page(false,0,pending).state==PageState::Changed && mail.page(false,0,confirmed).rows[0].outcome==DailyOutcome::Confirmed);mail.publish(web,daily);assert(mail.snapshot().backup.todayRevision==confirmed);
 assert(daily.configure(false,in));mail.publish(web,daily);assert(mail.snapshot().backup.sampleRevision==version && mail.snapshot().backup.todayRevision==confirmed);
 auto empty=stored;empty.sample.day=20765;empty.sample.count=0;assert(daily.restore(empty));mail.publish(web,daily);const auto emptyVersion=mail.snapshot().backup.sampleRevision;page=mail.page(true,0,emptyVersion);assert(page.state==PageState::Ok && page.count==0 && page.total==0 && emptyVersion!=version);
 assert(daily.restore({}));mail.publish(web,daily);assert(mail.page(true,0,mail.snapshot().backup.sampleRevision).state==PageState::Absent);
 mail.reset(8);web.boot=8;assert(daily.restore(stored));mail.publish(web,daily);assert(mail.page(true,0,mail.snapshot().backup.sampleRevision).boot==8);
 std::cout<<"PASS: eight-row daily pages publish atomically, retain stable versions and carry boot/date; empty and absent samples differ"<<std::endl;
}
void config_and_younger_automation_keep_chronological_learning_watermarks(){
 uint32_t now=0;DailyReplay daily([](const DailyPersistedState&){return true;});ControlRuntime core(7,[](const alpicool::Frame& f){assert(f[3]==1);return true;},daily,true);core.syncTime(1794067200,0);core.automationLink(AutomationHealth::Healthy,true,0);
 alpicool::Frame device{0xfe,0xfe,0x15,1,0,0,0,2,4,20,0xec,2,0,0,0xfd,0xfd,0xfd,0,20,100,14,3,0,0};unsigned sum=0;for(size_t n=0;n+2<device.size();++n)sum+=device[n];device[device.size()-2]=sum>>8;device.back()=sum;core.connection(true,1,0);core.observe(device,1,1,0);
 PortalMailbox mail({},[&]{return now;});mail.reset(7);PortalWorker worker(core,mail,[&]{return now;});WebAction preview;preview.kind=ActionKind::Preview;preview.enabled=true;now=50;const auto id=mail.submit(preview,0);assert(id);worker.tick();const auto token=mail.lookup(*id)->previewToken;assert(token);
 WebAction apply;apply.kind=ActionKind::Apply;apply.preview={7,token};now=100;const auto saved=mail.submit(apply,0);assert(saved);WebAction automatic;automatic.intent={alpicool::Setting::TargetC,4,Source::HomeKitAutomation};automatic.epoch=1;now=200;const auto command=mail.submit(automatic,0);assert(command);
 now=300;worker.tick();assert(mail.lookup(*saved)->state==WebState::Completed);core.observe(device,1,2,301);now=302;worker.tick();assert(mail.lookup(*command)->state==WebState::Confirmed && daily.today().count==1 && daily.record(0,false)->outcome==DailyOutcome::Confirmed && daily.today().gap==DayGap::PartialStart);
 std::cout<<"PASS: enabling behind a queued action preserves admission order without inventing a clock jump"<<std::endl;
}
int main(){config_and_younger_automation_keep_chronological_learning_watermarks();daily_pages_are_bounded_versioned_and_distinguish_empty_from_absent();health_forwarding_cannot_renew_old_evidence();admission_timestamps_and_loss_frontiers_are_linearized();}
