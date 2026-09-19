#pragma once
#include "libestdx/gpio/gpio_base.hpp"

namespace estdx::device {

template <gpio::GPIOOutputPin Pin, gpio::GpioPolarity POLARITY = gpio::GpioPolarity::ActiveHigh>
struct LED {
    static void on() {
        if constexpr (POLARITY == gpio::GpioPolarity::ActiveHigh) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    static void off() {
        if constexpr (POLARITY == gpio::GpioPolarity::ActiveLow) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    // Take it easy :)
    static void toggle() { Pin::toggle(); }
};

} // namespace estdx::device
