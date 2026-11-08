#include "ScheduleStorage.h"
namespace frost {
namespace {
uint32_t crc32(const std::vector<uint8_t>& bytes,size_t count) {
  uint32_t crc=0xffffffff;
  for(size_t i=0;i<count;++i) {
    crc^=bytes[i];
    for(int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
  }
  return ~crc;
}
void put(std::vector<uint8_t>& out,uint64_t value,unsigned count) {
  for(unsigned i=0;i<count;++i) out.push_back(static_cast<uint8_t>(value>>(8*i)));
}
uint64_t get(const std::vector<uint8_t>& in,size_t offset,unsigned count) {
  uint64_t value=0;for(unsigned i=0;i<count;++i) value|=uint64_t(in[offset+i])<<(8*i);return value;
}
}
std::optional<std::vector<uint8_t>> encodeScheduleState(const ScheduleState& state) {
  if(!WeeklySchedule::validState(state)) return std::nullopt;
  std::vector<uint8_t> out{'F','B','S',1,static_cast<uint8_t>(state.plan.periods.size()),
    static_cast<uint8_t>((state.plan.enabled?1:0)|(state.manual?2:0)|(state.overrideUntil?4:0))};
  put(out,state.generation,4);put(out,state.overrideUntil?static_cast<uint64_t>(*state.overrideUntil):0,8);
  for(const auto& p:state.plan.periods) {
    out.push_back(p.days);put(out,p.startMinute,2);put(out,p.endMinute,2);
    out.push_back(static_cast<uint8_t>(p.targetC));out.push_back(static_cast<uint8_t>(*p.mode));
  }
  put(out,crc32(out,out.size()),4);return out;
}
std::optional<ScheduleState> decodeScheduleState(const std::vector<uint8_t>& bytes) {
  if(bytes.size()<22 || bytes.size()>134 || bytes[0]!='F' || bytes[1]!='B' || bytes[2]!='S' || bytes[3]!=1 ||
     bytes[4]>16 || bytes.size()!=22+7*size_t(bytes[4]) || (bytes[5]&~7) ||
     get(bytes,bytes.size()-4,4)!=crc32(bytes,bytes.size()-4)) return std::nullopt;
  ScheduleState state;state.plan.enabled=bytes[5]&1;state.manual=bytes[5]&2;state.generation=get(bytes,6,4);
  const auto expiry=get(bytes,10,8);
  if(bytes[5]&4) {
    if(expiry>4102444800ULL+7*86400) return std::nullopt;
    state.overrideUntil=static_cast<int64_t>(expiry);
  } else if(expiry) return std::nullopt;
  for(size_t i=0;i<bytes[4];++i) {
    const auto offset=18+7*i;
    if(bytes[offset+6]>1) return std::nullopt;
    const int target=bytes[offset+5]<128?bytes[offset+5]:int(bytes[offset+5])-256;
    state.plan.periods.push_back({bytes[offset],static_cast<uint16_t>(get(bytes,offset+1,2)),
      static_cast<uint16_t>(get(bytes,offset+3,2)),target,static_cast<alpicool::Mode>(bytes[offset+6])});
  }
  return WeeklySchedule::validState(state)?std::optional<ScheduleState>(std::move(state)):std::nullopt;
}
}
