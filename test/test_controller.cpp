#include "FridgeController.h"
#include <cassert>
#include <iostream>
#include <fstream>

alpicool::Frame capturedC15() {
  std::ifstream in("test/fixtures/c15-query-01.hex");assert(in.is_open());
  alpicool::Frame frame;unsigned byte;
  while(in>>std::hex>>byte) frame.push_back(static_cast<uint8_t>(byte));
  return frame;
}
alpicool::Frame changed(alpicool::Frame frame,size_t offset,uint8_t value) {
  frame[offset]=value;unsigned sum=0;
  for(size_t i=0;i+2<frame.size();++i) sum+=frame[i];
  frame[frame.size()-2]=static_cast<uint8_t>(sum>>8);frame.back()=static_cast<uint8_t>(sum);
  return frame;
}

int main() {
  {
    std::vector<alpicool::Frame> sent;frost::Controller replay(61,[&](const alpicool::Frame& f){sent.push_back(f);return true;});
    replay.connection(true,1,100);replay.observe(capturedC15(),1,1,100);const auto cmd=replay.submit({alpicool::Setting::TargetC,4,frost::Source::LocalReplay},110);assert(cmd.id);replay.tick(111);
    assert(sent.size()==1 && sent[0][3]==1 && replay.command(*cmd.id)->state==frost::CommandState::AwaitingReadback);
    replay.observe(capturedC15(),1,2,112);assert(replay.command(*cmd.id)->state==frost::CommandState::Confirmed);
  }

  frost::Controller controller(42, [](const alpicool::Frame&) { return true; });
  const auto rejected=controller.submit({alpicool::Setting::TargetC,5},100);
  assert(!rejected.id && rejected.error==frost::Error::Offline);
  controller.connection(true,7,100);
  assert(controller.observe(capturedC15(),7,1,100));
  const auto accepted=controller.submit({alpicool::Setting::TargetC,5},110);
  assert(accepted.id && accepted.id->boot==42 && accepted.id->sequence==1);
  assert(controller.command(*accepted.id)->state==frost::CommandState::Queued);
  assert(controller.observed().status.targetC==4.0f);
  std::vector<alpicool::Frame> writes;
  frost::Controller sender(43,[&](const alpicool::Frame& f) { writes.push_back(f);return true; });
  sender.connection(true,7,100);assert(sender.observe(capturedC15(),7,1,100));
  const auto pending=sender.submit({alpicool::Setting::TargetC,5},110);
  assert(pending.id);sender.tick(111);
  assert((writes==std::vector<alpicool::Frame>{{0xfe,0xfe,0x04,0x05,0x05,0x02,0x0a}}));
  assert(sender.command(*pending.id)->state==frost::CommandState::AwaitingReadback);
  // Neither successful transport nor the old observation confirms the request.
  assert(sender.observed().status.targetC==4.0f);
  const auto atFive=changed(capturedC15(),8,5); // Synthetic panel change, not a real capture.
  assert(!sender.observe(atFive,6,99,112));
  assert(!sender.observe(atFive,7,1,112));
  assert(sender.command(*pending.id)->state==frost::CommandState::AwaitingReadback);
  assert(sender.observe(atFive,7,2,110)); // Arrived before send, parsed later.
  assert(sender.command(*pending.id)->state==frost::CommandState::AwaitingReadback);
  assert(sender.observe(atFive,7,3,111)); // Same millisecond is insufficient evidence.
  assert(sender.command(*pending.id)->state==frost::CommandState::AwaitingReadback);
  assert(sender.observe(atFive,7,4,112));
  assert(sender.command(*pending.id)->state==frost::CommandState::Confirmed);
  assert(sender.observed().status.targetC==5.0f);
  {
    frost::Controller guarded(44,[](const alpicool::Frame&) { return true; });
    guarded.connection(true,1,100);
    assert(guarded.submit({alpicool::Setting::TargetC,5},100).error==frost::Error::NoState);
    guarded.observe(capturedC15(),1,1,100);
    assert(guarded.submit({alpicool::Setting::TargetC,21},100).error==frost::Error::OutOfRange);
    assert(guarded.submit({alpicool::Setting::TargetC,-21},100).error==frost::Error::OutOfRange);
    assert(guarded.submit({alpicool::Setting::Power,2},100).error==frost::Error::InvalidValue);
    assert(guarded.submit({alpicool::Setting::Mode,2},100).error==frost::Error::InvalidValue);
    assert(guarded.submit({alpicool::Setting::TargetC,5},30101).error==frost::Error::Stale);
    guarded.observe(changed(capturedC15(),13,2),1,2,101);
    assert(guarded.submit({alpicool::Setting::TargetC,5},102).error==frost::Error::UnsupportedUnit);
    guarded.observe(changed(capturedC15(),4,1),1,3,102);
    assert(guarded.submit({alpicool::Setting::TargetC,5},103).error==frost::Error::Locked);
    guarded.observe(changed(capturedC15(),4,2),1,4,103);
    assert(guarded.submit({alpicool::Setting::TargetC,5},104).error==frost::Error::UnknownValue);
  }
  {
    unsigned sends=0;frost::Controller guarded(45,[&](const alpicool::Frame&) { ++sends;return true; });
    guarded.connection(true,1,100);guarded.observe(capturedC15(),1,1,100);
    const auto queued=guarded.submit({alpicool::Setting::TargetC,5},110);assert(queued.id);
    guarded.observe(changed(capturedC15(),4,1),1,2,112);guarded.tick(113);
    assert(sends==0 && guarded.command(*queued.id)->state==frost::CommandState::Failed);
    assert(guarded.command(*queued.id)->error==frost::Error::Locked);
  }
  {
    unsigned sends=0;frost::Controller c(46,[&](const alpicool::Frame&) { ++sends;return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto same=c.submit({alpicool::Setting::TargetC,4},110);c.tick(111);
    assert(same.id && c.command(*same.id)->state==frost::CommandState::Confirmed && sends==0);
    const auto pending=c.submit({alpicool::Setting::TargetC,5},112);c.tick(113);assert(sends==1);
    c.tick(5113);assert(c.command(*pending.id)->state==frost::CommandState::TimedOut);
    assert(c.command(*pending.id)->error==frost::Error::ReadbackTimeout);c.tick(7000);assert(sends==1);
    // Connection loss cancels in-flight and queued, retaining stale observation.
    c.observe(capturedC15(),1,2,7100);
    const auto active=c.submit({alpicool::Setting::TargetC,5},7110);c.tick(7111);
    const auto queued=c.submit({alpicool::Setting::TargetC,6},7112);
    c.connection(false,1,7113);
    assert(c.command(*active.id)->state==frost::CommandState::Cancelled);
    assert(c.command(*queued.id)->state==frost::CommandState::Cancelled);
    assert(!c.fresh(7113) && c.observed().status.targetC==4.0f);
    c.connection(true,2,7200);c.tick(7201);assert(sends==2);
    assert(c.submit({alpicool::Setting::TargetC,5},7201).error==frost::Error::NoState);
    assert(!c.observe(atFive,1,99,7300));
    c.observe(capturedC15(),2,1,7301);c.tick(7302);assert(sends==2);
    frost::Controller rebooted(47,[](const alpicool::Frame&) { return true; });
    assert(!rebooted.command(*active.id));rebooted.tick(8000);
  }
  {
    frost::Controller c(48,[](const alpicool::Frame&) { return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    c.submit({alpicool::Setting::TargetC,5},101);c.submit({alpicool::Setting::Power,0},102);
    c.submit({alpicool::Setting::TargetC,6},103);c.submit({alpicool::Setting::Mode,0},104);
    assert(c.submit({alpicool::Setting::Power,0},105).error==frost::Error::QueueFull);
    assert(c.submit({alpicool::Setting::TargetC,7},106).error==frost::Error::QueueFull);
  }
  {
    std::vector<alpicool::Frame> sent;frost::Controller c(49,[&](const alpicool::Frame& f) { sent.push_back(f);return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto first=c.submit({alpicool::Setting::TargetC,5},101);
    const auto latest=c.submit({alpicool::Setting::TargetC,6},102);
    assert(c.command(*first.id)->state==frost::CommandState::Cancelled);
    assert(c.command(*first.id)->error==frost::Error::Superseded);c.tick(103);
    assert((sent==std::vector<alpicool::Frame>{{0xfe,0xfe,4,5,6,2,0x0b}}));
    const auto queued=c.submit({alpicool::Setting::TargetC,7},104);
    c.submit({alpicool::Setting::TargetC,8},105);
    assert(c.command(*latest.id)->state==frost::CommandState::AwaitingReadback);
    assert(c.command(*queued.id)->state==frost::CommandState::Cancelled);
  }
  {
    frost::Controller c(50,[](const alpicool::Frame&) { return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto oldest=c.submit({alpicool::Setting::TargetC,4},101);c.tick(102);
    for(unsigned i=0;i<20;++i) { auto r=c.submit({alpicool::Setting::TargetC,4},103+i);assert(r.id);c.tick(103+i);assert(c.command(*r.id)->state==frost::CommandState::Confirmed); }
    assert(!c.command(*oldest.id));
  }
  {
    unsigned writes=0;frost::Controller c(51,[&](const alpicool::Frame&) { ++writes;return false; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto r=c.submit({alpicool::Setting::TargetC,5,frost::Source::Schedule},101);c.tick(102);c.tick(9000);
    assert(writes==1 && c.command(*r.id)->state==frost::CommandState::Failed);
    assert(c.command(*r.id)->error==frost::Error::TransportFailed);
    assert(c.command(*r.id)->intent.source==frost::Source::Schedule);
  }
  {
    frost::Controller c(52,[](const alpicool::Frame&) { return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto r=c.submit({alpicool::Setting::TargetC,5},101);c.tick(102);
    // The loop need not tick at exactly the deadline; late evidence still times out.
    c.observe(atFive,1,2,5102);
    assert(c.command(*r.id)->state==frost::CommandState::TimedOut);
    assert(c.observed().status.targetC==5.0f);
  }
  {
    unsigned writes=0;frost::Controller c(53,[&](const alpicool::Frame&) { ++writes;return true; });
    c.connection(true,1,100);c.observe(changed(capturedC15(),32,2),1,1,100);
    assert(c.submit({alpicool::Setting::Power,1},101).error==frost::Error::UnsupportedLayout);
    assert(c.submit({alpicool::Setting::Mode,1},101).error==frost::Error::UnsupportedLayout);
    const auto sameOff=c.submit({alpicool::Setting::Power,0},101);c.tick(102);
    assert(sameOff.id && c.command(*sameOff.id)->state==frost::CommandState::Confirmed && writes==0);
    auto corrupt=changed(capturedC15(),32,2);corrupt[8]^=1;
    assert(!c.observe(corrupt,1,2,105));assert(c.observed().receivedMs==100);
  }
  {
    frost::Controller c(54,[](const alpicool::Frame&) { return true; });
    c.connection(true,1,0xfffffff0);c.observe(capturedC15(),1,1,0xfffffff0);
    const auto r=c.submit({alpicool::Setting::TargetC,5},0xfffffff1);c.tick(0xfffffff2);
    c.observe(atFive,1,2,3);assert(c.command(*r.id)->state==frost::CommandState::Confirmed);
    assert(c.fresh(4));
  }
  {
    unsigned writes=0;frost::Controller c(55,[&](const alpicool::Frame&) { ++writes;return true; });
    c.connection(true,1,100);c.observe(capturedC15(),1,1,100);
    const auto uncertain=c.submit({alpicool::Setting::TargetC,5},101);c.tick(102);
    const auto restore=c.submit({alpicool::Setting::TargetC,4},103);assert(restore.id);
    c.tick(5102);
    assert(c.command(*uncertain.id)->state==frost::CommandState::TimedOut);
    assert(c.command(*restore.id)->state==frost::CommandState::Cancelled);
    assert(c.command(*restore.id)->error==frost::Error::ReadbackRequired);
    assert(!c.fresh(5102) && c.observed().status.targetC==4.0f && writes==1);
    assert(c.submit({alpicool::Setting::TargetC,4},5103).error==frost::Error::NoState);
    assert(!c.observe(capturedC15(),1,1,5103)); // Duplicate cannot reopen control.
    assert(!c.observe(capturedC15(),1,2,101)); // Queued pre-send notification cannot either.
    // A new observation recovers control eligibility, without replaying cancelled work.
    assert(c.observe(atFive,1,2,5104));c.tick(5105);assert(writes==1);
    const auto retry=c.submit({alpicool::Setting::TargetC,4},5106);c.tick(5107);
    assert(retry.id && writes==2 && c.command(*retry.id)->state==frost::CommandState::AwaitingReadback);
  }
  std::cout << "PASS: validation, revalidation, IDs, fresh readback, stale notifications, timeout, disconnect/no replay, bounded queue/history, supersession, transport failure\n";
}
