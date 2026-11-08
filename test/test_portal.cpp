#include "Portal.h"
#include <ArduinoJson.h>
#include <cassert>
#include <iostream>
#include <fstream>
using namespace frost;
void command_contract_is_strict_and_asynchronous() {
  PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.boot=42;snapshot.ready=true;snapshot.writes=false;mail.publish(snapshot);
  PortalApi api({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}},[]{return std::vector<std::string>{"fridge.local"};});
  HttpRequest r;r.method="POST";r.host="fridge.local";r.path="/api/v1/commands";r.contentType="application/json";r.body=R"({"kind":"set_temperature","value":5})";
  r.origin="http://evil.example";assert(api.handle(r,110).status==403);
  r.origin="http://fridge.local";assert(api.handle(r,110).status==409);
  r.body=R"({"kind":"set_temperature","value":"5"})";assert(api.handle(r,110).status==400);
  r.body=R"({"kind":"set_temperature","value":5.0})";assert(api.handle(r,110).status==400);
  r.body=R"({"kind":"set_temperature","value":21})";assert(api.handle(r,110).status==422);
  r.body=R"({"kind":"set_temperature","kind":"set_power","value":true})";assert(api.handle(r,110).status==400);
  r.body=R"({"kind":"set_temperature","value":5})";r.body+="junk";assert(api.handle(r,110).status==400);
  r.body=R"({"kind":"set_temperature","value":5})";snapshot.writes=true;snapshot.connected=true;snapshot.observed=true;snapshot.valid=true;snapshot.epoch=1;snapshot.observedMs=100;mail.publish(snapshot);
  unsigned writes=0;uint32_t now=111;
  DailyReplay daily([](const DailyPersistedState&){return true;});
  ControlRuntime core(9,[&](const alpicool::Frame& f){assert(f[3]==5 && f[4]==5);++writes;return true;},daily,true);
  alpicool::Frame frame{0xfe,0xfe,0x15,1,0,0,0,2,4,20,0xec,2,0,0,0,0,0,0,20,100,14,3,0,0};
  auto checksum=[](alpicool::Frame& f){unsigned sum=0;for(size_t i=0;i+2<f.size();++i)sum+=f[i];f[f.size()-2]=sum>>8;f.back()=sum;};checksum(frame);
  core.connection(true,1,100);core.observe(frame,1,1,100);PortalWorker worker(core,mail,[&]{return now;});
  const auto accepted=api.handle(r,110);assert(accepted.status==202 && writes==0 && accepted.location.rfind("/api/v1/commands/",0)==0);
  worker.tick();assert(writes==1);
  HttpRequest get;get.method="GET";get.host=r.host;get.path=accepted.location;
  assert(api.handle(get,112).body.find("awaiting_readback")!=std::string::npos);
  frame[8]=5;checksum(frame);core.observe(frame,1,2,113);now=114;worker.tick();
  assert(api.handle(get,115).body.find("confirmed")!=std::string::npos);
  mail.reset(43);snapshot.boot=43;mail.publish(snapshot);assert(api.handle(get,116).status==404);
}
void accepted_control_is_cancelled_across_reconnect() {
  PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.boot=42;snapshot.ready=true;snapshot.writes=true;snapshot.connected=true;snapshot.observed=true;snapshot.valid=true;snapshot.epoch=1;mail.publish(snapshot);
  PortalApi api({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}},[]{return std::vector<std::string>{"fridge.local"};});
  unsigned writes=0;uint32_t now=100;
  DailyReplay daily([](const DailyPersistedState&){return true;});
  ControlRuntime core(42,[&](const alpicool::Frame&){++writes;return true;},daily,true);
  alpicool::Frame frame{0xfe,0xfe,0x15,1,0,0,0,2,4,20,0xec,2,0,0,0,0,0,0,20,100,14,3,0,0};
  unsigned sum=0;for(size_t i=0;i+2<frame.size();++i)sum+=frame[i];frame[22]=sum>>8;frame[23]=sum;
  core.connection(true,1,1);core.observe(frame,1,1,1);
  HttpRequest r;r.method="POST";r.host="fridge.local";r.origin="http://fridge.local";r.contentType="application/json";r.path="/api/v1/commands";r.body=R"({"kind":"set_temperature","value":5})";
  auto accepted=api.handle(r,10);assert(accepted.status==202);
  core.connection(false,1,20);core.connection(true,2,30);core.observe(frame,2,2,31);
  PortalWorker worker(core,mail,[&]{return now;});worker.tick();
  r.method="GET";r.path=accepted.location;const auto result=api.handle(r,101);
  assert(writes==0 && result.body.find("cancelled")!=std::string::npos && result.body.find("disconnected")!=std::string::npos);
}

