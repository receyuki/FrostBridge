#include "BoundedHttp.h"
#include <algorithm>
#include <cctype>
#include <string_view>
namespace frost {
namespace {
std::string lower(std::string_view s) { std::string out(s);for(auto& c:out)c=std::tolower(static_cast<unsigned char>(c));return out; }
std::string_view trim(std::string_view s) {
  while(!s.empty() && (s.front()==' ' || s.front()=='\t')) s.remove_prefix(1);
  while(!s.empty() && (s.back()==' ' || s.back()=='\t')) s.remove_suffix(1);
  return s;
}
}
void HttpParser::reset() {
  headerSize_=bodyLength_=0;headersReady_=false;error_=0;
  request_.method.clear();request_.path.clear();request_.host.clear();request_.origin.clear();request_.contentType.clear();request_.body.clear();
}
size_t HttpParser::feed(const char* bytes,size_t length) {
  size_t used=0;
  while(used<length && !error_ && !complete()) {
    const char c=bytes[used++];
    if(!headersReady_) {
      if(headerSize_>=headerLimit) { error_=431;break; }
      if((c=='\n' && (!headerSize_ || header_[headerSize_-1]!='\r')) ||
         (headerSize_ && header_[headerSize_-1]=='\r' && c!='\n') ||
         (static_cast<unsigned char>(c)<32 && c!='\r' && c!='\n' && c!='\t')) { error_=400;break; }
      header_[headerSize_++]=c;
      if(headerSize_>=4 && std::string_view(header_.data()+headerSize_-4,4)=="\r\n\r\n") parseHeaders();
    } else request_.body.push_back(c);
  }
  return used;
}
void HttpParser::parseHeaders() {
  std::string_view text(header_.data(),headerSize_);const auto end=text.find("\r\n");
  const auto line=text.substr(0,end);const auto first=line.find(' '),last=line.rfind(' ');
  if(first==std::string_view::npos || first==last || line.substr(last+1)!="HTTP/1.1") { error_=400;return; }
  request_.method=std::string(line.substr(0,first));request_.path=std::string(line.substr(first+1,last-first-1));
  if(request_.method!="GET" && request_.method!="POST") { error_=405;return; }
  if(request_.path.empty() || request_.path.front()!='/' || request_.path.size()>96 || request_.path.find(' ')!=std::string::npos) { error_=414;return; }
  text.remove_prefix(end+2);unsigned seen=0,count=0;
  while(text.size()>2) {
    const auto n=text.find("\r\n");const auto header=text.substr(0,n);const auto colon=header.find(':');
    if(++count>32 || colon==std::string_view::npos || !colon || header.front()==' ' || header.front()=='\t') { error_=400;return; }
    for(const auto c:header.substr(0,colon)) {
      if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || std::string_view("!#$%&'*+-.^_`|~").find(c)!=std::string_view::npos)) { error_=400;return; }
    }
    const auto key=lower(header.substr(0,colon));const auto value=trim(header.substr(colon+1));
    unsigned bit=key=="host"?1:key=="origin"?2:key=="content-type"?4:key=="content-length"?8:key=="transfer-encoding"?16:0;
    if(bit && (seen&bit)) { error_=400;return; }seen|=bit;
    if(bit==16) { error_=400;return; }
    if(value.size()>160 && bit) { error_=431;return; }
    if(bit==1) request_.host=std::string(value);
    if(bit==2) request_.origin=std::string(value);
    if(bit==4) request_.contentType=std::string(value);
    if(bit==8) {
      if(value.empty()) { error_=400;return; }
      for(const auto c:value) {
        if(c<'0' || c>'9') { error_=400;return; }
        bodyLength_=bodyLength_*10+unsigned(c-'0');if(bodyLength_>bodyLimit) { error_=413;return; }
      }
    }
    text.remove_prefix(n+2);
  }
  if(request_.host.empty()) { error_=400;return; }
  if(request_.method=="POST" && !(seen&8)) { error_=411;return; }
  if(request_.method=="GET" && bodyLength_) { error_=400;return; }
  request_.body.reserve(bodyLength_);headersReady_=true;
}
}
