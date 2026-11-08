#include "HomeKitControl.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <limits>
using namespace frost;
void temperature_and_power_are_admitted_together(){
  PortalMailbox mail;mail.reset(42);WebSnapshot s;s.ready=s.writes=s.connected=s.observed=s.valid=true;s.epoch=7;s.capturedMs=s.observedMs=100;s.status.power=false;s.status.targetC=5;s.status.currentC=17;s.controls={Error::None,Error::None,Error::None};mail.publish(s);
  ApiBackend backend{[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();},[&](std::array<WebAction,2> a,uint32_t n){return mail.submitPair(a,n);}};
  HomeKitControl hk(backend);
  assert(!hk.requestClimate(5.5f,std::nullopt,false,101));assert(!mail.take());
  assert(!hk.requestClimate(std::numeric_limits<float>::quiet_NaN(),std::nullopt,false,101));assert(!mail.take());
  s.connected=false;mail.publish(s);assert(!hk.requestClimate(10.0f,true,false,101));assert(!mail.take());assert(!hk.climate(false,101).fresh && hk.climate(false,101).target==5);
  s.connected=true;s.writes=false;mail.publish(s);assert(!hk.requestClimate(10.0f,std::nullopt,false,101));assert(!mail.take());
  s.writes=true;s.status.minC=-15;s.status.maxC=20;mail.publish(s);assert(!hk.requestClimate(-20.0f,std::nullopt,false,101));assert(!mail.take());
  s.status.minC=-20;mail.publish(s);
  assert(hk.requestClimate(10.0f,true,false,102));
  auto temp=mail.take(),power=mail.take();assert(temp && power && temp->intent.setting==alpicool::Setting::TargetC && temp->intent.value==10 && power->intent.setting==alpicool::Setting::Power && power->intent.value==1);
  assert(temp->intent.source==Source::HomeKit && power->intent.source==Source::HomeKit && temp->epoch==7 && power->epoch==7);
  assert(hk.climate(false,103).target==5 && hk.climate(false,103).pending);
  auto r=*mail.lookup(temp->id);r.state=WebState::Confirmed;mail.put(r);r=*mail.lookup(power->id);r.state=WebState::Confirmed;mail.put(r);hk.tick(104);
  for(unsigned i=0;i<3;++i){WebAction a;assert(mail.submit(a,105));}
  assert(!hk.requestClimate(-10.0f,false,true,106));
  unsigned queued=0;while(mail.take())++queued;assert(queued==3); // no partial climate request
  assert(hk.requestClimate(-10.0f,false,true,107));temp=mail.take();power=mail.take();assert(temp->intent.source==Source::HomeKitAutomation && power->intent.source==Source::HomeKitAutomation);
  s.status.targetC=-10;s.capturedMs=s.observedMs=108;mail.publish(s);r=*mail.lookup(temp->id);r.state=WebState::Confirmed;mail.put(r);r=*mail.lookup(power->id);r.state=WebState::Confirmed;mail.put(r);hk.tick(109);s.capturedMs=110;mail.publish(s);assert(hk.climate(true,111).target==-10 && !hk.climate(true,111).pending && hk.automationConfirmed()==2);
  PortalMailbox full;full.reset(1);for(unsigned i=0;i<15;++i){assert(full.submit({},1));assert(full.take());}
  assert(!full.submitPair({},2) && !full.take());assert(full.submit({},3)); // one free history slot cannot admit a pair
}
void failures_same_value_and_boot_expiry(){
  PortalMailbox mail;mail.reset(42);WebSnapshot s;s.ready=s.writes=s.connected=s.observed=s.valid=true;s.epoch=1;s.capturedMs=s.observedMs=10;s.status.power=false;s.controls={Error::None,Error::None,Error::None};mail.publish(s);
  HomeKitControl hk({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}});
  assert(hk.request(HomeKitChannel::AutomationPower,false,11)); // same-value delivery is retained
  auto a=*mail.take();auto r=*mail.lookup(a.id);r.state=WebState::TimedOut;r.error=ApiError::Timeout;mail.put(r);hk.tick(12);
  assert(hk.automationRequests()==1 && hk.automationConfirmed()==0);
  assert(hk.value(HomeKitChannel::AutomationPower,12).error==ApiError::Timeout && hk.value(HomeKitChannel::Power,12).observed==false);
  assert(hk.request(HomeKitChannel::Power,true,13));assert(!hk.request(HomeKitChannel::Power,true,14));
  mail.reset(43);mail.publish(s);hk.tick(15);assert(hk.pending()==0 && hk.value(HomeKitChannel::Power,15).error==ApiError::Expired);
  s.connected=false;mail.publish(s);assert(!hk.request(HomeKitChannel::Power,true,16));assert(!mail.take());
  s.connected=true;mail.publish(s);assert(!hk.request(HomeKitChannel::Power,true,30011));
  s.observedMs=30011;s.writes=false;mail.publish(s);assert(!hk.request(HomeKitChannel::Power,true,30012));
  HomeKitControl health({});assert(!health.probeDue(0,false));assert(health.health(1000000)==LinkHealth::Unconfigured);
  assert(health.probeDue(0xfffffff0u,true));assert(health.acknowledge(10));assert(health.health(100)==LinkHealth::Healthy);
  assert(!health.acknowledge(101));assert(!health.probeDue(100,true));
}
void controller_readback_and_epoch(){
  PortalMailbox mail;mail.reset(42);WebSnapshot s;s.ready=s.writes=s.connected=s.observed=s.valid=true;s.epoch=1;s.capturedMs=s.observedMs=100;s.controls={Error::None,Error::None,Error::None};mail.publish(s);
  HomeKitControl hk({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}});
  std::ifstream input("test/fixtures/c15-query-panel-5.hex");std::string byte;alpicool::Frame frame;while(input>>byte)frame.push_back(std::stoul(byte,nullptr,16));
  auto checksum=[&]{unsigned sum=0;for(size_t i=0;i+2<frame.size();++i)sum+=frame[i];frame[frame.size()-2]=sum>>8;frame.back()=sum;};
  DailyReplay daily([](const DailyPersistedState&){return true;});
  unsigned writes=0,queries=0;uint32_t now=101;ControlRuntime core(42,[&](const alpicool::Frame& f){if(f[3]==1){++queries;return true;}assert(f[3]==2 && f[5]==1);++writes;return true;},daily,true);
  core.connection(true,1,100);core.observe(frame,1,1,100);PortalWorker worker(core,mail,[&]{return now;});
  assert(hk.request(HomeKitChannel::AutomationPower,true,101));worker.tick();hk.tick(101);assert(writes==1 && hk.automationConfirmed()==0);
  frame[5]=1;checksum();core.observe(frame,1,2,102);now=103;worker.tick();hk.tick(103);assert(hk.automationConfirmed()==1);
  bool sourceRetained=false;for(const auto& row:mail.results())if(row.hasController && row.intent.source==Source::HomeKitAutomation)sourceRetained=true;assert(sourceRetained);
  assert(hk.request(HomeKitChannel::AutomationPower,true,104));now=105;worker.tick();hk.tick(105);
  assert(hk.automationConfirmed()==1 && hk.pending()==1 && queries==1 && writes==1);
  core.observe(frame,1,3,106);now=107;worker.tick(); // terminal result exists before the next adapter poll
  assert(hk.request(HomeKitChannel::AutomationPower,true,108));hk.tick(108);assert(hk.automationConfirmed()==2);
  core.connection(false,1,109);core.connection(true,2,110);core.observe(frame,2,4,111);now=112;worker.tick();hk.tick(112);
  assert(writes==1 && hk.value(HomeKitChannel::AutomationPower,112).error==ApiError::Disconnected);
}
void early_automation_failures_and_round_trip_evidence_reach_the_worker(){
 PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.ready=snapshot.writes=true;mail.publish(snapshot);ApiBackend backend{[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}};
 backend.rejectedAutomation=[&](uint32_t n){mail.rejectAutomation(n);};backend.reportHealth=[&](AutomationHealth h,uint32_t n,std::optional<uint32_t> age){mail.reportHealth(h,n,age);};HomeKitControl hk(backend);
 assert(!hk.request(HomeKitChannel::AutomationPower,true,100));auto input=mail.receive(101);assert(input.loss && !input.action && input.lossFirstMs==100);
 assert(!hk.request(HomeKitChannel::Power,true,102));assert(!mail.receive(103).loss);
 snapshot.connected=snapshot.observed=snapshot.valid=true;snapshot.epoch=1;snapshot.observedMs=100;snapshot.status.power=false;snapshot.status.targetC=4;snapshot.controls={Error::None,Error::None,Error::None};mail.publish(snapshot);
 assert(!hk.requestClimate(5.5f,std::nullopt,true,104));assert(mail.receive(105).loss);
 assert(hk.request(HomeKitChannel::AutomationPower,true,106));assert(!hk.request(HomeKitChannel::AutomationPower,true,107));input=mail.receive(108);assert(input.action && input.loss);
 assert(hk.probeDue(109,true) && hk.acknowledge(110));hk.tick(111);const auto evidence=mail.healthEvidence();assert(evidence.health==AutomationHealth::Healthy && evidence.sampledMs==111 && evidence.ackAgeMs==1);
 std::cout<<"PASS: early rejected automation is not silently lost; only real round-trip evidence renews worker health"<<std::endl;
}
void preempted_snapshot_reads_use_the_live_clock(){
 uint32_t live=100;PortalMailbox mail({},[&]{return live;});mail.reset(42);WebSnapshot snapshot;snapshot.ready=snapshot.writes=snapshot.connected=snapshot.observed=snapshot.valid=true;snapshot.epoch=1;snapshot.observedMs=snapshot.capturedMs=100;snapshot.status.power=false;snapshot.status.targetC=5;snapshot.status.currentC=8;snapshot.controls={Error::None,Error::None,Error::None};mail.publish(snapshot);
 ApiBackend backend{[&]{live=120;snapshot.observedMs=snapshot.capturedMs=110;mail.publish(snapshot);return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}};HomeKitControl hk(backend,[&]{return live;});
 assert(hk.requestClimate(3.0f,std::nullopt,false,100));const auto action=mail.take();assert(action && action->admittedMs==120);assert(hk.value(HomeKitChannel::Power,100).fresh && hk.climate(false,100).fresh);
 std::cout<<"PASS: a newly published observation after preemption cannot underflow an old callback clock into a false stale/no-response rejection"<<std::endl;
}
int main(){
  preempted_snapshot_reads_use_the_live_clock();
  early_automation_failures_and_round_trip_evidence_reach_the_worker();
  temperature_and_power_are_admitted_together();
  failures_same_value_and_boot_expiry();controller_readback_and_epoch();
  PortalMailbox mail;mail.reset(42);WebSnapshot state;state.boot=42;state.ready=true;state.writes=true;state.connected=true;state.observed=true;state.valid=true;state.epoch=7;state.capturedMs=state.observedMs=100;state.status.power=false;state.status.mode=alpicool::Mode::Max;state.controls={Error::None,Error::None,Error::None};mail.publish(state);
  HomeKitControl hk({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}});
  assert(hk.request(HomeKitChannel::AutomationPower,true,101));
  const auto action=mail.take();assert(action && action->intent.source==Source::HomeKitAutomation && action->epoch==7 && action->intent.setting==alpicool::Setting::Power && action->intent.value==1);
  assert(hk.value(HomeKitChannel::Power,102).observed==false); // never optimistic
  assert(hk.value(HomeKitChannel::AutomationPower,102).pending);
  auto result=*mail.lookup(action->id);result.state=WebState::Confirmed;mail.put(result);
  state.status.power=true;state.capturedMs=state.observedMs=103;mail.publish(state);hk.tick(104);state.capturedMs=105;mail.publish(state);
  assert(hk.value(HomeKitChannel::AutomationPower,106).observed==true && !hk.value(HomeKitChannel::AutomationPower,106).pending);
  assert(hk.automationRequests()==1 && hk.automationConfirmed()==1);
  assert(hk.request(HomeKitChannel::Eco,true,105));assert(mail.take()->intent.source==Source::HomeKit);
  assert(hk.health(0)==LinkHealth::Unconfigured);assert(!hk.acknowledge(0));
  assert(hk.probeDue(0,true));assert(hk.acknowledge(500));assert(hk.health(180500)==LinkHealth::Healthy);assert(hk.health(180501)==LinkHealth::Expired);
  assert(hk.probeDue(60500,true));assert(!hk.acknowledge(80000));assert(hk.acknowledgements()==1);
  std::cout<<"PASS: HomeKit requests preserve source/observed state; round-trip health is not TCP presence\n";
}