void default_off_backup_status(){
 PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.ready=true;mail.publish(snapshot);
 PortalApi api({[&]{return mail.snapshot();},{},{},{}},[]{return std::vector<std::string>{"fridge.local"};});
 HttpRequest r;r.method="GET";r.host="fridge.local";r.path="/api/v1/backup";
 auto response=api.handle(r,100);assert(response.status==200);JsonDocument json;assert(!deserializeJson(json,response.body));
 assert(json["enabled"]==false && json["sample"].isNull() && json["generation"]==0);
 snapshot.backup.sampleValid=true;snapshot.backup.sampleCount=0;snapshot.backup.sampleDay=20764;mail.publish(snapshot);
 response=api.handle(r,100);assert(!deserializeJson(json,response.body));assert(json["sample"]["count"]==0);
 r.path="/api/v1/plan";assert(api.handle(r,100).status==404);r.host="evil.example";assert(api.handle(r,100).status==403);
 std::cout<<"PASS: daily backup defaults off, distinguishes empty sample from absent sample, removes weekly API\n";
}

void confirmed_backup_configuration_is_async_and_versioned(){
 uint32_t now=100;unsigned saves=0;DailyReplay daily([&](const DailyPersistedState&){++saves;return true;});
 ControlRuntime core(42,[](const alpicool::Frame&){return false;},daily);PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.ready=true;mail.publish(snapshot,daily);PortalWorker worker(core,mail,[&]{return now;});
 PortalApi api({[&]{return mail.snapshot();},[&](WebAction a,uint32_t n){return mail.submit(a,n);},[&](Ticket t){return mail.lookup(t);},[&]{return mail.results();}},[]{return std::vector<std::string>{"fridge.local"};});
 HttpRequest r;r.host="fridge.local";r.origin="http://fridge.local";r.method="POST";r.contentType="application/json";r.path="/api/v1/backup/preview";r.body=R"({"base_generation":0,"enabled":true})";
 const auto preview=api.handle(r,now);assert(preview.status==202 && saves==0);worker.tick();r.method="GET";r.path=preview.location;JsonDocument json;auto response=api.handle(r,now);assert(!deserializeJson(json,response.body));assert(json["state"]=="completed" && json["effect"]["state"]=="waiting_for_time");const std::string token=json["preview_id"].as<std::string>();
 r.method="POST";r.path="/api/v1/backup/apply";r.body="{\"preview_id\":\""+token+"\"}";auto apply=api.handle(r,now);assert(apply.status==202 && saves==0);worker.tick();assert(saves==1 && daily.state().enabled);r.method="GET";r.path=apply.location;response=api.handle(r,now);assert(!deserializeJson(json,response.body));assert(json["generation"]==1 && json["state"]=="completed");
 r.method="POST";r.path="/api/v1/backup/apply";auto stale=api.handle(r,now);assert(stale.status==202);worker.tick();r.method="GET";r.path=stale.location;assert(api.handle(r,now).body.find("stale_preview")!=std::string::npos && saves==1);
 snapshot.generation=1;mail.publish(snapshot,daily);r.method="POST";r.path="/api/v1/backup/preview";r.body=R"({"base_generation":0,"enabled":false})";assert(api.handle(r,now).status==409);
 r.body=R"({"base_generation":1,"enabled":1})";assert(api.handle(r,now).status==400);r.body=R"({"base_generation":1,"enabled":true,"enabled":false})";assert(api.handle(r,now).status==400);
 std::cout<<"PASS: backup preview is read-only; confirmed save publishes generation and rejects reused/stale/ill-typed previews\n";
}

