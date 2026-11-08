#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace frost {
struct HttpRequest { std::string method,path,host,origin,contentType,body; };
class HttpParser {
 public:
  static constexpr size_t headerLimit=2048,bodyLimit=2048;
  void reset();
  size_t feed(const char* bytes,size_t length);
  bool complete() const { return headersReady_ && !error_ && request_.body.size()==bodyLength_; }
  int error() const { return error_; }
  const HttpRequest& request() const { return request_; }
 private:
  std::array<char,headerLimit+1> header_{};
  size_t headerSize_=0,bodyLength_=0;
  bool headersReady_=false;
  int error_=0;
  HttpRequest request_;
  void parseHeaders();
};
}
