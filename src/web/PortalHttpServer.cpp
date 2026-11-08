#ifdef FROSTBRIDGE_GATEWAY
#include "PortalHttpServer.h"
#include "GeneratedWeb.h"
#include <algorithm>
#include <cerrno>
#include <lwip/sockets.h>
namespace frost {
namespace {
const char* reason(int status) {
  switch(status){case 200:return "OK";case 202:return "Accepted";case 400:return "Bad Request";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 408:return "Request Timeout";case 409:return "Conflict";case 411:return "Length Required";case 413:return "Payload Too Large";case 414:return "URI Too Long";case 415:return "Unsupported Media Type";case 422:return "Unprocessable Content";case 429:return "Too Many Requests";case 431:return "Request Header Fields Too Large";default:return "Service Unavailable";}
}
bool transient() {return errno==EWOULDBLOCK || errno==EAGAIN || errno==EINTR;}
}
void PortalHttpServer::begin(){server_.begin();server_.setNoDelay(true);}
void PortalHttpServer::respond(Client& c,int status,std::string body,const char* type,const char* location,const uint8_t* asset,size_t size) {
  c.body=std::move(body);c.asset=asset;c.assetSize=asset?size:c.body.size();c.sent=0;
  c.headers="HTTP/1.1 "+std::to_string(status)+" "+reason(status)+"\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(c.assetSize)+"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'\r\n";
  if(asset)c.headers+="Content-Encoding: gzip\r\n";
  if(location && *location)c.headers+=std::string("Location: ")+location+"\r\n";
  c.headers+="\r\n";c.phase=Client::Writing;c.since=millis();
}
void PortalHttpServer::route(Client& c,uint32_t nowMs) {
  const auto& r=c.parser.request();
  if(!api_.allowedHost(r.host)){const auto out=PortalApi::failure(ApiError::ForbiddenHost);respond(c,out.status,out.body,"application/json; charset=utf-8");return;}
  if(r.method=="GET")for(const auto& asset:webassets::files)if(r.path==asset.path || (r.path=="/index.html" && std::string(asset.path)=="/")){respond(c,200,{},asset.type,nullptr,asset.bytes,asset.size);return;}
  const auto out=api_.handle(r,nowMs);respond(c,out.status,out.body,"application/json; charset=utf-8",out.location.c_str());
}
void PortalHttpServer::poll(uint32_t nowMs) {
  for(auto& c:clients_)if(c.phase==Client::Empty) {
    auto incoming=server_.accept();if(!incoming)break;
    c.socket=std::move(incoming);c.socket.setNoDelay(true);c.since=nowMs;c.phase=Client::Reading;
  }
  for(auto& c:clients_) {
    if(c.phase==Client::Empty)continue;
    const auto fd=c.socket.fd();bool close=false;char input[512];
    if(c.phase==Client::Reading) {
      const int n=::recv(fd,input,sizeof(input),MSG_DONTWAIT);
      if(n>0){c.parser.feed(input,n);if(c.parser.error())respond(c,c.parser.error(),"{\"error\":{\"code\":\"invalid_http\",\"message\":\"HTTP request rejected\"}}","application/json; charset=utf-8");else if(c.parser.complete())route(c,nowMs);}
      else if(n==0 || !transient())close=true;
      if(c.phase==Client::Reading && nowMs-c.since>=5000)respond(c,408,"{\"error\":{\"code\":\"request_timeout\"}}","application/json; charset=utf-8");
    }
    if(c.phase==Client::Writing) {
      const size_t total=c.headers.size()+c.assetSize;
      const bool header=c.sent<c.headers.size();const size_t offset=header?c.sent:c.sent-c.headers.size();
      const uint8_t* data=header?reinterpret_cast<const uint8_t*>(c.headers.data()):c.asset?c.asset:reinterpret_cast<const uint8_t*>(c.body.data());
      const size_t length=header?c.headers.size():c.assetSize;
      const int n=::send(fd,data+offset,std::min(size_t(1024),length-offset),MSG_DONTWAIT);
      if(n>0)c.sent+=n;else if(n<0 && !transient())close=true;
      if(c.sent==total){::shutdown(fd,SHUT_WR);c.phase=Client::Draining;c.since=nowMs;}
      else if(millis()-c.since>=5000)close=true;
    }
    if(c.phase==Client::Draining) {
      const int n=::recv(fd,input,sizeof(input),MSG_DONTWAIT);
      if(n==0 || (n<0 && !transient()) || nowMs-c.since>=100)close=true;
    }
    if(close){
      c.socket.stop();c.parser.reset();c.headers.clear();std::string{}.swap(c.body);c.asset=nullptr;c.assetSize=c.sent=0;c.phase=Client::Empty;
    }
  }
}
}
#endif
