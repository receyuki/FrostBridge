#include "DailyJournal.h"
namespace frost {
DailyLoad DailyJournal::load(DailyReplay& owner){
  active_=-1;generation_=0;bool present=false;
  if(!read_)return DailyLoad::Invalid;
  for(uint8_t slot=0;slot<2;++slot){
    const auto size=read_(slot,work_);if(!size)continue;present=true;
    const auto stamp=dailyStateStamp(work_.data(),*size);if(!stamp)continue;
    if(active_>=0 && stamp->generation<=generation_)continue;
    if(owner.restoreEncoded(work_.data(),*size)){active_=slot;generation_=stamp->generation;}
  }
  return active_>=0?DailyLoad::Loaded:present?DailyLoad::Invalid:DailyLoad::Empty;
}
bool DailyJournal::save(const DailyPersistedState& state){
  if(!read_ || !write_ || state.generation<=generation_)return false;
  const auto size=encodeDailyState(state,work_);if(!size)return false;
  const auto expected=dailyStateStamp(work_.data(),*size);if(!expected)return false;
  const uint8_t slot=active_==0?1:0;
  if(!write_(slot,work_,*size))return false;
  const auto count=read_(slot,work_);if(!count || *count!=*size)return false;
  const auto actual=dailyStateStamp(work_.data(),*count);
  if(!actual || actual->generation!=expected->generation || actual->checksum!=expected->checksum)return false;
  active_=slot;generation_=state.generation;return true;
}
}
