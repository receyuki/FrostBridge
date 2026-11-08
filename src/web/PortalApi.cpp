#if !defined(ARDUINO) || defined(FROSTBRIDGE_GATEWAY)
#include "Portal.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
namespace frost {
namespace {
class Budget : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Block { size_t total; };
  size_t used_=0;
 public:
  void* allocate(size_t size) override {
    if(size>8192-sizeof(Block) || used_+size+sizeof(Block)>8192) return nullptr;
    auto* block=static_cast<Block*>(std::malloc(size+sizeof(Block)));if(!block)return nullptr;
    block->total=size+sizeof(Block);used_+=block->total;return block+1;
  }
  void deallocate(void* pointer) override { if(pointer){auto* block=static_cast<Block*>(pointer)-1;used_-=block->total;std::free(block);} }
  void* reallocate(void* pointer,size_t size) override {
    if(!pointer)return allocate(size);
    if(!size){deallocate(pointer);return nullptr;}
    auto* old=static_cast<Block*>(pointer)-1;const auto total=old->total;
    if(size>8192-sizeof(Block) || used_-total+size+sizeof(Block)>8192)return nullptr;
    auto* block=static_cast<Block*>(std::realloc(old,size+sizeof(Block)));if(!block)return nullptr;
    block->total=size+sizeof(Block);used_=used_-total+block->total;return block+1;
  }
};
struct Writer {
  std::string value;bool overflow=false;
  size_t write(uint8_t byte) { if(value.size()>=4096){overflow=true;return 0;}value.push_back(char(byte));return 1; }
  size_t write(const uint8_t* bytes,size_t count) { const auto n=std::min(count,4096-value.size());value.append(reinterpret_cast<const char*>(bytes),n);overflow|=n!=count;return n; }
};
ApiReply jsonReply(int status,JsonDocument& doc,std::string location={}) {
  Writer out;serializeJson(doc,out);
  if(doc.overflowed() || out.overflow)return {503,"{\"error\":{\"code\":\"gateway_busy\",\"message\":\"Gateway is busy. Please retry.\"}}",{}};
  return {status,std::move(out.value),std::move(location)};
}
std::string authority(std::string host) {
  for(auto& c:host){if(static_cast<unsigned char>(c)<=32 || static_cast<unsigned char>(c)>=127)return {};c=std::tolower(static_cast<unsigned char>(c));}
  if(host.size()>3 && host.compare(host.size()-3,3,":80")==0)host.resize(host.size()-3);
  if(host.find_first_of("/@:#?\\")!=std::string::npos)return {};
  return host;
}
template<class T> void nullable(JsonObject object,const char* key,const std::optional<T>& value,bool known=true) {
  if(known && value)object[key]=*value;else object[key]=nullptr;
}
bool fresh(const WebSnapshot& s,uint32_t now){return s.observed && s.valid && s.connected && now-s.observedMs<=30000;}
bool onlyKeys(JsonObjectConst object,std::initializer_list<const char*> keys) {
  for(const auto pair:object) {bool known=false;for(const auto key:keys)if(pair.key().size()==std::strlen(key) && std::memcmp(pair.key().c_str(),key,pair.key().size())==0)known=true;if(!known)return false;}
  return true;
}
std::optional<std::string> textValue(JsonVariantConst value) {
  if(!value.is<const char*>())return std::nullopt;
  const auto text=value.as<JsonString>();return std::string(text.c_str(),text.size());
}
size_t members(JsonVariantConst value) {
  size_t total=0;
  if(value.is<JsonObjectConst>())for(const auto pair:value.as<JsonObjectConst>())total+=1+members(pair.value());
  if(value.is<JsonArrayConst>())for(const auto child:value.as<JsonArrayConst>())total+=members(child);
  return total;
}
bool exactObject(const std::string& text,JsonDocument& doc) {
  if(!doc.is<JsonObject>())return false;
  bool quoted=false,escape=false,ended=false;int depth=0;size_t keys=0;
  for(const auto c:text) {
    if(ended){if(c!=' ' && c!='\t' && c!='\r' && c!='\n')return false;continue;}
    if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
    if(c=='"')quoted=true;else if(c==':')++keys;else if(c=='{' || c=='[')++depth;else if(c=='}' || c==']'){if(--depth==0)ended=true;}
  }
  return ended && keys==members(doc.as<JsonVariantConst>());
}
const char* actionName(ActionKind kind){switch(kind){case ActionKind::Command:return "command";case ActionKind::Preview:return "preview";case ActionKind::Apply:return "backup_save";case ActionKind::Clock:return "clock";}return "unknown";}
const char* sourceName(Source source){switch(source){case Source::Manual:return "usb";case Source::Web:return "web";case Source::HomeKit:return "homekit_manual";case Source::HomeKitAutomation:return "homekit_automation";case Source::Schedule:return "legacy_schedule";case Source::LocalReplay:return "local_replay";}return "unknown";}
void effectJson(JsonObject object,const ReplayDecision& effect){
 object["state"]=replayModeName(effect.mode);
 if(effect.mask&1)object["target_c"]=effect.values[0];else object["target_c"]=nullptr;
 if(effect.mask&2)object["power"]=effect.values[1]!=0;else object["power"]=nullptr;
 if(effect.mask&4)object["mode"]=effect.values[2]==1?"eco":"max";else object["mode"]=nullptr;
 nullable(object,"next_action_ms",effect.nextActionMs);
}
void resultJson(JsonObject object,const WebResult& result){
 object["id"]=ticketString(result.id);object["type"]=actionName(result.kind);object["state"]=stateName(result.state);
 if(result.kind==ActionKind::Command)object["source"]=sourceName(result.intent.source);
 object["error_code"]=errorCode(result.error);object["message"]=errorMessage(result.error);object["created_ms"]=result.createdMs;object["finished_ms"]=result.finishedMs;
 if(result.kind==ActionKind::Preview && result.previewToken){object["preview_id"]=ticketString({result.id.boot,result.previewToken});effectJson(object["effect"].to<JsonObject>(),result.effect);}
 if(result.kind==ActionKind::Preview || result.kind==ActionKind::Apply)object["generation"]=result.generation;
}
std::string bootText(uint32_t boot){return ticketString({boot,1}).substr(0,8);}
std::string recordsPath(uint32_t boot,bool sample,uint32_t revision,uint8_t offset){return "/api/v1/backup/records/"+bootText(boot)+(sample?"/sample/":"/today/")+std::to_string(revision)+"/"+std::to_string(offset);}
std::optional<uint32_t> decimal(const std::string& text){
 if(text.empty() || text.size()>10)return std::nullopt;
 uint64_t result=0;for(char c:text){if(c<'0' || c>'9')return std::nullopt;result=result*10+c-'0';if(result>UINT32_MAX)return std::nullopt;}return static_cast<uint32_t>(result);
}
void backupJson(JsonObject object,const WebSnapshot& s){
 const auto& b=s.backup;object["boot"]=bootText(s.boot);object["enabled"]=b.enabled;object["generation"]=s.generation;object["time_trusted"]=s.timeTrusted;
 if(s.timeTrusted)object["unix_seconds"]=s.wall;else object["unix_seconds"]=nullptr;
 object["manual_override"]=s.manual;if(s.overrideUntilMs)object["override_until_ms"]=s.overrideUntilMs;else object["override_until_ms"]=nullptr;
 effectJson(object["current"].to<JsonObject>(),s.decision);
 if(b.sampleValid){auto sample=object["sample"].to<JsonObject>();sample["day"]=b.sampleDay;sample["count"]=b.sampleCount;sample["revision"]=b.sampleRevision;sample["records_url"]=recordsPath(s.boot,true,b.sampleRevision,0);if(s.timeTrusted)sample["age_days"]=(s.wall+28800)/86400-b.sampleDay;else sample["age_days"]=nullptr;}
 else object["sample"]=nullptr;
 auto today=object["today"].to<JsonObject>();if(b.todayDay)today["day"]=b.todayDay;else today["day"]=nullptr;today["count"]=b.todayCount;today["gap"]=dayGapCode(b.todayGap);today["revision"]=b.todayRevision;
 if(b.todayDay)today["records_url"]=recordsPath(s.boot,false,b.todayRevision,0);else today["records_url"]=nullptr;
 if(b.hasLastDay){auto last=object["last_closed"].to<JsonObject>();last["day"]=b.lastDay;last["count"]=b.lastCount;last["gap"]=dayGapCode(b.lastGap);}else object["last_closed"]=nullptr;
 object["error_code"]=errorCode(apiError(s.automaticError));object["storage_error"]=errorCode(apiError(s.storageError));
 constexpr const char* stores[]{"empty","loaded","invalid","unavailable"};object["store_status"]=stores[s.storeStatus<4?s.storeStatus:2];
}
}
bool PortalApi::allowedHost(const std::string& host)const{const auto canonical=authority(host);if(canonical.empty())return false;for(const auto& allowed:hosts_())if(canonical==authority(allowed))return true;return false;}
ApiReply PortalApi::failure(ApiError error){Budget budget;JsonDocument doc(&budget);doc["error"]["code"]=errorCode(error);doc["error"]["message"]=errorMessage(error);return jsonReply(errorStatus(error),doc);}
ApiReply PortalApi::handle(const HttpRequest& request,uint32_t nowMs)const{
 if(!allowedHost(request.host))return failure(ApiError::ForbiddenHost);
 const auto snapshot=backend_.snapshot();Budget budget;JsonDocument doc(&budget);
 auto fail=[&](ApiError error){doc.clear();doc["error"]["code"]=errorCode(error);doc["error"]["message"]=errorMessage(error);return jsonReply(errorStatus(error),doc);};
 if(request.method=="GET"){
  if(request.path=="/api/v1/health" || request.path=="/health"){
   doc["firmware"]="m4-daily-backup";doc["gateway_ready"]=snapshot.ready;doc["writes_enabled"]=snapshot.writes;doc["uptime_ms"]=nowMs;
   doc["wifi_connected"]=snapshot.wifi;doc["ble_initialized"]=snapshot.ble;doc["scan_running"]=snapshot.scanning;doc["advertisements"]=snapshot.advertisements;
   doc["free_heap"]=snapshot.freeHeap;doc["min_heap"]=snapshot.minHeap;doc["largest_block"]=snapshot.largestBlock;doc["fridge_connected"]=snapshot.connected;doc["state_stale"]=!fresh(snapshot,nowMs);
   if(snapshot.observed)doc["state_age_ms"]=nowMs-snapshot.observedMs;else doc["state_age_ms"]=nullptr;
   doc["backup_enabled"]=snapshot.backup.enabled;doc["sample_count"]=snapshot.backup.sampleCount;doc["time_trusted"]=snapshot.timeTrusted;doc["manual_override"]=snapshot.manual;return jsonReply(200,doc);
  }
  if(!snapshot.ready)return fail(ApiError::NotReady);
  if(request.path=="/api/v1/backup"){backupJson(doc.to<JsonObject>(),snapshot);return jsonReply(200,doc);}
  if(request.path=="/api/v1/status"){
   doc["boot"]=bootText(snapshot.boot);auto gateway=doc["gateway"].to<JsonObject>();gateway["ready"]=snapshot.ready;gateway["writes_enabled"]=snapshot.writes;gateway["uptime_ms"]=nowMs;
   auto device=doc["device"].to<JsonObject>();device["connected"]=snapshot.connected;device["stale"]=!fresh(snapshot,nowMs);
   if(snapshot.observed){device["age_ms"]=nowMs-snapshot.observedMs;device["epoch"]=snapshot.epoch;device["sequence"]=snapshot.sequence;}else{device["age_ms"]=nullptr;device["epoch"]=nullptr;device["sequence"]=nullptr;}
   nullable(device,"current_c",snapshot.status.currentC,snapshot.observed);nullable(device,"target_c",snapshot.status.targetC,snapshot.observed);nullable(device,"power",snapshot.status.power,snapshot.observed);nullable(device,"locked",snapshot.status.locked,snapshot.observed);
   nullable(device,"min_c",snapshot.status.minC,snapshot.observed);nullable(device,"max_c",snapshot.status.maxC,snapshot.observed);nullable(device,"voltage_v",snapshot.status.voltage,snapshot.observed);
   if(snapshot.observed && snapshot.status.mode)device["mode"]=*snapshot.status.mode==alpicool::Mode::Eco?"eco":"max";else device["mode"]=nullptr;
   if(snapshot.observed && snapshot.status.unit<=1)device["unit"]=snapshot.status.unit?"fahrenheit":"celsius";else device["unit"]=nullptr;
   auto controls=doc["controls"].to<JsonObject>();controls["temperature"]=errorCode(apiError(snapshot.controls[0]));controls["power"]=errorCode(apiError(snapshot.controls[1]));controls["mode"]=errorCode(apiError(snapshot.controls[2]));backupJson(doc["backup"].to<JsonObject>(),snapshot);
   auto hk=doc["homekit"].to<JsonObject>();hk["enabled"]=snapshot.homekit;hk["paired"]=snapshot.paired;hk["secure_client"]=snapshot.secureClient;hk["pending"]=snapshot.homekitPending;
   constexpr const char* health[]{"unconfigured","healthy","expired"};hk["automation_health"]=health[snapshot.automationHealth<3?snapshot.automationHealth:0];nullable(hk,"ack_age_ms",snapshot.healthAckAge);hk["acknowledgements"]=snapshot.healthAcks;
   hk["automation_requests"]=snapshot.automationRequests;hk["automation_confirmed"]=snapshot.automationConfirmed;hk["daily_replay_available"]=true;auto errors=hk["errors"].to<JsonArray>();for(const auto error:snapshot.homekitErrors)errors.add(errorCode(error));return jsonReply(200,doc);
  }
  if(request.path.rfind("/api/v1/backup/records/",0)==0){
   const auto rest=request.path.substr(23);std::array<std::string,4> parts;size_t begin=0;
   for(size_t i=0;i<4;++i){const auto end=rest.find('/',begin);if((i<3 && end==std::string::npos) || (i==3 && end!=std::string::npos))return fail(ApiError::BadRequest);
  parts[i]=rest.substr(begin,end==std::string::npos?end:end-begin);begin=end+1;}
   const auto id=parseTicket(parts[0]+"-00000001");const auto revision=decimal(parts[2]),offset=decimal(parts[3]);const bool sample=parts[1]=="sample";
   if(!id || !revision || !offset || *offset>=64 || *offset%8 || (!sample && parts[1]!="today"))return fail(ApiError::BadRequest);
   if(id->boot!=snapshot.boot)return fail(ApiError::RecordsChanged);
  if(!backend_.page)return fail(ApiError::NotFound);
   const auto page=backend_.page(sample,static_cast<uint8_t>(*offset),*revision);
   if(page.boot!=snapshot.boot || page.state==PageState::Changed)return fail(ApiError::RecordsChanged);
  if(page.state==PageState::Absent)return fail(ApiError::NotFound);
   doc["boot"]=bootText(page.boot);doc["revision"]=page.revision;doc["day"]=page.day;doc["gap"]=dayGapCode(page.gap);doc["offset"]=page.offset;doc["total"]=page.total;
   auto rows=doc["records"].to<JsonArray>();for(unsigned i=0;i<page.count;++i){const auto& record=page.rows[i];auto row=rows.add<JsonObject>();row["second"]=record.action.second;row["order"]=record.action.order;
    constexpr const char* fields[]{"temperature","power","mode"};row["setting"]=fields[static_cast<size_t>(record.action.field)];
    if(record.action.field==DailyField::Power)row["value"]=record.action.value!=0;else if(record.action.field==DailyField::Mode)row["value"]=record.action.value==1?"eco":"max";else row["value"]=record.action.value;
    constexpr const char* outcomes[]{"pending","confirmed","failed"};row["outcome"]=outcomes[static_cast<size_t>(record.outcome)];
   }
   if(page.offset+page.count<page.total)doc["next"]=recordsPath(page.boot,sample,page.revision,page.offset+8);else doc["next"]=nullptr;
   if(page.offset)doc["previous"]=recordsPath(page.boot,sample,page.revision,page.offset-8);else doc["previous"]=nullptr;
   return jsonReply(200,doc);
  }
  for(const auto prefix:{"/api/v1/commands/","/api/v1/operations/"})if(request.path.rfind(prefix,0)==0){
   const auto id=parseTicket(request.path.substr(std::strlen(prefix)));if(!id)return fail(ApiError::NotFound);
  const auto result=backend_.lookup(*id);
   if(!result || ((result->kind==ActionKind::Command)!=(std::string(prefix)=="/api/v1/commands/")))return fail(ApiError::NotFound);
  resultJson(doc.to<JsonObject>(),*result);return jsonReply(200,doc);
  }
  if(request.path=="/api/v1/activity"){
   auto records=backend_.results();std::sort(records.begin(),records.end(),[](const WebResult& a,const WebResult& b){return a.id.sequence>b.id.sequence;});auto rows=doc["operations"].to<JsonArray>();unsigned count=0;
   for(const auto& r:records)if(r.id.sequence && count++<8){auto row=rows.add<JsonObject>();row["id"]=ticketString(r.id);row["type"]=actionName(r.kind);row["state"]=stateName(r.state);row["error_code"]=errorCode(r.error);row["created_ms"]=r.createdMs;if(r.kind==ActionKind::Command)row["source"]=sourceName(r.intent.source);}
   auto automatic=doc["automatic"].to<JsonArray>();for(size_t i=0;i<snapshot.automaticCount && i<4;++i){const auto& r=snapshot.automatic[i];auto row=automatic.add<JsonObject>();row["id"]=ticketString(r.id);row["type"]="local_replay";constexpr const char* states[]{"queued","awaiting_readback","confirmed","failed","timed_out","cancelled"};row["state"]=states[static_cast<size_t>(r.state)];row["error_code"]=errorCode(apiError(r.error));row["recorded_ms"]=r.finishedMs?r.finishedMs:r.sentMs;}
   return jsonReply(200,doc);
  }
  return fail(ApiError::NotFound);
 }
 if(request.method!="POST")return fail(ApiError::BadRequest);
 if(request.origin.rfind("http://",0)!=0 || authority(request.origin.substr(7))!=authority(request.host))return fail(ApiError::ForbiddenOrigin);
 std::string type=request.contentType;for(auto& c:type)c=std::tolower(static_cast<unsigned char>(c));if(type!="application/json" && type!="application/json; charset=utf-8" && type!="application/json;charset=utf-8")return fail(ApiError::ContentType);
 if(request.body.size()>HttpParser::bodyLimit)return fail(ApiError::TooLarge);
 const auto decoded=deserializeJson(doc,request.body.c_str(),request.body.size(),DeserializationOption::NestingLimit(5));if(decoded)return fail(decoded==DeserializationError::NoMemory || decoded==DeserializationError::TooDeep?ApiError::TooLarge:ApiError::BadRequest);
 if(!exactObject(request.body,doc))return fail(ApiError::BadRequest);
  if(!snapshot.ready)return fail(ApiError::NotReady);
  WebAction action;
 if(request.path=="/api/v1/commands"){
  const auto object=doc.as<JsonObjectConst>();if(!onlyKeys(object,{"kind","value"}) || object.size()!=2)return fail(ApiError::BadRequest);
  const auto kind=textValue(object["kind"]);if(!kind)return fail(ApiError::BadRequest);
  if(*kind=="set_temperature"){if(!object["value"].is<int>())return fail(ApiError::BadRequest);
  action.intent.setting=alpicool::Setting::TargetC;action.intent.value=object["value"].as<int>();if(action.intent.value<-20 || action.intent.value>20)return fail(ApiError::OutOfRange);
  }
  else if(*kind=="set_power"){if(!object["value"].is<bool>())return fail(ApiError::BadRequest);
  action.intent.setting=alpicool::Setting::Power;action.intent.value=object["value"].as<bool>()?1:0;}
  else if(*kind=="set_mode"){const auto mode=textValue(object["value"]);if(!mode || (*mode!="eco" && *mode!="max"))return fail(ApiError::BadRequest);
  action.intent.setting=alpicool::Setting::Mode;action.intent.value=*mode=="eco"?1:0;}
  else return fail(ApiError::BadRequest);
  if(!snapshot.writes)return fail(ApiError::WritesDisabled);
  if(!snapshot.connected)return fail(ApiError::Offline);
  if(!snapshot.observed || !snapshot.valid)return fail(ApiError::UnknownState);
  if(!fresh(snapshot,nowMs))return fail(ApiError::Stale);
  action.epoch=snapshot.epoch;
 }else if(request.path=="/api/v1/backup/preview"){
  const auto object=doc.as<JsonObjectConst>();if(object.size()!=2 || !onlyKeys(object,{"base_generation","enabled"}) || !object["base_generation"].is<uint32_t>() || !object["enabled"].is<bool>())return fail(ApiError::BadRequest);
  action.kind=ActionKind::Preview;action.baseGeneration=object["base_generation"].as<uint32_t>();action.enabled=object["enabled"].as<bool>();if(action.baseGeneration!=snapshot.generation)return fail(ApiError::StalePreview);
 }else if(request.path=="/api/v1/backup/apply"){
  const auto object=doc.as<JsonObjectConst>();if(object.size()!=1 || !onlyKeys(object,{"preview_id"}))return fail(ApiError::BadRequest);
  const auto text=textValue(object["preview_id"]);const auto id=text?parseTicket(*text):std::nullopt;
  if(!id || id->boot!=snapshot.boot)return fail(ApiError::StalePreview);
  action.kind=ActionKind::Apply;action.preview=*id;
 }else if(request.path=="/api/v1/clock"){
  const auto object=doc.as<JsonObjectConst>();if(object.size()!=1 || !onlyKeys(object,{"unix_seconds"}) || !object["unix_seconds"].is<int64_t>())return fail(ApiError::BadRequest);
  action.kind=ActionKind::Clock;action.unixSeconds=object["unix_seconds"].as<int64_t>();if(action.unixSeconds<1577836800 || action.unixSeconds>4102444800LL)return fail(ApiError::InvalidTime);
 }else return fail(ApiError::NotFound);
 doc.clear();const auto id=backend_.submit(action,nowMs);if(!id)return fail(ApiError::Busy);
  const std::string location=std::string(action.kind==ActionKind::Command?"/api/v1/commands/":"/api/v1/operations/")+ticketString(*id);doc["id"]=ticketString(*id);doc["state"]="queued";doc["poll"]=location;return jsonReply(202,doc,location);
}
}
#endif
