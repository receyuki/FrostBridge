#include "BoundedHttp.h"
#include <cassert>
#include <iostream>
using namespace frost;
int main() {
  HttpParser parser;
  const std::string oversized="POST /api/v1/commands HTTP/1.1\r\nHost: fridge.local\r\nContent-Length: 99999999\r\nContent-Type: application/json\r\n\r\n";
  parser.feed(oversized.data(),oversized.size());assert(parser.error()==413 && !parser.complete());
  HttpParser valid;const std::string request="GET /api/v1/status HTTP/1.1\r\nHost: fridge.local\r\n\r\n";
  valid.feed(request.data(),request.size());assert(valid.complete() && valid.request().path=="/api/v1/status" && valid.request().host=="fridge.local");
  HttpParser malformed;const std::string bad="GET / HTTP/1.1\r\nHost: fridge.local\r\nBad Header: value\r\n\r\n";
  malformed.feed(bad.data(),bad.size());assert(malformed.error()==400);
  const std::string post="POST /api/v1/clock HTTP/1.1\r\nHost: fridge.local\r\nContent-Length: 2\r\n\r\n{}";
  for(size_t cut=0;cut<=post.size();++cut){HttpParser p;p.feed(post.data(),cut);p.feed(post.data()+cut,post.size()-cut);assert(p.complete() && p.request().body=="{}");}
  for(const auto& headers:{"Host: fridge.local\r\nHost: other.local", "Host: fridge.local\r\nTransfer-Encoding: chunked", "Host: fridge.local\r\nContent-Length: 0\r\nContent-Length: 0"}) {
    HttpParser p;const std::string text=std::string("GET / HTTP/1.1\r\n")+headers+"\r\n\r\n";p.feed(text.data(),text.size());assert(p.error()==400);
  }
  malformed.reset();malformed.feed(request.data(),request.size());assert(malformed.complete() && !malformed.error());
  std::cout<<"PASS: bounded HTTP rejects oversized length before reading body\n";
}
