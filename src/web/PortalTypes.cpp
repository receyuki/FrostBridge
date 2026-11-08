#include "Portal.h"
#include <cstdio>
namespace frost {
namespace {
struct ErrorInfo {const char* code;const char* message;int status;};
constexpr ErrorInfo errors[]{
 {"none","",200},{"bad_request","Invalid request.",400},{"forbidden_host","Use the gateway's own address.",403},
 {"forbidden_origin","Request origin is not allowed.",403},{"unsupported_content_type","Send JSON content.",415},
 {"payload_too_large","Request is too large or complex.",413},{"queue_full","The queue is full. Please retry shortly.",429},
 {"gateway_not_ready","Gateway is starting. Please wait.",503},{"not_found","Record not found or expired.",404},
 {"writes_disabled","Device writes are disabled in this build.",409},{"invalid_time","Time is outside the supported range.",422},
 {"stale_preview","State changed. Preview again before saving.",409},{"device_offline","Fridge is disconnected.",503},
 {"state_unknown","No usable device state yet.",409},{"state_stale","Device readings are stale. Waiting for an update.",409},
 {"locked","The fridge controls are locked.",409},{"out_of_range","Temperature is out of range.",422},
 {"unsupported_unit","Select Celsius on the fridge first.",409},{"unsupported_layout","This device layout does not support this control.",409},
 {"persistence_failed","Save not confirmed. Reload, check the settings and retry.",503},{"transport_failed","Device write was not confirmed. Wait for a fresh reading.",409},
 {"readback_timeout","No timely device readback confirmation.",409},{"disconnected","Connection closed. Please submit again.",503},
 {"superseded","A newer operation replaced this request.",409},{"readback_required","Wait for a fresh device reading before resubmitting.",409},
 {"result_expired","Result expired. Check the current device state.",404},{"records_changed","Records changed. Reloading.",409},
 {"request_expired","Request waited too long. Please submit again.",409}
};
static_assert(sizeof(errors)/sizeof(errors[0])==static_cast<size_t>(ApiError::RequestExpired)+1,"error table");
const ErrorInfo& info(ApiError error){const auto i=static_cast<size_t>(error);return errors[i<sizeof(errors)/sizeof(errors[0])?i:1];}
}
ApiError apiError(Error e){
 switch(e){
  case Error::None:return ApiError::None;case Error::Offline:return ApiError::Offline;case Error::NoState:case Error::UnknownValue:return ApiError::UnknownState;
  case Error::Stale:return ApiError::Stale;case Error::UnsupportedUnit:return ApiError::UnsupportedUnit;case Error::Locked:return ApiError::Locked;
  case Error::OutOfRange:return ApiError::OutOfRange;case Error::InvalidValue:return ApiError::BadRequest;case Error::UnsupportedLayout:return ApiError::UnsupportedLayout;
  case Error::TransportFailed:return ApiError::Transport;case Error::ReadbackTimeout:return ApiError::Timeout;case Error::Disconnected:return ApiError::Disconnected;
  case Error::QueueFull:return ApiError::Busy;case Error::Superseded:return ApiError::Superseded;case Error::ReadbackRequired:return ApiError::ReadbackRequired;
  case Error::WritesDisabled:return ApiError::WritesDisabled;case Error::PersistenceFailed:return ApiError::Persistence;case Error::RequestExpired:return ApiError::RequestExpired;
 }
 return ApiError::UnknownState;
}
ApiError apiError(BackupError error){switch(error){case BackupError::None:return ApiError::None;case BackupError::StalePreview:return ApiError::StalePreview;case BackupError::PersistenceFailed:return ApiError::Persistence;}return ApiError::BadRequest;}
const char* errorCode(ApiError error){return info(error).code;}
const char* errorMessage(ApiError error){return info(error).message;}
int errorStatus(ApiError error){return info(error).status;}
const char* stateName(WebState state){constexpr const char* names[]{"queued","running","awaiting_readback","confirmed","completed","failed","timed_out","cancelled"};const auto i=static_cast<size_t>(state);return i<8?names[i]:"failed";}
const char* replayModeName(ReplayMode mode){constexpr const char* names[]{"disabled","waiting_for_time","homekit_primary","learning","checking_link","manual_override","replaying"};const auto i=static_cast<size_t>(mode);return i<7?names[i]:"disabled";}
const char* dayGapCode(DayGap gap){constexpr const char* names[]{"none","partial_start","time_unknown","clock_changed","input_gap","homekit_unavailable","persistence","rejected","failed","overflow","pending_at_midnight","device_unavailable"};const auto i=static_cast<size_t>(gap);return i<12?names[i]:"input_gap";}
std::string ticketString(Ticket id){char text[18];std::snprintf(text,sizeof(text),"%08x-%08x",static_cast<unsigned>(id.boot),static_cast<unsigned>(id.sequence));return text;}
std::optional<Ticket> parseTicket(const std::string& text){
 if(text.size()!=17 || text[8]!='-')return std::nullopt;
 Ticket id;for(unsigned i=0;i<17;++i){if(i==8)continue;const char c=text[i];const unsigned n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:16;if(n>15)return std::nullopt;auto& part=i<8?id.boot:id.sequence;part=(part<<4)|n;}
 return id.sequence?std::optional<Ticket>(id):std::nullopt;
}
}
