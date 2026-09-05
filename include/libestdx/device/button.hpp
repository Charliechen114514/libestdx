#include "libestdx/gpio/gpio_base.hpp"

namespace estdx {

template <estdx::GPIOInputPin Pin, estdx::GpioPolarity POLARITY = GpioPolarity::ActiveLow>
struct Button {
    static bool is_pressed() {
        if constexpr (POLARITY == GpioPolarity::ActiveLow) {
            return !Pin::level();
        } else {
            return Pin::level();
        }
    }
};

} // namespace estdx
