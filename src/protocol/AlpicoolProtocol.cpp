#include "AlpicoolProtocol.h"
#include <cstring>
namespace alpicool {
std::array<uint8_t, 6> query() { return {0xfe, 0xfe, 0x03, 0x01, 0x02, 0x00}; }
bool validFrame(const Frame& f) {
  if (f.size() < 6 || f.size() > 258 || f[0] != 0xfe || f[1] != 0xfe ||
      f[2] < 3 || f.size() != static_cast<size_t>(f[2]) + 3) return false;
  uint16_t sum = 0;
  for (size_t i = 0; i + 2 < f.size(); ++i) sum = static_cast<uint16_t>(sum + f[i]);
  return sum == static_cast<uint16_t>((f[f.size()-2] << 8) | f.back());
}
std::vector<Frame> Decoder::feed(const uint8_t* bytes, size_t length) {
  std::vector<Frame> out;
  for(auto& frame:feedStamped(bytes,length,{})) out.push_back(std::move(frame.bytes));
  return out;
}
std::vector<ReceivedFrame> Decoder::feedStamped(const uint8_t* bytes,size_t length,Receipt receipt) {
  std::vector<ReceivedFrame> out;
  auto discard = [this](size_t n) {
    std::memmove(buffer_.data(), buffer_.data()+n, used_-n);
    std::memmove(receipts_.data(),receipts_.data()+n,(used_-n)*sizeof(Receipt));
    used_ -= n;
  };
  for (size_t i=0; i<length; ++i) {
    if (used_ == buffer_.size()) discard(1);
    receipts_[used_]={static_cast<uint32_t>(receipt.sequence+i),receipt.receivedMs};buffer_[used_++] = bytes[i];
    while (used_ >= 2) {
      if (buffer_[0] != 0xfe || buffer_[1] != 0xfe) { discard(1); continue; }
      if (used_ < 3) break;
      if (buffer_[2] < 3) { discard(1); continue; }
      const size_t total = static_cast<size_t>(buffer_[2])+3;
      if (used_ < total) break;
      Frame f(buffer_.begin(), buffer_.begin()+total);
      if (validFrame(f)) { out.push_back({std::move(f),receipts_[0]}); discard(total); }
      else discard(1);
    }
  }
  return out;
}
std::optional<Status> decodeStatus(const Frame& f) {
  if (!validFrame(f) || f.size()<24 || f[3]!=1) return std::nullopt;
  const uint8_t* d=f.data()+4;
  Status s;
  if (d[0]<=1) s.locked=d[0]!=0;
  if (d[1]<=1) s.power=d[1]!=0;
  if (d[2]<=1) s.mode=d[2]==0 ? Mode::Max : Mode::Eco;
  if (d[3]<=2) s.protection=d[3];
  s.unit=d[9];
  auto temperature=[&](uint8_t raw) {
    const int signedValue=raw<128 ? raw : static_cast<int>(raw)-256;
    return s.unit==0 ? static_cast<float>(signedValue) : (signedValue-32)*5.0f/9.0f;
  };
  if (s.unit<=1) {
    s.targetC=temperature(d[4]); s.currentC=temperature(d[14]);
    const float maximum=temperature(d[5]), minimum=temperature(d[6]);
    if (minimum<=maximum) { s.minC=minimum; s.maxC=maximum; }
  }
  if (d[15]<=100) s.batteryPercent=d[15];
  if (d[17]<=9) s.voltage=d[16]+d[17]/10.0f;
  return s;
}

std::optional<Frame> encodeSetting(const Frame& snapshot,Setting setting,int value) {
  const auto state=decodeStatus(snapshot);
  if (!state || state->unit!=0 || !state->locked || *state->locked) return std::nullopt;
  Frame frame;
  switch(setting) {
    case Setting::TargetC:
      if(!state->minC || !state->maxC || value < -20 || value > 20 || value<*state->minC || value>*state->maxC) return std::nullopt;
      frame={0xfe,0xfe,0x04,0x05,static_cast<uint8_t>(value)};
      break;
    case Setting::Power:
    case Setting::Mode:
      // buildSet model==1 (pinned reference): 14 settings bytes even with
      // a 36-byte Query. Other extended model mappings are not implemented.
      if(!(snapshot.size()==24 || (snapshot.size()==36 && snapshot[32]==1)) || (value!=0 && value!=1) || !state->power || !state->mode || !state->protection ||
         !state->targetC || !state->minC || !state->maxC || *state->targetC<*state->minC || *state->targetC>*state->maxC) return std::nullopt;
      frame={0xfe,0xfe,0x11,0x02};
      frame.insert(frame.end(),snapshot.begin()+4,snapshot.begin()+18);
      frame[setting==Setting::Power?5:6]=static_cast<uint8_t>(value);
      break;
    default:return std::nullopt;
  }
  uint16_t sum=0;
  for(const auto byte:frame) sum=static_cast<uint16_t>(sum+byte);
  frame.push_back(static_cast<uint8_t>(sum>>8));frame.push_back(static_cast<uint8_t>(sum));
  return frame;
}

}
