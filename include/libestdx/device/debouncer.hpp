#pragma once
#include <cstdint>

namespace estdx::device {

template <std::uint32_t INTERVAL_MS, std::uint8_t STABLE_COUNT>
struct Debouncer {
    static_assert(STABLE_COUNT > 0, "one sample is not consensus");

    bool update(bool raw, std::uint32_t now_ms) {
        if (now_ms - last_sample_ms_ < INTERVAL_MS) {
            return false;
        }
        last_sample_ms_ = now_ms;

        if (raw == candidate_) {
            if (count_ < STABLE_COUNT) {
                ++count_;
            }
        } else {
            candidate_ = raw;
            count_ = 1;
        }

        if (count_ >= STABLE_COUNT && candidate_ != stable_) {
            stable_ = candidate_;
            return true;
        }
        return false;
    }

    bool stable() const { return stable_; }

  private:
    // 初值让第一次 update 立即到期:0 - (0 - INTERVAL) = INTERVAL,不小于门槛。
    std::uint32_t last_sample_ms_ = 0 - INTERVAL_MS;
    bool stable_ = false;
    bool candidate_ = false;
    std::uint8_t count_ = 0;
};

} // namespace estdx::device
