#ifdef FROSTBRIDGE_GATEWAY
#include "HomeKitBridge.h"
#include "HomeKitControl.h"
#include <HomeSpan.h>
#include <memory>
namespace frost::hap {
namespace {
std::unique_ptr<HomeKitControl> control;
bool secure(){return homeSpan.getStatus().first==HS_CONNECTED;}
void information(uint32_t aid,const char* name){
  new SpanAccessory(aid);
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name(name);
  new Characteristic::Manufacturer("FrostBridge");
  new Characteristic::Model("C15 BLE Bridge");
  new Characteristic::FirmwareRevision("0.3.1");
}
class FridgeSwitch final:public Service::Switch {
  HomeKitChannel channel_;SpanCharacteristic* on_;
 public:
  FridgeSwitch(HomeKitChannel channel,const char* name):channel_(channel){
    new Characteristic::ConfiguredName(name);on_=new Characteristic::On(false);
  }
  boolean update() override {
    if(!on_->updated())return true; // Home may write ConfiguredName while adding.
    // HAP accepts a queued request; completion remains the Controller's readback.
    // Never block HomeSpan or call setVal from its write callback.
    return control->request(channel_,on_->getNewVal<bool>(),millis());
  }
  void loop() override {
    const auto now=millis();const auto value=control->value(channel_,now);
    if(!value.observed || (value.fresh && value.pending))return;
    // Retain the SDK-accepted pending value until completion, then reconcile
    // from observation. Offline/failure paths retain no optimistic hold.
    if(on_->getVal<bool>()!=*value.observed)on_->setVal(*value.observed);
  }
};
class HealthReply final:public Service::Switch {
  SpanCharacteristic* on_;
 public:
  explicit HealthReply(const char* name){new Characteristic::ConfiguredName(name);on_=new Characteristic::On(false);}
  boolean update() override {return !on_->updated() || !on_->getNewVal<bool>() || control->acknowledge(millis());}
  void loop() override {if(on_->getVal<bool>())on_->setVal(false);}
};
class FridgeClimate final:public Service::HeaterCooler {
  bool automatic_;SpanCharacteristic *active_,*current_,*target_,*state_,*mode_,*units_;
 public:
  FridgeClimate(bool automatic,const char* name):automatic_(automatic){
    new Characteristic::ConfiguredName(name);
    active_=new Characteristic::Active(0);
    current_=(new Characteristic::CurrentTemperature(0))->setRange(-128,127,1);
    target_=(new Characteristic::CoolingThresholdTemperature(5))->setRange(-20,20,1);
    state_=new Characteristic::CurrentHeaterCoolerState(0);
    mode_=(new Characteristic::TargetHeaterCoolerState(2))->setValidValues(1,2);
    units_=(new Characteristic::TemperatureDisplayUnits(0))->setValidValues(1,0);
  }
  boolean update() override {
    if((mode_->updated() && mode_->getNewVal<int>()!=2) || (units_->updated() && units_->getNewVal<int>()!=0))return false;
    const auto target=target_->updated()?std::optional<float>(target_->getNewVal<float>()):std::nullopt;
    const auto power=active_->updated()?std::optional<bool>(active_->getNewVal<int>()!=0):std::nullopt;
    return control->requestClimate(target,power,automatic_,millis());
  }
  void loop() override {
    const auto r=control->climate(automatic_,millis());
    if(r.current && current_->getVal<float>()!=*r.current)current_->setVal(*r.current);
    if(r.target && !(r.fresh && r.targetPending) && target_->getVal<float>()!=*r.target)target_->setVal(*r.target);
    if(r.power){
      const int enabled=*r.power?1:0;if(!(r.fresh && r.powerPending) && active_->getVal<int>()!=enabled)active_->setVal(enabled);
      // Logical cooling enable only; no compressor-running bit has been decoded.
      const int cooling=*r.power?3:0;if(state_->getVal<int>()!=cooling)state_->setVal(cooling);
    }
  }
};
class HealthProbe final:public Service::StatelessProgrammableSwitch {
  SpanCharacteristic* event_;
 public:
  HealthProbe(){event_=new Characteristic::ProgrammableSwitchEvent();}
  void loop() override {if(control->probeDue(millis(),secure()))event_->setVal(0);}
};
}
void begin(ApiBackend backend){
#ifdef FROSTBRIDGE_HOMEKIT_ZH
  constexpr const char* names[]={"冰箱电源","冰箱节能","自动化电源","自动化节能","连通检测","检测回应","冰箱温控","自动化温控"};
#else
  constexpr const char* names[]={"Fridge Power","Fridge ECO","Automation Power","Automation ECO","Connectivity Probe","Probe Reply","Fridge Temperature","Automation Temperature"};
#endif
  control=std::make_unique<HomeKitControl>(std::move(backend),[]{return millis();});
  information(1,"FrostBridge");
  information(2,names[0]);new FridgeSwitch(HomeKitChannel::Power,names[0]);
  information(3,names[1]);new FridgeSwitch(HomeKitChannel::Eco,names[1]);
  information(4,names[2]);new FridgeSwitch(HomeKitChannel::AutomationPower,names[2]);
  information(5,names[3]);new FridgeSwitch(HomeKitChannel::AutomationEco,names[3]);
  information(6,names[4]);new HealthProbe();
  information(7,names[5]);new HealthReply(names[5]);
  information(8,names[6]);new FridgeClimate(false,names[6]);
  information(9,names[7]);new FridgeClimate(true,names[7]);
}
void poll(uint32_t now){if(control)control->tick(now);}
void diagnostics(WebSnapshot& snapshot,uint32_t now){
  if(!control)return;
  snapshot.homekit=true;snapshot.paired=homeSpan.controllerListBegin()!=homeSpan.controllerListEnd();snapshot.secureClient=secure();
  control->diagnostics(snapshot,now);
}
}
#endif
