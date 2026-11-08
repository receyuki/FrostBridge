#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>
namespace alpicool {
using Frame = std::vector<uint8_t>;
std::array<uint8_t, 6> query();
bool validFrame(const Frame& frame);
enum class Mode { Max, Eco };
enum class Setting { TargetC, Power, Mode };
struct Status {
  std::optional<bool> power, locked;
  std::optional<Mode> mode;
  uint8_t unit = 255;
  std::optional<float> targetC, currentC, minC, maxC, voltage;
  std::optional<uint8_t> batteryPercent, protection;
};
std::optional<Status> decodeStatus(const Frame& frame);
// Only documented layouts/settings are encodable; no hardware side effects.
std::optional<Frame> encodeSetting(const Frame& snapshot, Setting setting, int value);
// sequence identifies the first input byte's position in the receive stream.
// Byte positions distinguish multiple frames in a single notification.
struct Receipt { uint32_t sequence=0,receivedMs=0; };
struct ReceivedFrame { Frame bytes;Receipt first; };
class Decoder {
 public:
  std::vector<Frame> feed(const uint8_t* bytes, size_t length);
  std::vector<ReceivedFrame> feedStamped(const uint8_t* bytes,size_t length,Receipt receipt);
  void reset() { used_ = 0; }
 private:
  std::array<uint8_t, 258> buffer_{};
  std::array<Receipt,258> receipts_{};
  size_t used_ = 0;
};
}
