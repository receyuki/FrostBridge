#pragma once
#ifdef FROSTBRIDGE_GATEWAY
#include "Portal.h"
#include <NetworkServer.h>
#include <NetworkClient.h>
namespace frost {
// Poll only from the HomeSpan loop. No socket operation waits for the peer.
class PortalHttpServer {
 public:
  PortalHttpServer(ApiBackend backend,std::function<std::vector<std::string>()> hosts):api_(std::move(backend),std::move(hosts)),server_(80,4) {}
  void begin();
  void poll(uint32_t nowMs);
 private:
  struct Client {
    NetworkClient socket;HttpParser parser;
    std::string headers,body;
    const uint8_t* asset=nullptr;size_t assetSize=0,sent=0;
    uint32_t since=0;enum Phase { Empty,Reading,Writing,Draining } phase=Empty;
  };
  PortalApi api_;NetworkServer server_;std::array<Client,2> clients_;
  void respond(Client& client,int status,std::string body,const char* type,const char* location=nullptr,const uint8_t* asset=nullptr,size_t size=0);
  void route(Client& client,uint32_t nowMs);
};
}
#endif
