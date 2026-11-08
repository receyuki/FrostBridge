#pragma once
#ifdef FROSTBRIDGE_GATEWAY
#include "Portal.h"
namespace frost::hap {
void begin(ApiBackend backend);
void poll(uint32_t now);
void diagnostics(WebSnapshot& snapshot,uint32_t now);
}
#endif
