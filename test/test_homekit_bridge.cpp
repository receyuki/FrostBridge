#include "HomeKitBridge.h"
#include <HomeSpan.h>
#include <cassert>
#include <iostream>
using namespace frost;
SpanService& climate(){for(auto& s:stubServices)if(s->aid==8 && dynamic_cast<Service::HeaterCooler*>(s.get()))return *s;std::abort();}
SpanCharacteristic& characteristic(SpanService& s,const char* type){for(auto* c:s.characteristics)if(c->type==type)return *c;std::abort();}
bool writeBoth(SpanService& s,double temperature,bool power){
 auto& t=characteristic(s,"CoolingThresholdTemperature");auto& a=characteristic(s,"Active");t.changed=a.changed=true;t.incoming=temperature;a.incoming=power;
 const bool accepted=s.update();if(accepted){t.value=temperature;a.value=power;}t.changed=a.changed=false;return accepted;
}
bool write(SpanService& s,const char* type,double value){auto& c=characteristic(s,type);c.changed=true;c.incoming=value;const bool accepted=s.update();if(accepted)c.value=value;c.changed=false;return accepted;}
int main(){
 PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.boot=42;snapshot.ready=snapshot.writes=snapshot.connected=snapshot.observed=snapshot.valid=true;snapshot.epoch=1;snapshot.capturedMs=snapshot.observedMs=100;snapshot.status.targetC=5;snapshot.status.power=true;snapshot.status.currentC=8;snapshot.controls={Error::None,Error::None,Error::None};mail.publish(snapshot);
 bool delayConsumption=false;
 hap::begin({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){auto r=mail.lookup(t);if(delayConsumption && r && r->state==WebState::Confirmed){snapshot.capturedMs=152;mail.publish(snapshot);stubNow=153;delayConsumption=false;}return r;},[&]{return mail.results();},[&](const std::array<WebAction,2>& a,uint32_t n){return mail.submitPair(a,n);}});
#ifdef FROSTBRIDGE_HOMEKIT_ZH
 const char* names[]={"FrostBridge", "冰箱电源","冰箱节能","自动化电源","自动化节能","连通检测","检测回应","冰箱温控","自动化温控"};
#else
 const char* names[]={"FrostBridge", "Fridge Power","Fridge ECO","Automation Power","Automation ECO","Connectivity Probe","Probe Reply","Fridge Temperature","Automation Temperature"};
#endif
 size_t informationCount=0;
 for(const auto& s:stubServices){
  assert(s->aid>=1 && s->aid<=9);
  if(dynamic_cast<Service::AccessoryInformation*>(s.get())){++informationCount;assert(characteristic(*s,"Name").text==(s->aid==1?"FrostBridge":names[s->aid-1]));}
  for(const auto* c:s->characteristics)if(c->type=="ConfiguredName")assert(c->text==names[s->aid-1]);
 }
 assert(informationCount==9);
 std::cout<<"PASS: all accessory and configured names follow the selected language; default is English\n";
 auto& service=climate();auto& target=characteristic(service,"CoolingThresholdTemperature");service.loop();
 assert(write(service,"CoolingThresholdTemperature",3));const auto action=mail.take();assert(action&&action->intent.value==3);
 service.loop();assert(target.getVal<int>()==3 && "accepted target must not immediately roll back before BLE reply");assert(mail.snapshot().status.targetC==5);
 snapshot.capturedMs=151;mail.publish(snapshot); // older observation published after the confirming packet arrived
 auto result=*mail.lookup(action->id);result.state=WebState::Confirmed;result.finishedMs=150;mail.put(result);stubNow=151;delayConsumption=true;hap::poll(stubNow);service.loop();
 assert(target.getVal<int>()==3 && "terminal result must wait for its later snapshot publication");
 snapshot.capturedMs=154;snapshot.observedMs=149;snapshot.status.targetC=3;mail.publish(snapshot);stubNow=155;hap::poll(stubNow);service.loop();assert(target.getVal<int>()==3);
 stubNow=160;assert(write(service,"CoolingThresholdTemperature",4));const auto retryAction=mail.take();assert(retryAction);
 assert(write(service,"CoolingThresholdTemperature",4) && "identical in-flight HAP retry must not fail after original acceptance");assert(!mail.take());
 assert(!write(service,"CoolingThresholdTemperature",5));assert(!mail.take());
 assert(writeBoth(service,4,false));const auto power=mail.take();assert(power && power->intent.setting==alpicool::Setting::Power && power->intent.value==0 && !mail.take());
 auto powerResult=*mail.lookup(power->id);powerResult.state=WebState::Confirmed;powerResult.finishedMs=170;mail.put(powerResult);snapshot.status.power=false;snapshot.capturedMs=172;mail.publish(snapshot);stubNow=173;hap::poll(stubNow);service.loop();assert(characteristic(service,"Active").getVal<int>()==0 && target.getVal<int>()==4);
 snapshot.status.currentC=7;mail.publish(snapshot);service.loop();assert(characteristic(service,"CurrentTemperature").getVal<int>()==7 && target.getVal<int>()==4);
 result=*mail.lookup(retryAction->id);result.state=WebState::TimedOut;result.error=ApiError::Timeout;result.finishedMs=180;mail.put(result);stubNow=181;hap::poll(stubNow);service.loop();assert(target.getVal<int>()==3);
 snapshot.connected=false;mail.publish(snapshot);assert(!write(service,"CoolingThresholdTemperature",2));assert(!mail.take());
 snapshot.connected=true;snapshot.capturedMs=200;mail.publish(snapshot);stubNow=201;assert(write(service,"CoolingThresholdTemperature",2));const auto stuck=mail.take();assert(stuck);
 result=*mail.lookup(stuck->id);result.state=WebState::Confirmed;result.error=ApiError::None;result.finishedMs=210;mail.put(result);stubNow=211;hap::poll(stubNow);service.loop();assert(target.getVal<int>()==2);
 stubNow=1211;service.loop();assert(target.getVal<int>()==3); // no indefinite optimistic hold when snapshot publication stalls
 SpanService* powerSwitch=nullptr;for(auto& s:stubServices)if(s->aid==2 && dynamic_cast<Service::Switch*>(s.get()))powerSwitch=s.get();assert(powerSwitch);
 powerSwitch->loop();assert(write(*powerSwitch,"On",true));const auto switchAction=mail.take();assert(switchAction);powerSwitch->loop();assert(characteristic(*powerSwitch,"On").getVal<bool>());
 result=*mail.lookup(switchAction->id);result.state=WebState::TimedOut;result.error=ApiError::Timeout;mail.put(result);hap::poll(stubNow);powerSwitch->loop();assert(!characteristic(*powerSwitch,"On").getVal<bool>());
 std::cout<<"PASS: HAP pending target, terminal publication barrier, identical retry, conflict/offline rejection, live current temperature and timeout rollback\n";
}
