#include "ControlRuntime.h"
namespace frost {
ControlRuntime::ControlRuntime(uint32_t boot,Controller::Writer writer,DailyReplay& daily,bool writesEnabled):
 writesEnabled_(writesEnabled),control_(boot,[this,writer=std::move(writer)](const alpicool::Frame& frame){return writesEnabled_ && writer && writer(frame);}),daily_(daily){}
DailyInputs ControlRuntime::inputs(uint32_t stamp,uint32_t nowMs){
  clock_.advance(nowMs);
  if(haveLink_){linkElapsedMs_+=static_cast<uint32_t>(nowMs-linkAdvanceMs_);linkAdvanceMs_=nowMs;}
  const auto health=link_==AutomationHealth::Healthy && linkElapsedMs_+ackAgeMs_>180000?AutomationHealth::Expired:link_;
  return {stamp,clock_.timeAt(stamp),haveLink_?health:AutomationHealth::Unknown,haveLink_ && networkReady_,control_.fresh(nowMs)};
}
void ControlRuntime::consumeLearning(uint32_t nowMs){
  clock_.advance(nowMs);
  for(auto& pending:learning_)if(pending.id){
    const auto result=control_.command(*pending.id);
    if(!result || (result->state!=CommandState::Queued && result->state!=CommandState::AwaitingReadback)){
      const bool confirmed=result && result->state==CommandState::Confirmed;
      if(pending.token)daily_.complete(*pending.token,confirmed,LearningCompletion{nowMs,result?clock_.timeAt(result->finishedMs):std::nullopt});
      if(confirmed)daily_.confirmAutomation(pending.admittedMs,nowMs);
      pending.id.reset();
    }
  }
}
void ControlRuntime::consumeReplay(uint32_t nowMs){
  if(replayId_){
    const auto result=control_.command(*replayId_);
    if(result && (result->state==CommandState::Queued || result->state==CommandState::AwaitingReadback))return;
    if(dispatchedRevision_==replayRevision_){
      if(result && result->state==CommandState::Confirmed){if(boundary_)++boundaryIndex_;else replayMask_&=uint8_t(~replayBit_);}
      else{
        replayMask_=0;boundary_=false;autoError_=result?result->error:Error::ReadbackRequired;
        if(!result || result->state!=CommandState::Cancelled){
          retryReplay_=true;replayRetryAtMs_=nowMs+5000;
          if(control_.fresh(nowMs))recover_=true;
        }
      }
    }
    replayId_.reset();
  }
}
void ControlRuntime::cancelExpiredManual(const DailyInputs& in){
  const auto& state=daily_.state();
  if(!state.manual || !state.overrideUntilMs || !in.wallMs || *in.wallMs<*state.overrideUntilMs)return;
  for(const auto source:{Source::Manual,Source::Web,Source::HomeKit})control_.cancelQueued(in.nowMs,source);
}
void ControlRuntime::inputLoss(uint32_t firstMs,uint32_t lastMs){
  if(!daily_.state().enabled)return;
  if(!lossPending_){lossFirstMs_=firstMs;lossLastMs_=lastMs;lossPending_=true;return;}
  if(static_cast<int32_t>(firstMs-lossFirstMs_)<0)lossFirstMs_=firstMs;
  if(static_cast<int32_t>(lastMs-lossLastMs_)>0)lossLastMs_=lastMs;
}
void ControlRuntime::prepareLearningInput(const DailyInputs& in){
  if(!daily_.state().enabled){lossPending_=false;return;}
  const auto first=clock_.timeAt(lossFirstMs_),last=clock_.timeAt(lossLastMs_);
  auto intersects=[&]{const auto day=daily_.today().day;return !first || !last || (day>=(*first+28800000)/86400000 && day<=(*last+28800000)/86400000);};
  if(lossPending_ && intersects())daily_.invalidateCurrent(DayGap::Rejected);
  daily_.advance(in);
  if(lossPending_ && intersects())daily_.invalidateCurrent(DayGap::Rejected);
  if(lossPending_ && static_cast<int32_t>(in.nowMs-lossLastMs_)>=0)lossPending_=false;
}
Submission ControlRuntime::submit(Intent intent,uint32_t nowMs,std::optional<uint32_t> stamp,std::optional<uint32_t> expectedEpoch){
  cancelExpiredManual(inputs(nowMs,nowMs));
  consumeLearning(nowMs);const auto in=inputs(stamp.value_or(nowMs),nowMs);
  if(intent.source==Source::HomeKitAutomation)prepareLearningInput(in);
  const auto token=daily_.capture(intent,in);
  auto error=expectedEpoch && *expectedEpoch!=control_.observed().epoch?Error::Disconnected:
    stamp && nowMs-*stamp>5000?Error::RequestExpired:checkControl(intent,nowMs);
  if(error==Error::None && intent.source!=Source::HomeKitAutomation && !daily_.manualOverride(in,false,nowMs))error=Error::PersistenceFailed;
  if(error==Error::None)control_.cancelQueued(nowMs,Source::LocalReplay);
  const auto result=error==Error::None?control_.submit(intent,nowMs):Submission{std::nullopt,error};
  consumeLearning(nowMs);
  if(!result.id){if(token)daily_.complete(*token,false);}
  else if(intent.source==Source::HomeKitAutomation){
    bool tracked=false;
    for(auto& pending:learning_)if(!pending.id){pending.id=result.id;pending.token=token;pending.admittedMs=in.nowMs;tracked=true;break;}
    if(!tracked){if(token)daily_.complete(*token,false);daily_.invalidateCurrent(DayGap::InputGap);}
  }
  return result;
}
void ControlRuntime::tick(uint32_t nowMs){
  consumeReplay(nowMs);
  const auto in=inputs(nowMs,nowMs);const auto desired=daily_.decision(in);
  cancelExpiredManual(in);
  const bool clockChanged=lastClockRevision_!=clock_.revision();
  if(recover_ || clockChanged || !lastDecision_ || !(desired==*lastDecision_)){
    bool regular=false;uint8_t begin=0,end=0;
    const bool unfinished=replayId_ || replayMask_ || (boundary_ && boundaryIndex_<boundaryEnd_);
    if(!recover_ && !clockChanged && !unfinished && lastDecision_ && lastDecision_->mode==ReplayMode::Replaying &&
       desired.mode==ReplayMode::Replaying && lastReplayWall_ && in.wallMs && *in.wallMs>=*lastReplayWall_ && *in.wallMs-*lastReplayWall_<=5000){
      std::optional<int64_t> boundaryTime;bool multiple=false;
      const auto base=*lastReplayWall_-(*lastReplayWall_+28800000)%86400000;
      const auto& sample=daily_.state().sample;
      for(uint8_t n=0;n<sample.count;++n){
        auto at=base+int64_t(sample.actions[n].second)*1000;if(at<=*lastReplayWall_)at+=86400000;
        if(at>*in.wallMs)continue;
        if(!boundaryTime){boundaryTime=at;begin=n;end=n+1;}
        else if(at==*boundaryTime)end=n+1;
        else multiple=true;
      }
      regular=boundaryTime.has_value() && !multiple;
    }
    control_.cancelQueued(nowMs,Source::LocalReplay);++replayRevision_;autoError_=Error::None;
    boundary_=regular;boundaryIndex_=begin;boundaryEnd_=end;
    replayMask_=desired.mode==ReplayMode::Replaying && !regular?desired.mask:0;
    lastDecision_=desired;recover_=false;
  }
  lastReplayWall_=in.wallMs;lastClockRevision_=clock_.revision();
  control_.tick(nowMs);consumeLearning(nowMs);
  consumeReplay(nowMs);if(replayId_)return;
  if(desired.mode!=ReplayMode::Replaying){autoError_=Error::None;return;}
  if((!boundary_ || boundaryIndex_>=boundaryEnd_) && !replayMask_)return;
  if(control_.inFlight())return;
  if(retryReplay_){if(static_cast<int32_t>(nowMs-replayRetryAtMs_)<0)return;retryReplay_=false;}
  if(!writesEnabled_){autoError_=Error::WritesDisabled;return;}
  const std::array<alpicool::Setting,3> settings{{alpicool::Setting::TargetC,alpicool::Setting::Power,alpicool::Setting::Mode}};
  auto intentFor=[&](const DailyAction& a){return Intent{settings[static_cast<size_t>(a.field)],a.value,Source::LocalReplay};};
  if(boundary_){
    // Validate this whole boundary before changing its first field, preserving
    // the original stable order even when a field appears twice in one second.
    for(uint8_t n=boundaryIndex_;n<boundaryEnd_;++n){const auto error=control_.check(intentFor(daily_.state().sample.actions[n]),nowMs);if(error!=Error::None){autoError_=error;return;}}
    const auto submitted=control_.submit(intentFor(daily_.state().sample.actions[boundaryIndex_]),nowMs);
    replayId_=submitted.id;autoError_=submitted.error;dispatchedRevision_=replayRevision_;return;
  }
  // Recovery validates the complete managed profile, then prioritizes OFF so
  // missed history can never produce a brief ON before the final OFF goal.
  for(size_t field=0;field<3;++field)if(desired.mask&(1u<<field)){
    const auto error=control_.check({settings[field],desired.values[field],Source::LocalReplay},nowMs);
    if(error!=Error::None){autoError_=error;return;}
  }
  const std::array<uint8_t,3> order=(desired.mask&2) && desired.values[1]==0?std::array<uint8_t,3>{{1,0,2}}:std::array<uint8_t,3>{{0,2,1}};
  for(const auto field:order)if(replayMask_&(1u<<field)){
    const auto submitted=control_.submit({settings[field],desired.values[field],Source::LocalReplay},nowMs);
    replayId_=submitted.id;autoError_=submitted.error;replayBit_=uint8_t(1u<<field);dispatchedRevision_=replayRevision_;return;
  }
}
void ControlRuntime::learnThrough(uint32_t stamp,uint32_t nowMs){consumeLearning(nowMs);cancelExpiredManual(inputs(nowMs,nowMs));prepareLearningInput(inputs(stamp,nowMs));}
void ControlRuntime::automationLink(AutomationHealth link,bool ready,uint32_t sampledMs,std::optional<uint32_t> age){
  if(link==AutomationHealth::Healthy && !age)link=AutomationHealth::Unknown;
  if(!haveLink_ || sampledMs!=linkMs_ || link!=link_ || age.value_or(0)!=ackAgeMs_){
    link_=link;linkMs_=linkAdvanceMs_=sampledMs;linkElapsedMs_=0;ackAgeMs_=age.value_or(0);
  }
  networkReady_=ready;haveLink_=true;
}
void ControlRuntime::connection(bool ready,uint32_t epoch,uint32_t nowMs){control_.connection(ready,epoch,nowMs);consumeLearning(nowMs);recover_=true;}
bool ControlRuntime::observe(const alpicool::Frame& frame,uint32_t epoch,uint32_t sequence,uint32_t receivedMs,std::optional<uint32_t> processedMs){
  const auto nowMs=processedMs.value_or(receivedMs);clock_.advance(nowMs);
  const auto before=control_.observed().status;const auto flight=control_.inFlight();
  const bool comparable=control_.observed().epoch==epoch && control_.fresh(receivedMs);
  if(!control_.observe(frame,epoch,sequence,receivedMs))return false;
  if(!comparable)recover_=true;
  const auto& after=control_.observed().status;
  auto changed=[&](bool difference,alpicool::Setting setting){return difference && (!flight || flight->intent.setting!=setting || !control_.matches(flight->intent));};
  const bool external=comparable && (
    changed(before.power && after.power && before.power!=after.power,alpicool::Setting::Power) ||
    changed(before.mode && after.mode && before.mode!=after.mode,alpicool::Setting::Mode) ||
    changed(before.targetC && after.targetC && before.targetC!=after.targetC,alpicool::Setting::TargetC));
  if(external)daily_.manualOverride(inputs(receivedMs,nowMs),true,nowMs);
  consumeLearning(nowMs);consumeReplay(nowMs);return true;
}
bool ControlRuntime::syncTime(int64_t seconds,uint32_t nowMs){return clock_.sync(seconds,nowMs);}
BackupPreview ControlRuntime::previewBackup(bool enabled,uint32_t nowMs){
  const auto in=inputs(nowMs,nowMs);
  preview_=BackupPreview{enabled,daily_.state().generation,clock_.revision(),++nextPreview_,daily_.preview(enabled,in)};return *preview_;
}
BackupError ControlRuntime::applyBackup(const BackupPreview& preview,uint32_t nowMs,std::optional<uint32_t> admittedMs){
  const auto in=inputs(nowMs,nowMs);
  if(!preview_ || preview.token!=preview_->token || preview.enabled!=preview_->enabled || preview.generation!=preview_->generation ||
     preview.clockRevision!=preview_->clockRevision || !(preview.effect==preview_->effect) || preview.generation!=daily_.state().generation ||
     preview.clockRevision!=clock_.revision() || !(preview.effect==daily_.preview(preview.enabled,in)))return BackupError::StalePreview;
  if(!daily_.configure(preview.enabled,admittedMs?inputs(*admittedMs,nowMs):in,nowMs))return BackupError::PersistenceFailed;
  control_.cancelQueued(nowMs,Source::LocalReplay);
  if(preview.enabled)for(const auto source:{Source::Manual,Source::Web,Source::HomeKit})control_.cancelQueued(nowMs,source);
  recover_=true;preview_.reset();return BackupError::None;
}
bool ControlRuntime::syncNetworkTime(NetworkTimeSample sample,uint32_t nowMs){
  if(!sample.sequence || sample.sequence==lastNetworkSequence_)return false;
  lastNetworkSequence_=sample.sequence;const auto age=static_cast<uint32_t>(nowMs-sample.receivedMs);
  return age<=30000 && clock_.sync(int64_t(sample.seconds)+(age+sample.fractionMs)/1000,nowMs,(age+sample.fractionMs)%1000);
}
}
