#include "DailyReplay.h"
#include <algorithm>
namespace frost {
namespace {
constexpr int64_t dayMs=86400000,beijingOffsetMs=28800000;
bool usableTime(const DailyInputs& in){return in.wallMs && *in.wallMs>=1577836800000LL && *in.wallMs<=4102444800000LL;}
bool healthy(const DailyInputs& in){return in.networkReady && in.link==AutomationHealth::Healthy;}
bool recordingReady(const DailyInputs& in){return healthy(in) && in.deviceReady;}
}
bool DailyReplay::validState(const DailyPersistedState& state){
  if(state.overrideUntilMs && (!state.manual || *state.overrideUntilMs<1577836800000LL || *state.overrideUntilMs>4102531200000LL))return false;
  const auto& sample=state.sample;
  if(!sample.valid)return sample.count==0 && sample.day==0;
  if(sample.count>64 || sample.day<18262 || sample.day>47482)return false;
  uint64_t orders=0;
  for(size_t i=0;i<sample.count;++i){
    const auto& a=sample.actions[i];
    if(a.second>=86400 || a.order>=64 || (orders&(uint64_t(1)<<a.order)))return false;
    orders|=uint64_t(1)<<a.order;
    if(a.field==DailyField::Temperature){if(a.value<-20 || a.value>20)return false;}
    else if(a.field==DailyField::Power || a.field==DailyField::Mode){if(a.value!=0 && a.value!=1)return false;}
    else return false;
    if(i){const auto& p=sample.actions[i-1];if(a.second<p.second || (a.second==p.second && a.order<=p.order))return false;}
  }
  return true;
}
bool DailyReplay::restore(const DailyPersistedState& state){
  if(!validState(state))return false;
  state_=state;resetAfterRestore();return true;
}
void DailyReplay::resetAfterRestore(){today_={};lastClosed_.reset();haveTime_=previousReady_=false;storageFault_=retryPending_=dirty_=false;manualAtMs_.reset();++candidateSerial_;}
bool DailyReplay::commit(const DailyPersistedState& next,uint32_t nowMs){
  if(!save_ || !save_(next)){storageFault_=retryPending_=true;retryAtMs_=nowMs+5000;return false;}
  state_=next;storageFault_=retryPending_=dirty_=false;return true;
}
bool DailyReplay::configure(bool enabled,const DailyInputs& inputs,std::optional<uint32_t> processedMs){
  const bool changed=enabled!=state_.enabled;
  auto next=state_;next.enabled=enabled;next.manual=false;next.overrideUntilMs.reset();++next.generation;
  if(!commit(next,processedMs.value_or(inputs.nowMs)))return false;
  if(changed){haveTime_=false;advance(inputs);}return true;
}
void DailyReplay::advance(const DailyInputs& in){
  if(!state_.enabled)return;
  if(!usableTime(in)){today_.gap=DayGap::TimeUnknown;haveTime_=false;return;}
  if((dirty_ || (state_.manual && !state_.overrideUntilMs)) && (!retryPending_ || static_cast<int32_t>(in.nowMs-retryAtMs_)>=0)){
    const auto nextAction=decision(in).nextActionMs;
    if(dirty_ || nextAction){auto next=state_;if(next.manual && !next.overrideUntilMs)next.overrideUntilMs=nextAction;++next.generation;commit(next,in.nowMs);}
  }
  const auto wall=*in.wallMs;const auto local=wall+beijingOffsetMs;
  const auto day=static_cast<int32_t>(local/dayMs);const bool boundary=local%dayMs==0;
  if(!haveTime_){today_={day,DayGap::PartialStart};++candidateSerial_;}
  else {
    const auto elapsed=static_cast<uint32_t>(in.nowMs-lastNowMs_);
    const auto offset=wall-lastWallMs_-int64_t(elapsed);
    const bool stable=offset>=-2000 && offset<=2000;
    if(!stable)today_.gap=DayGap::ClockChanged;
    else if(elapsed>5000)today_.gap=DayGap::InputGap;
    // A correction around midnight must not rotate backwards and
    // discard actions, then certify coverage on the second forward crossing.
    if(day<today_.day){today_.gap=DayGap::ClockChanged;previousReady_=recordingReady(in);lastNowMs_=in.nowMs;lastWallMs_=wall;return;}
    if(day!=today_.day){
      const bool nextDay=day==today_.day+1;
      if(today_.gap==DayGap::None && !recordingReady(in))today_.gap=healthy(in)?DayGap::DeviceUnavailable:DayGap::LinkUnavailable;
      for(size_t i=0;i<today_.count;++i)if(records_[i].outcome==DailyOutcome::Pending)today_.gap=DayGap::PendingAtBoundary;
      if(nextDay && today_.gap==DayGap::None && recordingReady(in)){
        auto next=state_;next.sample.valid=true;next.sample.day=today_.day;next.sample.count=today_.count;
        for(size_t i=0;i<today_.count;++i)next.sample.actions[i]=records_[i].action;
        std::sort(next.sample.actions.begin(),next.sample.actions.begin()+next.sample.count,[](const DailyAction& a,const DailyAction& b){return a.second<b.second || (a.second==b.second && a.order<b.order);});
        ++next.generation;if(!commit(next,in.nowMs))today_.gap=DayGap::Persistence;
      }
      lastClosed_=today_;
      const bool startsCovered=stable && recordingReady(in) && ((nextDay && elapsed<=5000 && previousReady_) || boundary);
      today_={day,startsCovered?DayGap::None:DayGap::PartialStart};++candidateSerial_;
    }
  }
  if(!healthy(in))today_.gap=DayGap::LinkUnavailable;
  else if(!in.deviceReady)today_.gap=DayGap::DeviceUnavailable;
  haveTime_=true;previousReady_=recordingReady(in);lastNowMs_=in.nowMs;lastWallMs_=wall;
}
std::optional<LearningToken> DailyReplay::capture(Intent intent,const DailyInputs& in){
  if(!state_.enabled || intent.source!=Source::HomeKitAutomation)return std::nullopt;
  advance(in);if(!usableTime(in) || (*in.wallMs+beijingOffsetMs)/dayMs!=today_.day)return std::nullopt;
  DailyField field;
  switch(intent.setting){
    case alpicool::Setting::TargetC:field=DailyField::Temperature;break;
    case alpicool::Setting::Power:field=DailyField::Power;break;
    case alpicool::Setting::Mode:field=DailyField::Mode;break;
    default:today_.gap=DayGap::Rejected;return std::nullopt;
  }
  if((field==DailyField::Temperature && (intent.value<-20 || intent.value>20)) ||
     (field!=DailyField::Temperature && intent.value!=0 && intent.value!=1)){today_.gap=DayGap::Rejected;return std::nullopt;}
  if(today_.count==records_.size()){today_.gap=DayGap::Overflow;return std::nullopt;}
  const auto index=today_.count++;
  const auto second=static_cast<uint32_t>((*in.wallMs+beijingOffsetMs)%dayMs/1000);
  records_[index]={{second,field,static_cast<int8_t>(intent.value),index},DailyOutcome::Pending};
  return LearningToken{candidateSerial_,index,in.nowMs};
}
bool DailyReplay::complete(LearningToken token,bool confirmed,std::optional<LearningCompletion> completion){
  if(token.candidate!=candidateSerial_ || token.index>=today_.count || records_[token.index].outcome!=DailyOutcome::Pending)return false;
  if(confirmed && completion){
    if(!completion->confirmedWallMs)today_.gap=DayGap::ClockChanged;
    else{const auto day=(*completion->confirmedWallMs+beijingOffsetMs)/dayMs;if(day>today_.day)today_.gap=DayGap::PendingAtBoundary;else if(day<today_.day)today_.gap=DayGap::ClockChanged;}
  }
  records_[token.index].outcome=confirmed?DailyOutcome::Confirmed:DailyOutcome::Failed;
  if(!confirmed)today_.gap=DayGap::Failed;
  else if(state_.manual && (!manualAtMs_ || static_cast<int32_t>(token.admittedMs-*manualAtMs_)>0 ||
          (token.admittedMs==*manualAtMs_ && (token.candidate!=manualCandidate_ || token.index>=manualAfter_)))){
    endOverride(completion?completion->processedMs:lastNowMs_);
  }
  return true;
}
bool DailyReplay::endOverride(uint32_t nowMs){
  auto next=state_;next.manual=false;next.overrideUntilMs.reset();++next.generation;
  if(commit(next,nowMs))return true;
  state_.manual=false;state_.overrideUntilMs.reset();dirty_=true;return false;
}
bool DailyReplay::confirmAutomation(uint32_t admittedMs,uint32_t processedMs){
  if(!state_.enabled || !state_.manual)return true;
  if(manualAtMs_ && static_cast<int32_t>(admittedMs-*manualAtMs_)<=0)return true;
  return endOverride(processedMs);
}
std::optional<DailyRecord> DailyReplay::record(uint8_t index,bool sample)const{
  if(sample){if(!state_.sample.valid || index>=state_.sample.count)return std::nullopt;return DailyRecord{state_.sample.actions[index],DailyOutcome::Confirmed};}
  if(index>=today_.count)return std::nullopt;
  return records_[index];
}
ReplayDecision DailyReplay::evaluate(bool enabled,bool manual,const DailyInputs& in)const{
  ReplayDecision result;if(!enabled)return result;
  if(!usableTime(in)){result.mode=ReplayMode::WaitingForTime;return result;}
  const auto wall=*in.wallMs,local=wall+beijingOffsetMs;const auto day=local/dayMs;
  const bool sample=state_.sample.valid && state_.sample.day<day;
  if(sample)for(size_t n=0;n<state_.sample.count;++n){
    auto at=wall-local%dayMs+int64_t(state_.sample.actions[n].second)*1000;if(at<=wall)at+=dayMs;
    if(!result.nextActionMs || at<*result.nextActionMs)result.nextActionMs=at;
  }
  if(healthy(in)){result.mode=ReplayMode::HomeKitPrimary;return result;}
  if(manual && (!state_.overrideUntilMs || wall<*state_.overrideUntilMs)){result.mode=ReplayMode::ManualOverride;return result;}
  if(!sample){result.mode=ReplayMode::Learning;return result;}
  if(in.link==AutomationHealth::Unknown && in.nowMs-bootMs_<180000){result.mode=ReplayMode::CheckingLink;return result;}
  result.mode=ReplayMode::Replaying;
  std::array<std::optional<int8_t>,3> before,last;
  const auto second=static_cast<uint32_t>(local%dayMs/1000);
  for(size_t n=0;n<state_.sample.count;++n){const auto& a=state_.sample.actions[n];const auto field=static_cast<size_t>(a.field);last[field]=a.value;if(a.second<=second)before[field]=a.value;}
  for(size_t field=0;field<3;++field)if(last[field]){result.mask|=uint8_t(1u<<field);result.values[field]=before[field].value_or(*last[field]);}
  return result;
}
ReplayDecision DailyReplay::decision(const DailyInputs& in)const{return evaluate(state_.enabled,state_.manual,in);}
ReplayDecision DailyReplay::preview(bool enabled,const DailyInputs& in)const{return evaluate(enabled,false,in);}

bool DailyReplay::manualOverride(const DailyInputs& in,bool alreadyApplied,std::optional<uint32_t> processedMs){
  if(!state_.enabled)return true;
  auto next=state_;next.manual=true;next.overrideUntilMs=usableTime(in)?decision(in).nextActionMs:std::nullopt;++next.generation;
  const bool saved=commit(next,processedMs.value_or(in.nowMs));
  if(saved || alreadyApplied){manualCandidate_=candidateSerial_;manualAfter_=today_.count;manualAtMs_=in.nowMs;}
  if(saved)return true;
  if(alreadyApplied){state_.manual=true;state_.overrideUntilMs=next.overrideUntilMs;dirty_=true;}
  return false;
}

}
