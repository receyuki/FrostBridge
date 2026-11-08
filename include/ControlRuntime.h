#pragma once
#include "DailyReplay.h"
#include "TrustedClock.h"
namespace frost {
struct NetworkTimeSample { uint32_t seconds=0,receivedMs=0,sequence=0;uint16_t fractionMs=0; };
enum class BackupError:uint8_t {None,StalePreview,PersistenceFailed};
struct BackupPreview {bool enabled=false;uint32_t generation=0,clockRevision=0,token=0;ReplayDecision effect;};
// Single BLE-worker owner. Writer and DailyReplay Save must never reenter it.
// Intent/observation/terminal processing and learnThrough/applyBackup may
// synchronously persist through bounded Save/NVS. Never call from HAP callbacks
// or critical sections. submit/applyBackup return persistence errors directly;
// asynchronous coverage/override failures are visible via daily().storageHealthy().
class ControlRuntime {
 public:
  ControlRuntime(uint32_t boot,Controller::Writer writer,DailyReplay& daily,bool writesEnabled=false);
  Submission submit(Intent intent,uint32_t nowMs,std::optional<uint32_t> admittedMs=std::nullopt,std::optional<uint32_t> expectedEpoch=std::nullopt);
  void tick(uint32_t nowMs);
  void learnThrough(uint32_t safeMs,uint32_t nowMs);
  // Rejected/lost admission interval, not controller execution time. Applied
  // before sealing only to intersecting days; undatable intervals are gaps.
  void inputLoss(uint32_t firstMs,uint32_t lastMs);
  // Repeat the original sample stamp/ack age when forwarding cached evidence;
  // refresh networkReady from the worker's current Wi-Fi state each iteration.
  void automationLink(AutomationHealth link,bool networkReady,uint32_t sampledMs,std::optional<uint32_t> ackAgeMs=0);
  void connection(bool ready,uint32_t epoch,uint32_t nowMs);
  bool observe(const alpicool::Frame& frame,uint32_t epoch,uint32_t sequence,uint32_t receivedMs,std::optional<uint32_t> processedMs=std::nullopt);
  bool syncTime(int64_t unixSeconds,uint32_t nowMs);
  bool syncNetworkTime(NetworkTimeSample sample,uint32_t nowMs);
  BackupPreview previewBackup(bool enabled,uint32_t nowMs);
  BackupError applyBackup(const BackupPreview& preview,uint32_t nowMs,std::optional<uint32_t> admittedMs=std::nullopt);
  const Observation& observed()const{return control_.observed();}
  std::optional<Command> command(CommandId id)const{return control_.command(id);}
  std::vector<Command> commands()const{return control_.commands();}
  const DailyReplay& daily()const{return daily_;}
  ReplayDecision decision(uint32_t nowMs){return daily_.decision(inputs(nowMs,nowMs));}
  Error lastAutoError()const{return autoError_;}
  bool writesEnabled()const{return writesEnabled_;}
  bool timeTrusted()const{return clock_.trusted();}
  std::optional<int64_t> unixTime(uint32_t nowMs){clock_.advance(nowMs);const auto time=clock_.timeAt(nowMs);return time?std::optional<int64_t>(*time/1000):std::nullopt;}
  Error checkControl(Intent intent,uint32_t nowMs)const{return writesEnabled_?control_.check(intent,nowMs):Error::WritesDisabled;}
 private:
  bool writesEnabled_;Controller control_;DailyReplay& daily_;TrustedClock clock_;
  struct Learning {std::optional<CommandId> id;std::optional<LearningToken> token;uint32_t admittedMs=0;};
  std::array<Learning,5> learning_{};
  AutomationHealth link_=AutomationHealth::Unknown;bool networkReady_=false,haveLink_=false;uint32_t linkMs_=0,lastNetworkSequence_=0;
  uint32_t linkAdvanceMs_=0,ackAgeMs_=0;uint64_t linkElapsedMs_=0;
  Error autoError_=Error::None;
  std::optional<ReplayDecision> lastDecision_;std::optional<CommandId> replayId_;
  uint32_t replayRevision_=0,dispatchedRevision_=0;uint8_t replayMask_=0,replayBit_=0;bool recover_=true;
  std::optional<int64_t> lastReplayWall_;uint32_t lastClockRevision_=0;
  bool boundary_=false;uint8_t boundaryIndex_=0,boundaryEnd_=0;
  bool lossPending_=false;uint32_t lossFirstMs_=0,lossLastMs_=0;
  bool retryReplay_=false;uint32_t replayRetryAtMs_=0;
  uint32_t nextPreview_=0;std::optional<BackupPreview> preview_;
  DailyInputs inputs(uint32_t stamp,uint32_t nowMs);
  void consumeLearning(uint32_t nowMs);
  void consumeReplay(uint32_t nowMs);
  void cancelExpiredManual(const DailyInputs& now);
  void prepareLearningInput(const DailyInputs& input);
};
}
