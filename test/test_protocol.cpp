#include "AlpicoolProtocol.h"
#include <array>
#include <cassert>
#include <iostream>
#include <cmath>
#include <fstream>

int main() {
  const std::array<unsigned char, 6> expected{0xfe, 0xfe, 0x03, 0x01, 0x02, 0x00};
  assert(alpicool::query() == expected);
  const alpicool::Frame capture{0xfe,0xfe,0x15,0x01,0x00,0x01,0x00,0x00,0xf1,0x14,0xec,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0x64,0x0c,0x03,0x05,0x6c};
  alpicool::Decoder decoder;
  const auto frames = decoder.feed(capture.data(), capture.size());
  assert(frames.size() == 1 && frames[0] == capture);
  alpicool::Decoder fragments;
  assert(fragments.feed(capture.data(), 10).empty());
  auto joined = fragments.feed(capture.data()+10, capture.size()-10);
  assert(joined.size() == 1 && joined[0] == capture);
  const auto status = alpicool::decodeStatus(capture);
  assert(status && status->power == true && status->mode == alpicool::Mode::Max);
  assert(status->targetC == -15.0f && status->currentC == -13.0f);
  assert(status->voltage && std::fabs(*status->voltage-12.3f) < 0.01f);
  // V2 is another real upstream README capture, not our C15.
  const alpicool::Frame unknownBattery{0xfe,0xfe,0x15,0x01,0x00,0x01,0x00,0x00,0xec,0x14,0xec,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xf7,0x7f,0x0b,0x01,0x05,0x83};
  auto second=alpicool::decodeStatus(unknownBattery);
  assert(second && !second->batteryPercent && second->targetC==-20.0f && second->currentC==-9.0f);
  for(size_t split=1;split<capture.size();++split) {
    alpicool::Decoder d;
    assert(d.feed(capture.data(),split).empty());
    assert(d.feed(capture.data()+split,capture.size()-split)==std::vector<alpicool::Frame>{capture});
  }
  alpicool::Frame concatenated{0x01,0xfe,0x07,0x22};
  concatenated.insert(concatenated.end(),capture.begin(),capture.end());
  concatenated.insert(concatenated.end(),unknownBattery.begin(),unknownBattery.end());
  alpicool::Decoder many;
  assert(many.feed(concatenated.data(),concatenated.size())==(std::vector<alpicool::Frame>{capture,unknownBattery}));
  auto corrupt=capture;corrupt[10]^=1;
  alpicool::Decoder recovery;
  assert(recovery.feed(corrupt.data(),corrupt.size()).empty());
  assert(!alpicool::decodeStatus(corrupt));
  assert(recovery.feed(capture.data(),capture.size())==std::vector<alpicool::Frame>{capture});
  const alpicool::Frame shortResponse{0xfe,0xfe,0x03,0x01,0x02,0x00};
  assert(!alpicool::decodeStatus(shortResponse));
  alpicool::Decoder invalidLength;
  const uint8_t badLength[]{0xfe,0xfe,0x00};
  assert(invalidLength.feed(badLength,sizeof(badLength)).empty());
  assert(invalidLength.feed(capture.data(),capture.size())==std::vector<alpicool::Frame>{capture});
  // Synthetic derived fixtures: Fahrenheit 32/50 => Celsius 0/10.
  const alpicool::Frame fahrenheit{0xfe,0xfe,0x15,0x1,0x0,0x1,0x0,0x0,0x20,0x44,0xfc,0x2,0x0,0x1,0x0,0x0,0x0,0x0,0x32,0x64,0xc,0x3,0x4,0x1b};
  const auto f=alpicool::decodeStatus(fahrenheit);
  assert(f && f->targetC==0.0f && f->currentC==10.0f && f->minC==-20.0f);
  const alpicool::Frame unknownUnit{0xfe,0xfe,0x15,0x1,0x0,0x1,0x0,0x0,0x20,0x44,0xfc,0x2,0x0,0x2,0x0,0x0,0x0,0x0,0x32,0x64,0xc,0x3,0x4,0x1c};
  const auto u=alpicool::decodeStatus(unknownUnit);
  assert(u && !u->targetC && !u->currentC && !u->minC);
  // Real target C15, Query-only capture. These are protocol interpretations;
  // human panel verification is recorded separately, not asserted here.
  std::ifstream captured("test/fixtures/c15-query-01.hex");
  assert(captured.is_open());
  alpicool::Frame c15;
  unsigned value;
  while (captured >> std::hex >> value) {
    assert(value<=255);
    c15.push_back(static_cast<uint8_t>(value));
  }
  assert(c15.size()==36 && alpicool::validFrame(c15));
  const auto actual=alpicool::decodeStatus(c15);
  assert(actual && actual->power==false && actual->mode==alpicool::Mode::Max);
  assert(actual->unit==0 && actual->targetC==4.0f && actual->currentC==20.0f);
  assert(actual->voltage && std::fabs(*actual->voltage-14.3f)<0.01f);
  const auto c15on=alpicool::encodeSetting(c15,alpicool::Setting::Power,1);
  assert(c15on && c15on->size()==20 && alpicool::validFrame(*c15on) && (*c15on)[5]==1);
  for(size_t i=4;i<18;++i)if(i!=5)assert((*c15on)[i]==c15[i]);
  // Derived powered-on state for the model1 off-direction check, not a new capture.
  auto c15powered=c15;c15powered[5]=1;unsigned c15sum=0;for(size_t i=0;i+2<c15powered.size();++i)c15sum+=c15powered[i];c15powered[c15powered.size()-2]=c15sum>>8;c15powered.back()=c15sum;
  const auto c15off=alpicool::encodeSetting(c15powered,alpicool::Setting::Power,0);
  assert(c15off && c15off->size()==20 && alpicool::validFrame(*c15off) && (*c15off)[5]==0);
  for(size_t i=4;i<18;++i)if(i!=5)assert((*c15off)[i]==c15powered[i]);
  const auto c15eco=alpicool::encodeSetting(c15,alpicool::Setting::Mode,1);
  assert(c15eco && c15eco->size()==20 && (*c15eco)[6]==1);
  for(size_t i=4;i<18;++i)if(i!=6)assert((*c15eco)[i]==c15[i]);
  const auto eco=alpicool::encodeSetting(capture,alpicool::Setting::Mode,1);
  // Independent pinned upstream encode_set_command, single-zone branch.
  const alpicool::Frame expectedEco{0xfe,0xfe,0x11,0x02,0x00,0x01,0x01,0x00,0xf1,0x14,0xec,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04};
  assert(eco && *eco==expectedEco);
  auto calibrated=capture;calibrated[14]=0xfd;calibrated[15]=2;calibrated[16]=0xff;
  unsigned calibratedSum=0;for(size_t i=0;i+2<calibrated.size();++i) calibratedSum+=calibrated[i];
  calibrated[calibrated.size()-2]=calibratedSum>>8;calibrated.back()=calibratedSum;
  const auto off=alpicool::encodeSetting(calibrated,alpicool::Setting::Power,0);
  assert(off && alpicool::validFrame(*off) && (*off)[5]==0);
  for(size_t i=4;i<18;++i) if(i!=5) assert((*off)[i]==calibrated[i]);
  for(size_t split=1;split<c15.size();++split) {
    alpicool::Decoder d;
    assert(d.feed(c15.data(),split).empty());
    assert(d.feed(c15.data()+split,c15.size()-split)==std::vector<alpicool::Frame>{c15});
  }
  alpicool::Decoder stamped;
  assert(stamped.feedStamped(c15.data(),10,{10,100}).empty());
  const auto stampedFrame=stamped.feedStamped(c15.data()+10,c15.size()-10,{20,120});
  assert(stampedFrame.size()==1 && stampedFrame[0].bytes==c15);
  assert(stampedFrame[0].first.sequence==10 && stampedFrame[0].first.receivedMs==100);
  // Resynchronization must preserve provenance of the eventual header.
  alpicool::Decoder resyncStamp;
  const uint8_t noise[]{1,2,3};resyncStamp.feedStamped(noise,3,{12,130});
  const auto recoveredStamp=resyncStamp.feedStamped(c15.data(),c15.size(),{15,140});
  assert(recoveredStamp.size()==1 && recoveredStamp[0].first.sequence==15 && recoveredStamp[0].first.receivedMs==140);
  alpicool::Decoder stampedMany;
  auto joinedStamped=c15;joinedStamped.insert(joinedStamped.end(),c15.begin(),c15.end());
  const auto twoStamped=stampedMany.feedStamped(joinedStamped.data(),joinedStamped.size(),{100,200});
  assert(twoStamped.size()==2 && twoStamped[0].first.sequence==100 && twoStamped[1].first.sequence==136);
  assert(twoStamped[0].first.receivedMs==200 && twoStamped[1].first.receivedMs==200);
  std::cout << "PASS: real C15 36-byte query capture, checksum, status and all split points\n";
  std::cout << "PASS: upstream query/status, all split points, noise/multiple frames, corruption recovery, unknown battery, short/invalid length\n";
}
