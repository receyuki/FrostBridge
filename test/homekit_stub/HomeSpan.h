#pragma once
// External SDK seam: records HAP characteristic publication, not Controller logic.
#include <vector>
#include <memory>
#include <string>
#include <utility>
using boolean=bool;
inline uint32_t stubNow=100;
inline uint32_t millis(){return stubNow;}
struct SpanService;
struct SpanCharacteristic {
  double value=0,incoming=0;bool changed=false;std::string type,text;
  SpanCharacteristic(const char* t,double v=0);
  virtual ~SpanCharacteristic()=default;
  template<class T> T getVal()const{return static_cast<T>(value);}
  template<class T> T getNewVal()const{return static_cast<T>(incoming);}
  bool updated()const{return changed;}
  void setVal(double v){value=v;}
  SpanCharacteristic* setRange(double,double,double){return this;}
  SpanCharacteristic* setValidValues(int,...){return this;}
};
struct SpanService {
  uint32_t aid;std::vector<SpanCharacteristic*> characteristics;
  SpanService();virtual ~SpanService()=default;
  virtual boolean update(){return true;}virtual void loop(){}
};
inline uint32_t stubAid=0;inline SpanService* stubService=nullptr;
inline std::vector<std::unique_ptr<SpanService>> stubServices;
inline std::vector<std::unique_ptr<SpanCharacteristic>> stubCharacteristics;
inline SpanService::SpanService():aid(stubAid){stubService=this;stubServices.emplace_back(this);}
inline SpanCharacteristic::SpanCharacteristic(const char* t,double v):value(v),incoming(v),type(t){stubService->characteristics.push_back(this);stubCharacteristics.emplace_back(this);}
struct SpanAccessory{explicit SpanAccessory(uint32_t);};
inline std::vector<std::unique_ptr<SpanAccessory>> stubAccessories;
inline SpanAccessory::SpanAccessory(uint32_t a){stubAid=a;stubAccessories.emplace_back(this);}
namespace Service {
struct AccessoryInformation:SpanService{};struct Switch:SpanService{};
struct HeaterCooler:SpanService{};struct StatelessProgrammableSwitch:SpanService{};
}
namespace Characteristic {
#define NUMERIC(Name) struct Name:SpanCharacteristic{explicit Name(double v=0):SpanCharacteristic(#Name,v){}};
#define TEXT(Name) struct Name:SpanCharacteristic{explicit Name(const char* v):SpanCharacteristic(#Name){text=v;}};
NUMERIC(Identify) NUMERIC(On) NUMERIC(Active) NUMERIC(CurrentTemperature)
NUMERIC(CoolingThresholdTemperature) NUMERIC(CurrentHeaterCoolerState)
NUMERIC(TargetHeaterCoolerState) NUMERIC(TemperatureDisplayUnits) NUMERIC(ProgrammableSwitchEvent)
TEXT(Name) TEXT(Manufacturer) TEXT(Model) TEXT(FirmwareRevision) TEXT(ConfiguredName)
#undef NUMERIC
#undef TEXT
}
constexpr int HS_CONNECTED=1;
inline struct {std::pair<int,int> getStatus(){return {HS_CONNECTED,0};}int controllerListBegin(){return 0;}int controllerListEnd(){return 1;}} homeSpan;
