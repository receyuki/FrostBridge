#include "FridgeController.h"
#include <algorithm>
#include <limits>
namespace frost {
Controller::Controller(uint32_t boot, Writer writer):boot_(boot),writer_(std::move(writer)) {}
Submission Controller::submit(Intent intent, uint32_t nowMs) {
  const auto error=check(intent,nowMs);
  if(error!=Error::None) return {std::nullopt,error};
  const bool merge=intent.setting==alpicool::Setting::TargetC && !queue_.empty() && find(queue_.back())->intent.setting==alpicool::Setting::TargetC;
  if(merge) {
    auto* old=find(queue_.back());old->state=CommandState::Cancelled;old->error=Error::Superseded;old->finishedMs=nowMs;queue_.pop_back();
  }
  if(history_.size()>=16) {
    const auto terminal=std::find_if(history_.begin(),history_.end(),[](const Command& c) { return c.state!=CommandState::Queued && c.state!=CommandState::AwaitingReadback; });
    if(terminal==history_.end()) return {std::nullopt,Error::QueueFull};
    history_.erase(terminal);
  }
  const CommandId id{boot_,++nextId_};
  history_.push_back({id,intent});queue_.push_back(id);
  return {id,Error::None};
}
Error Controller::check(Intent intent,uint32_t nowMs) const {
  const auto error=validate(intent,nowMs);if(error!=Error::None) return error;
  const bool merge=intent.setting==alpicool::Setting::TargetC && !queue_.empty() && command(queue_.back())->intent.setting==alpicool::Setting::TargetC;
  return (queue_.size()>=4 && !merge) || nextId_==std::numeric_limits<uint32_t>::max()?Error::QueueFull:Error::None;
}
void Controller::cancelQueued(uint32_t nowMs,std::optional<Source> source) {
  for(auto it=queue_.begin();it!=queue_.end();) {
    auto* c=find(*it);
    if(!source || c->intent.source==*source) {
      c->state=CommandState::Cancelled;c->error=Error::Superseded;c->finishedMs=nowMs;it=queue_.erase(it);
    } else ++it;
  }
}
void Controller::connection(bool ready,uint32_t epoch,uint32_t nowMs) {
  if(!ready || epoch!=epoch_) {
    for(auto& c:history_) if(c.state==CommandState::Queued || c.state==CommandState::AwaitingReadback) {
      c.state=CommandState::Cancelled;c.error=Error::Disconnected;c.finishedMs=nowMs;
    }
    queue_.clear();flight_.reset();observed_.valid=false;needsPostWrite_=false;
  }
  ready_=ready;epoch_=epoch;
}
bool Controller::observe(const alpicool::Frame& frame,uint32_t epoch,uint32_t sequence,uint32_t receivedMs) {
  if (!ready_ || epoch!=epoch_) return false;
  if (!observed_.frame.empty() && observed_.epoch==epoch &&
      (static_cast<int32_t>(sequence-observed_.sequence)<=0 || static_cast<int32_t>(receivedMs-observed_.receivedMs)<0)) return false;
  if(needsPostWrite_ && (static_cast<int32_t>(sequence-sentSequence_)<=0 || static_cast<int32_t>(receivedMs-lastSendMs_)<=0)) return false;
  const auto status=alpicool::decodeStatus(frame);
  if (!status) return false;
  needsPostWrite_=false;
  observed_={*status,frame,epoch,sequence,receivedMs,true};
  if(flight_) {
    auto* c=find(*flight_);
    if(static_cast<int32_t>(receivedMs-c->sentMs)>0) {
      if(receivedMs-c->sentMs>=5000) {
        uncertainOutcome(*c,CommandState::TimedOut,Error::ReadbackTimeout,receivedMs);
      } else if(static_cast<int32_t>(sequence-sentSequence_)>0 && matches(c->intent)) {
        c->state=CommandState::Confirmed;c->finishedMs=receivedMs;flight_.reset();
      }
    }
  }
  return true;
}
std::optional<Command> Controller::command(CommandId id) const {
  for(const auto& c:history_) if(c.id==id) return c;
  return std::nullopt;
}
Command* Controller::find(CommandId id) {
  for(auto& c:history_) if(c.id==id) return &c;
  return nullptr;
}
bool Controller::matches(Intent intent) const {
  const auto& s=observed_.status;
  switch(intent.setting) {
    case alpicool::Setting::TargetC:return s.targetC && *s.targetC==intent.value;
    case alpicool::Setting::Power:return s.power && *s.power==(intent.value!=0);
    case alpicool::Setting::Mode:return s.mode && static_cast<int>(*s.mode)==intent.value;
  }
  return false;
}
bool Controller::fresh(uint32_t nowMs) const {
  return ready_ && observed_.valid && observed_.epoch==epoch_ && nowMs-observed_.receivedMs<=30000;
}
Error Controller::validate(Intent intent,uint32_t nowMs) const {
  if(!ready_ || !writer_) return Error::Offline;
  if(!observed_.valid || observed_.epoch!=epoch_) return Error::NoState;
  if(!fresh(nowMs)) return Error::Stale;
  const auto& s=observed_.status;
  if(s.unit!=0) return Error::UnsupportedUnit;
  if(!s.locked) return Error::UnknownValue;
  if(*s.locked) return Error::Locked;
  switch(intent.setting) {
    case alpicool::Setting::TargetC:
      if(!s.targetC || !s.minC || !s.maxC) return Error::UnknownValue;
      if(intent.value < -20 || intent.value > 20 || intent.value<*s.minC || intent.value>*s.maxC) return Error::OutOfRange;
      break;
    case alpicool::Setting::Power:
      if(intent.value!=0 && intent.value!=1) return Error::InvalidValue;
      if(!s.power) return Error::UnknownValue;
      break;
    case alpicool::Setting::Mode:
      if(intent.value!=0 && intent.value!=1) return Error::InvalidValue;
      if(!s.mode) return Error::UnknownValue;
      break;
    default:return Error::InvalidValue;
  }
  if(!matches(intent) && !alpicool::encodeSetting(observed_.frame,intent.setting,intent.value)) return Error::UnsupportedLayout;
  return Error::None;
}
void Controller::uncertainOutcome(Command& command,CommandState state,Error error,uint32_t nowMs) {
  const bool postSend=observed_.valid && observed_.epoch==epoch_ &&
    static_cast<int32_t>(observed_.sequence-sentSequence_)>0 && static_cast<int32_t>(observed_.receivedMs-command.sentMs)>0;
  if(!postSend) { observed_.valid=false;needsPostWrite_=true; }
  command.state=state;command.error=error;command.finishedMs=nowMs;flight_.reset();
  for(const auto id:queue_) {
    auto* queued=find(id);queued->state=CommandState::Cancelled;queued->error=Error::ReadbackRequired;queued->finishedMs=nowMs;
  }
  queue_.clear();
}
void Controller::tick(uint32_t nowMs) {
  if(flight_) {
    auto* c=find(*flight_);
    if(nowMs-c->sentMs<5000) return;
    uncertainOutcome(*c,CommandState::TimedOut,Error::ReadbackTimeout,nowMs);
  }
  if(flight_ || queue_.empty()) return;
  const auto id=queue_.front();queue_.pop_front();auto* c=find(id);
  const auto error=validate(c->intent,nowMs);
  if(error!=Error::None) { c->state=CommandState::Failed;c->error=error;c->finishedMs=nowMs;return; }
  const bool alreadyCurrent=matches(c->intent);
  const bool requiresQuery=c->intent.source==Source::HomeKit || c->intent.source==Source::HomeKitAutomation || c->intent.source==Source::LocalReplay;
  if(alreadyCurrent && !requiresQuery) { c->state=CommandState::Confirmed;c->finishedMs=nowMs;return; }
  std::optional<alpicool::Frame> frame;
  if(alreadyCurrent){const auto query=alpicool::query();frame=alpicool::Frame(query.begin(),query.end());}
  else frame=alpicool::encodeSetting(observed_.frame,c->intent.setting,c->intent.value);
  if(!frame) { c->state=CommandState::Failed;c->error=Error::UnsupportedLayout;c->finishedMs=nowMs;return; }
  c->sentMs=nowMs;c->state=CommandState::AwaitingReadback;flight_=id;sentSequence_=observed_.sequence;lastSendMs_=nowMs;
  if(!writer_(*frame)) uncertainOutcome(*c,CommandState::Failed,Error::TransportFailed,nowMs);
}
}
