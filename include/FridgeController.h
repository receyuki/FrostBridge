#pragma once
#include "AlpicoolProtocol.h"
#include <functional>
#include <optional>
#include <deque>

namespace frost {
enum class Error { None, Offline, NoState, Stale, UnsupportedUnit, Locked, UnknownValue,
                   OutOfRange, InvalidValue, UnsupportedLayout, TransportFailed, ReadbackTimeout, Disconnected,
                   QueueFull, Superseded, ReadbackRequired, WritesDisabled, PersistenceFailed,RequestExpired };
struct CommandId { uint32_t boot=0, sequence=0; };
inline bool operator==(CommandId a,CommandId b) { return a.boot==b.boot && a.sequence==b.sequence; }
enum class CommandState { Queued, AwaitingReadback, Confirmed, Failed, TimedOut, Cancelled };
enum class Source { Manual, Web, HomeKit, Schedule, HomeKitAutomation,LocalReplay };
struct Intent { alpicool::Setting setting; int value; Source source=Source::Manual; };
struct Submission { std::optional<CommandId> id; Error error=Error::None; };
struct Command {
  CommandId id;
  Intent intent;
  CommandState state=CommandState::Queued;
  Error error=Error::None;
  uint32_t sentMs=0, finishedMs=0;
};
struct Observation {
  alpicool::Status status;
  alpicool::Frame frame;
  uint32_t epoch=0, sequence=0, receivedMs=0;
  bool valid=false;
};
// Single owner; the transport function must not reenter the Controller.
class Controller {
 public:
  using Writer=std::function<bool(const alpicool::Frame&)>;
  Controller(uint32_t bootSession, Writer writer);
  Submission submit(Intent intent, uint32_t nowMs);
  Error check(Intent intent,uint32_t nowMs) const;
  void cancelQueued(uint32_t nowMs,std::optional<Source> source=std::nullopt);
  std::optional<Command> inFlight() const { return flight_?command(*flight_):std::nullopt; }
  bool matches(Intent intent) const; // value comparison; use check/fresh for control.
  void tick(uint32_t nowMs);
  void connection(bool ready, uint32_t epoch, uint32_t nowMs);
  // Sequence/time are captured at transport ingress, before parsing; epochs
  // advance on reconnect. Invalid/stale deliveries cannot reopen control.
  bool observe(const alpicool::Frame& frame, uint32_t epoch, uint32_t sequence, uint32_t receivedMs);
  std::optional<Command> command(CommandId id) const;
  const Observation& observed() const { return observed_; }
  bool fresh(uint32_t nowMs) const;
  std::vector<Command> commands() const { return {history_.begin(),history_.end()}; }
 private:
  uint32_t boot_, nextId_=0, epoch_=0;
  bool ready_=false;
  Writer writer_;
  Observation observed_;
  std::deque<Command> history_;
  std::deque<CommandId> queue_;
  std::optional<CommandId> flight_;
  uint32_t sentSequence_=0, lastSendMs_=0;
  bool needsPostWrite_=false;
  Command* find(CommandId id);
  Error validate(Intent intent,uint32_t nowMs) const;
  void uncertainOutcome(Command& command,CommandState state,Error error,uint32_t nowMs);
};
}
