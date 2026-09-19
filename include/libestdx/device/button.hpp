#pragma once
#include "libestdx/gpio/gpio_base.hpp"

namespace estdx::device {

template <gpio::GPIOInputPin Pin, gpio::GpioPolarity POLARITY = gpio::GpioPolarity::ActiveLow>
struct Button {
    static bool is_pressed() {
        if constexpr (POLARITY == gpio::GpioPolarity::ActiveLow) {
            return !Pin::level();
        } else {
            return Pin::level();
        }
    }
};

} // namespace estdx::device
