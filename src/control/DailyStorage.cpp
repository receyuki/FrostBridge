#include "DailyStorage.h"
namespace frost {
namespace {
void put(uint8_t* out,size_t offset,uint32_t value,unsigned count){for(unsigned n=0;n<count;++n)out[offset+n]=static_cast<uint8_t>(value>>(8*n));}
uint32_t get(const uint8_t* in,size_t offset,unsigned count){uint32_t value=0;for(unsigned n=0;n<count;++n)value|=uint32_t(in[offset+n])<<(8*n);return value;}
uint32_t crc(const uint8_t* bytes,size_t size){
  uint32_t value=0xffffffff;
  for(size_t n=0;n<size;++n){value^=bytes[n];for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320u:0);}
  return ~value;
}
}
std::optional<size_t> encodeDailyState(const DailyPersistedState& state,DailyBytes& out){
  if(!DailyReplay::validState(state))return std::nullopt;
  for(size_t n=0;n<64;++n)out[n]=0;
  out[0]='F';out[1]='B';out[2]='D';out[3]=1;out[4]=(state.enabled?1:0)|(state.sample.valid?2:0)|(state.manual?4:0)|(state.overrideUntilMs?8:0);out[5]=state.sample.count;
  put(out.data(),8,state.generation,4);put(out.data(),12,static_cast<uint32_t>(state.sample.day),4);
  if(state.overrideUntilMs){const auto until=static_cast<uint64_t>(*state.overrideUntilMs);put(out.data(),16,static_cast<uint32_t>(until),4);put(out.data(),20,static_cast<uint32_t>(until>>32),4);}
  for(size_t n=0;n<state.sample.count;++n){
    const auto& a=state.sample.actions[n];const auto at=64+n*8;
    put(out.data(),at,a.second,4);out[at+4]=static_cast<uint8_t>(a.field);out[at+5]=static_cast<uint8_t>(a.value);put(out.data(),at+6,a.order,2);
  }
  const size_t size=68+size_t(state.sample.count)*8;put(out.data(),size-4,crc(out.data(),size-4),4);return size;
}
std::optional<DailyStateStamp> dailyStateStamp(const uint8_t* bytes,size_t size){
  if(!bytes || size<68 || size>dailyStateCapacity || bytes[0]!='F' || bytes[1]!='B' || bytes[2]!='D' || bytes[3]!=1 ||
      (bytes[4]&~15) || bytes[5]>64 || size!=68+size_t(bytes[5])*8 || get(bytes,size-4,4)!=crc(bytes,size-4))return std::nullopt;
  for(size_t n=6;n<8;++n)if(bytes[n])return std::nullopt;
  for(size_t n=(bytes[4]&8)?24:16;n<64;++n)if(bytes[n])return std::nullopt;
  return DailyStateStamp{get(bytes,8,4),get(bytes,size-4,4)};
}
bool decodeDailyState(const uint8_t* bytes,size_t size,DailyPersistedState& out){
  if(!dailyStateStamp(bytes,size))return false;
  const auto day=get(bytes,12,4);if(day>47482)return false;
  DailyPersistedState decoded;decoded.enabled=bytes[4]&1;decoded.sample.valid=bytes[4]&2;decoded.sample.count=bytes[5];decoded.generation=get(bytes,8,4);decoded.sample.day=static_cast<int32_t>(day);
  decoded.manual=bytes[4]&4;
  if(bytes[4]&8){const uint64_t until=uint64_t(get(bytes,16,4))|(uint64_t(get(bytes,20,4))<<32);if(until>4102531200000ULL)return false;decoded.overrideUntilMs=static_cast<int64_t>(until);}
  for(size_t n=0;n<decoded.sample.count;++n){
    const auto at=64+n*8;const int value=bytes[at+5]<128?int(bytes[at+5]):int(bytes[at+5])-256;
    decoded.sample.actions[n]={get(bytes,at,4),static_cast<DailyField>(bytes[at+4]),static_cast<int8_t>(value),static_cast<uint16_t>(get(bytes,at+6,2))};
  }
  if(!DailyReplay::validState(decoded))return false;
  out=decoded;return true;
}
bool DailyReplay::restoreEncoded(const uint8_t* bytes,size_t size){
  if(!decodeDailyState(bytes,size,state_))return false;
  resetAfterRestore();return true;
}

}
