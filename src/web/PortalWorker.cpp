#include "Portal.h"
namespace frost {
namespace {bool active(WebState state){return state==WebState::Queued || state==WebState::Running || state==WebState::AwaitingReadback;}}
void PortalWorker::drainInputs(){
 for(unsigned i=0;i<4;++i){
  const auto input=mail_.receive(now_());if(input.loss)core_.inputLoss(input.lossFirstMs,input.lossLastMs);
  if(input.action){
   const auto& action=*input.action;auto previous=mail_.lookup(action.id);
   if(previous){
    auto& result=*previous;const auto now=now_();result.error=ApiError::None;
    switch(action.kind){
     case ActionKind::Command:{
      const auto submitted=core_.submit(action.intent,now,action.admittedMs,action.epoch);result.error=apiError(submitted.error);
      if(submitted.id){result.controller=*submitted.id;result.hasController=true;result.state=WebState::Queued;}
      else if(result.error==ApiError::None)result.error=ApiError::Expired;
      break;
     }
     case ActionKind::Preview:
      if(action.baseGeneration!=core_.daily().state().generation)result.error=ApiError::StalePreview;
      else{preview_=core_.previewBackup(action.enabled,now);result.previewToken=preview_->token;result.effect=preview_->effect;result.generation=preview_->generation;}
      break;
     case ActionKind::Apply:
      if(!preview_ || action.preview.boot!=action.id.boot || action.preview.sequence!=preview_->token)result.error=ApiError::StalePreview;
      else{result.error=apiError(core_.applyBackup(*preview_,now,action.admittedMs));if(result.error==ApiError::None){result.generation=core_.daily().state().generation;preview_.reset();}}
      break;
     case ActionKind::Clock:
      if(!core_.syncTime(action.unixSeconds+static_cast<uint32_t>(now-action.admittedMs)/1000,now))result.error=ApiError::InvalidTime;
      break;
    }
    if(result.error!=ApiError::None){result.state=result.error==ApiError::Disconnected?WebState::Cancelled:WebState::Failed;result.finishedMs=now_();}
    else if(!result.hasController){result.state=WebState::Completed;result.finishedMs=now_();}
    mail_.put(result);
   }else if(action.kind==ActionKind::Command && action.intent.source==Source::HomeKitAutomation)core_.inputLoss(action.admittedMs,action.admittedMs);
  }
  // The locked frontier is the next FIFO admission or locked-empty time, never
  // an unbounded owner wall time that could seal before older actions drain.
  core_.learnThrough(input.frontierMs,now_());if(!input.action)break;
 }
}
void PortalWorker::updateResults(){
 for(size_t index=0;index<16;++index){
  const auto slot=mail_.resultAt(index);if(!slot || !slot->hasController || !active(slot->state))continue;
  auto result=*slot;
  const auto command=core_.command(result.controller);
  if(!command){result.state=WebState::Failed;result.error=ApiError::Expired;result.finishedMs=now_();}
  else{
   switch(command->state){
    case CommandState::Queued:result.state=WebState::Queued;break;case CommandState::AwaitingReadback:result.state=WebState::AwaitingReadback;break;
    case CommandState::Confirmed:result.state=WebState::Confirmed;break;case CommandState::Failed:result.state=WebState::Failed;break;
    case CommandState::TimedOut:result.state=WebState::TimedOut;break;case CommandState::Cancelled:result.state=WebState::Cancelled;break;
   }
   result.error=apiError(command->error);result.finishedMs=command->finishedMs;
  }
  mail_.put(result);
 }
}
void PortalWorker::tick(){drainInputs();core_.tick(now_());updateResults();}
}
