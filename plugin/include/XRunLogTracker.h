#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

// Message-thread bookkeeping for the standalone audio log. The device count
// comes from the backend (JACK/ALSA); JUCE's manager count also includes its
// callback-time estimate, so these two figures must never be added together.
class XRunLogTracker {
public:
  enum class Reason { baseline, increase, heartbeat, summary };

  struct Report {
    Reason reason;
    int deviceTotal;
    int deviceDelta;
    int managerTotal;
    int managerDelta;
    std::uint64_t intervalMs;
  };

  static constexpr std::uint64_t kHeartbeatMs = 60'000;

  std::optional<Report> observe(const void* device, int deviceCount,
                                int managerCount, std::uint64_t nowMs,
                                bool closing = false) {
    if (device == nullptr) {
      reset();
      return std::nullopt;
    }

    // The backend count can be -1 when unavailable. A new/reopened device or
    // a counter reset starts a new baseline, never a negative delta.
    if (!active || device != lastDevice || nowMs < lastReportMs ||
        (deviceCount >= 0 && lastDeviceCount >= 0 && deviceCount < lastDeviceCount) ||
        (deviceCount < 0 && managerCount < lastManagerCount)) {
      active = true;
      lastDevice = device;
      lastDeviceCount = deviceCount;
      lastManagerCount = managerCount;
      lastReportMs = nowMs;
      return Report{Reason::baseline, deviceCount, 0, managerCount, 0, 0};
    }

    const int deviceDelta = deviceCount >= 0 && lastDeviceCount >= 0
                                ? std::max(0, deviceCount - lastDeviceCount) : 0;
    const int managerDelta = std::max(0, managerCount - lastManagerCount);
    // JUCE's callback-time estimate may reset independently of the backend
    // counter. Preserve a simultaneous device XRUN and rebase that estimate.
    if (managerCount < lastManagerCount)
      lastManagerCount = managerCount;
    const auto elapsed = nowMs - lastReportMs;
    if (deviceDelta == 0 && managerDelta == 0 && elapsed < kHeartbeatMs && !closing)
      return std::nullopt;

    lastDeviceCount = deviceCount;
    lastManagerCount = managerCount;
    lastReportMs = nowMs;
    return Report{closing ? Reason::summary :
                  deviceDelta > 0 || managerDelta > 0 ? Reason::increase : Reason::heartbeat,
                  deviceCount, deviceDelta, managerCount, managerDelta, elapsed};
  }

  void reset() {
    active = false;
    lastDevice = nullptr;
  }

private:
  bool active = false;
  const void* lastDevice = nullptr;
  int lastDeviceCount = -1;
  int lastManagerCount = 0;
  std::uint64_t lastReportMs = 0;
};
