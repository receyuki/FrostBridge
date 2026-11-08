#ifdef FROSTBRIDGE_GATEWAY
#include "NvsDailyStore.h"
#include <nvs.h>
namespace frost {
namespace {const char* key(uint8_t slot){return slot==0?"slot0":"slot1";}}
NvsDailyStore::NvsDailyStore():journal_(
 [this](uint8_t slot,DailyBytes& out)->std::optional<size_t>{
  if(!open_ || slot>1)return size_t(0);
  if(!prefs_.isKey(key(slot)))return std::nullopt;
  const auto size=prefs_.getBytesLength(key(slot));
  if(size<68 || size>out.size() || prefs_.getBytes(key(slot),out.data(),size)!=size)return size_t(0);
  return size;
 },[this](uint8_t slot,const DailyBytes& bytes,size_t size){
  return open_ && writable_ && slot<=1 && size<=bytes.size() && prefs_.putBytes(key(slot),bytes.data(),size)==size;
 }){}
DailyStoreStatus NvsDailyStore::load(DailyReplay& owner){
 if(!open_)open_=prefs_.begin("fb-daily",false);
 if(!open_)return status_=DailyStoreStatus::Unavailable;
 const auto loaded=journal_.load(owner);
 if(loaded==DailyLoad::Invalid){writable_=false;return status_=DailyStoreStatus::Invalid;}
 if(loaded==DailyLoad::Empty){
  nvs_stats_t stats{};writable_=nvs_get_stats(nullptr,&stats)==ESP_OK && stats.available_entries>=96;
  return status_=writable_?DailyStoreStatus::Empty:DailyStoreStatus::Unavailable;
 }
 writable_=true;return status_=DailyStoreStatus::Loaded;
}
bool NvsDailyStore::verifyCapacity(){
 nvs_stats_t stats{};if(nvs_get_stats(nullptr,&stats)!=ESP_OK || stats.available_entries<96)return false;
 Preferences test;if(!test.begin("fb-dtest",false))return false;
 DailyBytes bytes{};bool ok=true;
 auto pattern=[&](unsigned seed){for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<uint8_t>((i*37+seed*53)&255);};
 auto verify=[&](unsigned slot,unsigned seed){
  if(test.getBytesLength(key(slot))!=bytes.size() || test.getBytes(key(slot),bytes.data(),bytes.size())!=bytes.size())return false;
  for(size_t i=0;i<bytes.size();++i)if(bytes[i]!=static_cast<uint8_t>((i*37+seed*53)&255))return false;
  return true;
 };
 for(unsigned round=0;round<3 && ok;++round)for(unsigned slot=0;slot<2 && ok;++slot){
  const unsigned seed=round*2+slot;pattern(seed);ok=test.putBytes(key(slot),bytes.data(),bytes.size())==bytes.size() && verify(slot,seed);
  if(ok && (round || slot))ok=verify(1-slot,seed-1);
 }
 for(unsigned slot=0;slot<2;++slot)if(test.isKey(key(slot)))ok=test.remove(key(slot)) && ok;
 test.end();return ok;
}
bool NvsDailyStore::save(const DailyPersistedState& state){
 if(!open_ || !writable_ || !journal_.save(state))return false;
 status_=DailyStoreStatus::Loaded;return true;
}
}
#endif
