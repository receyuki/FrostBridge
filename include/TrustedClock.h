#pragma once
#include <cstdint>
#include <optional>
namespace frost {
// Single worker owner. Advance at least once per uint32 millisecond cycle.
// No wall time is persisted. timeAt accepts a drained admission stamp up to
// 60 seconds old, only within the current sync epoch; an older/ambiguous stamp
// returns null instead of inventing a timestamp across a clock correction.
class TrustedClock {
 public:
  bool sync(int64_t unixSeconds,uint32_t nowMs,uint16_t fractionMs=0);
  void advance(uint32_t nowMs);
  std::optional<int64_t> timeAt(uint32_t admittedMs)const;
  bool trusted()const{return trusted_;}
  uint32_t revision()const{return revision_;}
 private:
  bool trusted_=false;uint32_t nowMs_=0,revision_=0;
  int64_t wallMs_=0;uint64_t sinceSyncMs_=0;
};
}
