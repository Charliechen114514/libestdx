#include "libestdx/gpio/gpio_base.hpp"

namespace estdx {

template <estdx::GPIOOutputPin Pin, estdx::GpioPolarity POLARITY = GpioPolarity::ActiveHigh>
struct LED {
    static void on() {
        if constexpr (POLARITY == GpioPolarity::ActiveHigh) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    static void off() {
        if constexpr (POLARITY == GpioPolarity::ActiveLow) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    // Take it easy :)
    static void toggle() { Pin::toggle(); }
};

} // namespace estdx
