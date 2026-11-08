#include "TrustedClock.h"
namespace frost {
bool TrustedClock::sync(int64_t seconds,uint32_t nowMs,uint16_t fractionMs){
  if(fractionMs>=1000 || seconds<1577836800 || seconds>4102444800LL)return false;
  trusted_=true;nowMs_=nowMs;wallMs_=seconds*1000+fractionMs;sinceSyncMs_=0;++revision_;return true;
}
void TrustedClock::advance(uint32_t nowMs){
  const auto elapsed=static_cast<uint32_t>(nowMs-nowMs_);nowMs_=nowMs;
  if(trusted_){wallMs_+=elapsed;sinceSyncMs_+=elapsed;if(wallMs_>4102444800000LL)trusted_=false;}
}
std::optional<int64_t> TrustedClock::timeAt(uint32_t admittedMs)const{
  const auto age=static_cast<uint32_t>(nowMs_-admittedMs);
  if(!trusted_ || age>60000 || age>sinceSyncMs_)return std::nullopt;
  return wallMs_-age;
}
}
