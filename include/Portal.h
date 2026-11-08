#pragma once
#include "ControlRuntime.h"
#include "BoundedHttp.h"
#include <array>
#include <functional>
#include <optional>
#include <string>
namespace frost {
using Ticket=CommandId;
enum class ActionKind {Command,Preview,Apply,Clock};
struct WebAction {
  int64_t unixSeconds=0;Ticket id{};Intent intent{alpicool::Setting::TargetC,0,Source::Web};Ticket preview{};
  uint32_t baseGeneration=0,epoch=0,admittedMs=0;ActionKind kind=ActionKind::Command;bool enabled=false;
};
enum class WebState {Queued,Running,AwaitingReadback,Confirmed,Completed,Failed,TimedOut,Cancelled};
enum class ApiError {None,BadRequest,ForbiddenHost,ForbiddenOrigin,ContentType,TooLarge,Busy,NotReady,NotFound,WritesDisabled,InvalidTime,StalePreview,Offline,UnknownState,Stale,Locked,OutOfRange,UnsupportedUnit,UnsupportedLayout,Persistence,Transport,Timeout,Disconnected,Superseded,ReadbackRequired,Expired,RecordsChanged,RequestExpired};
ApiError apiError(Error error);
ApiError apiError(BackupError error);
const char* errorCode(ApiError error);
const char* errorMessage(ApiError error);
int errorStatus(ApiError error);
const char* stateName(WebState state);
const char* replayModeName(ReplayMode mode);
const char* dayGapCode(DayGap gap);
struct WebResult {
  Ticket id{};ActionKind kind=ActionKind::Command;WebState state=WebState::Queued;ApiError error=ApiError::None;
  uint32_t createdMs=0,finishedMs=0;Intent intent{alpicool::Setting::TargetC,0,Source::Web};
  bool hasController=false;CommandId controller{};uint32_t previewToken=0,generation=0;ReplayDecision effect;
};
struct BackupView {
  bool enabled=false,sampleValid=false,hasLastDay=false;
  int32_t sampleDay=0,todayDay=0,lastDay=0;uint8_t sampleCount=0,todayCount=0,lastCount=0;
  DayGap todayGap=DayGap::PartialStart,lastGap=DayGap::PartialStart;uint32_t sampleRevision=0,todayRevision=0;
};
struct WebSnapshot {
  uint32_t boot=0,capturedMs=0,observedMs=0,epoch=0,sequence=0,generation=0;
  bool ready=false,connected=false,observed=false,valid=false,writes=false,timeTrusted=false,manual=false;
  bool wifi=false,ble=false,scanning=false;uint32_t advertisements=0,freeHeap=0,minHeap=0,largestBlock=0;
  bool homekit=false,paired=false,secureClient=false;
  uint8_t automationHealth=0,homekitPending=0;uint32_t automationRequests=0,automationConfirmed=0,healthAcks=0;
  std::optional<uint32_t> healthAckAge;std::array<ApiError,6> homekitErrors{};
  alpicool::Status status;int64_t wall=0,overrideUntilMs=0;BackupView backup;ReplayDecision decision;
  Error automaticError=Error::None,storageError=Error::None;uint8_t storeStatus=0;
  std::array<Error,3> controls{{Error::NoState,Error::NoState,Error::NoState}};
  std::array<Command,4> automatic{};uint8_t automaticCount=0;
};
enum class PageState:uint8_t {Ok,Changed,Absent};
struct DailyPage {
  PageState state=PageState::Absent;uint32_t boot=0,revision=0;int32_t day=0;uint8_t offset=0,total=0,count=0;
  DayGap gap=DayGap::PartialStart;bool sample=false;std::array<DailyRecord,8> rows{};
};
struct AutomationEvidence {AutomationHealth health=AutomationHealth::Unknown;uint32_t sampledMs=0;std::optional<uint32_t> ackAgeMs;};
struct Critical {void* context=nullptr;void(*enter)(void*)=nullptr;void(*leave)(void*)=nullptr;};
struct IngressEnvelope {std::optional<WebAction> action;uint32_t frontierMs=0;bool loss=false;uint32_t lossFirstMs=0,lossLastMs=0;};
class PortalMailbox {
 public:
  // The clock runs under the lock: it must be bounded/nonallocating. Production
  // supplies millis; an empty clock uses explicit timestamps in synchronous tests.
  explicit PortalMailbox(Critical critical={},std::function<uint32_t()> now={}):critical_(critical),now_(std::move(now)){}
  void reset(uint32_t boot);
  std::optional<Ticket> submit(WebAction action,uint32_t nowMs);
  std::optional<std::array<Ticket,2>> submitPair(const std::array<WebAction,2>& actions,uint32_t nowMs);
  std::optional<WebAction> take();
  IngressEnvelope receive(uint32_t nowMs);
  void rejectAutomation(uint32_t nowMs);
  void reportHealth(AutomationHealth health,uint32_t observedMs,std::optional<uint32_t> ackAgeMs);
  AutomationEvidence healthEvidence()const;
  void put(const WebResult& result);
  std::optional<WebResult> lookup(Ticket id)const;
  std::array<WebResult,16> results()const;
  // Stream one fixed slot on the worker; avoid putting all 16 results on its stack.
  std::optional<WebResult> resultAt(size_t index)const;
  void publish(const WebSnapshot& snapshot);
  void publish(const WebSnapshot& snapshot,const DailyReplay& daily);
  WebSnapshot snapshot()const;
  DailyPage page(bool sample,uint8_t offset,uint32_t revision)const;
 private:
  Critical critical_;std::function<uint32_t()> now_;
  uint32_t boot_=0,next_=0;std::array<WebAction,4> queue_{};size_t head_=0,count_=0;
  std::array<WebResult,16> results_{};WebSnapshot snapshot_;AutomationEvidence evidence_;
  bool loss_=false;uint32_t lossFirstMs_=0,lossLastMs_=0;
  DailySample sample_;DailyCandidate candidate_;std::array<DailyRecord,64> records_{};
  uint32_t sampleRevision_=0,todayRevision_=0;
  bool room(size_t amount)const;
  Ticket push(const WebAction& action,uint32_t nowMs);
  std::optional<WebAction> pop();
  void markLoss(uint32_t nowMs);
};
class PortalWorker {
 public:
  PortalWorker(ControlRuntime& core,PortalMailbox& mailbox,std::function<uint32_t()> now):core_(core),mail_(mailbox),now_(std::move(now)){}
  void tick();
 private:
  ControlRuntime& core_;PortalMailbox& mail_;std::function<uint32_t()> now_;
  std::optional<BackupPreview> preview_;
  // Keep input locals off the deeper Controller terminal/persistence path.
  void drainInputs();
  void updateResults();
};
struct ApiReply {int status=200;std::string body,location;};
struct ApiBackend {
  std::function<WebSnapshot()> snapshot;
  std::function<std::optional<Ticket>(WebAction,uint32_t)> submit;
  std::function<std::optional<WebResult>(Ticket)> lookup;
  std::function<std::array<WebResult,16>()> results;
  std::function<std::optional<std::array<Ticket,2>>(const std::array<WebAction,2>&,uint32_t)> submitPair={};
  std::function<void(uint32_t)> rejectedAutomation={};
  std::function<void(AutomationHealth,uint32_t,std::optional<uint32_t>)> reportHealth={};
  std::function<DailyPage(bool,uint8_t,uint32_t)> page={};
};
class PortalApi {
 public:
  PortalApi(ApiBackend backend,std::function<std::vector<std::string>()> hosts):backend_(std::move(backend)),hosts_(std::move(hosts)){}
  bool allowedHost(const std::string& host)const;
  ApiReply handle(const HttpRequest& request,uint32_t nowMs)const;
  static ApiReply failure(ApiError error);
 private:
  ApiBackend backend_;std::function<std::vector<std::string>()> hosts_;
};
std::string ticketString(Ticket id);
std::optional<Ticket> parseTicket(const std::string& text);
}
