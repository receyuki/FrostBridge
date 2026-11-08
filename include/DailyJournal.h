#pragma once
#include "DailyStorage.h"
namespace frost {
enum class DailyLoad :uint8_t { Empty,Loaded,Invalid };
class DailyJournal {
 public:
  // Read returns absent=nullopt; an invalid length signals a present unreadable
  // slot. Callbacks are synchronous, bounded and never reenter the journal.
  using Read=std::function<std::optional<size_t>(uint8_t,DailyBytes&)>;
  using Write=std::function<bool(uint8_t,const DailyBytes&,size_t)>;
  DailyJournal(Read read,Write write):read_(std::move(read)),write_(std::move(write)){}
  DailyLoad load(DailyReplay& owner);
  bool save(const DailyPersistedState& state);
 private:
  Read read_;Write write_;DailyBytes work_{};int8_t active_=-1;uint32_t generation_=0;
};
}
