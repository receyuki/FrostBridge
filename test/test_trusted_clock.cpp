#include "TrustedClock.h"
#include <cassert>
#include <iostream>
using namespace frost;
int main(){
 constexpr int64_t midnight=1794067200000LL;TrustedClock clock;assert(!clock.timeAt(0));
 assert(clock.sync(midnight/1000,0xfffffff0));clock.advance(16);
 assert(clock.timeAt(16)==midnight+32 && clock.timeAt(0xfffffff8)==midnight+8);
 assert(!clock.timeAt(0xffffffe0)); // stamp predates first trusted sync
 uint32_t now=16;for(unsigned minute=0;minute<60*24*60;++minute){now+=60000;clock.advance(now);}
 assert(clock.timeAt(now)==midnight+32+5184000000LL);
 assert(!clock.timeAt(now-60001));assert(!clock.sync(0,now));assert(clock.trusted());
 assert(clock.sync(midnight/1000+6000000,now));assert(!clock.timeAt(now-1));assert(clock.timeAt(now)==midnight+6000000000LL);
 TrustedClock fractional;assert(fractional.sync(midnight/1000,100,875));fractional.advance(350);assert(fractional.timeAt(350)==midnight+1125);assert(!fractional.sync(midnight/1000,350,1000));
 std::cout<<"PASS: trusted clock keeps millisecond time offline across repeated wraps; ambiguous pre-sync admissions stay undated"<<std::endl;
}