void bounded_daily_pages_are_boot_and_revision_scoped(){
 DailyPersistedState saved;saved.sample.valid=true;saved.sample.day=20764;saved.sample.count=64;
 for(unsigned i=0;i<64;++i)saved.sample.actions[i]={static_cast<uint32_t>(i*600),DailyField::Temperature,static_cast<int8_t>(i%41-20),static_cast<uint16_t>(i)};
 DailyReplay daily([](const DailyPersistedState&){return true;});assert(daily.restore(saved));PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.ready=true;mail.publish(snapshot,daily);
 ApiBackend backend;backend.snapshot=[&]{return mail.snapshot();};backend.page=[&](bool sample,uint8_t offset,uint32_t revision){return mail.page(sample,offset,revision);};PortalApi api(backend,[]{return std::vector<std::string>{"fridge.local"};});
 HttpRequest r;r.method="GET";r.host="fridge.local";r.path="/api/v1/backup";JsonDocument json;assert(!deserializeJson(json,api.handle(r,100).body));std::string url=json["sample"]["records_url"].as<std::string>();unsigned seen=0;
 while(!url.empty()){r.path=url;auto response=api.handle(r,100);assert(response.status==200 && response.body.size()<2048);assert(!deserializeJson(json,response.body));assert(json["total"]==64 && json["records"].size()==8);seen+=json["records"].size();url=json["next"].isNull()?"":json["next"].as<std::string>();}assert(seen==64);
 r.path="/api/v1/backup/records/0000002a/sample/1/1";assert(api.handle(r,100).status==400);
 r.path="/api/v1/backup/records/0000002a/sample/1/64";assert(api.handle(r,100).status==400);
 r.path="/api/v1/backup/records/0000002a/sample/1/0";saved.sample.count=0;assert(daily.restore(saved));mail.publish(snapshot,daily);assert(api.handle(r,100).status==409);
 r.path="/api/v1/backup";assert(!deserializeJson(json,api.handle(r,100).body));r.path=json["sample"]["records_url"].as<std::string>();assert(!deserializeJson(json,api.handle(r,100).body));assert(json["records"].size()==0 && json["total"]==0);
 backend.page=[&](bool,uint8_t,uint32_t){mail.reset(43);return DailyPage{};};PortalApi racing(backend,[]{return std::vector<std::string>{"fridge.local"};});assert(racing.handle(r,100).status==409);
 std::cout<<"PASS: maximum daily sample uses eight bounded pages; revision/boot changes require reload, valid empty remains visible\n";
}

void activity_keeps_automation_source_and_bounded_history(){
 PortalMailbox mail;mail.reset(42);WebSnapshot snapshot;snapshot.ready=true;snapshot.automaticCount=4;for(unsigned i=0;i<4;++i){snapshot.automatic[i].id={42,i+1};snapshot.automatic[i].intent.source=Source::LocalReplay;}mail.publish(snapshot);
 for(unsigned i=0;i<20;++i){WebAction action;action.intent.source=Source::HomeKitAutomation;const auto id=mail.submit(action,i);assert(id);mail.take();auto result=*mail.lookup(*id);result.state=WebState::Confirmed;mail.put(result);}
 PortalApi api({[&]{return mail.snapshot();},{},{},[&]{return mail.results();}},[]{return std::vector<std::string>{"fridge.local"};});HttpRequest r;r.method="GET";r.host="fridge.local";r.path="/api/v1/activity";const auto response=api.handle(r,100);JsonDocument json;assert(response.status==200 && response.body.size()<=4096 && !deserializeJson(json,response.body));assert(json["operations"].size()==8 && json["automatic"].size()==4 && json["operations"][0]["source"]=="homekit_automation");assert(json["automatic"][0]["type"]=="local_replay");
 std::cout<<"PASS: activity retains HomeKit automation identity and bounded independent local replay history\n";
}
void http_errors_default_to_english(){
 PortalApi api({},[]{return std::vector<std::string>{"fridge.local"};});HttpRequest request;request.method="GET";request.host="other.local";request.path="/api/v1/status";
 const auto response=api.handle(request,100);JsonDocument json;assert(response.status==403 && !deserializeJson(json,response.body));assert(json["error"]["code"]=="forbidden_host");assert(json["error"]["message"]=="Use the gateway's own address.");
 std::cout<<"PASS: HTTP errors default to English while preserving status and machine-readable code\n";
}
int main(){http_errors_default_to_english();command_contract_is_strict_and_asynchronous();accepted_control_is_cancelled_across_reconnect();default_off_backup_status();confirmed_backup_configuration_is_async_and_versioned();bounded_daily_pages_are_boot_and_revision_scoped();activity_keeps_automation_source_and_bounded_history();}
