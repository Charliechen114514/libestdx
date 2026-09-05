#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>
namespace estdx {
enum class GpioDirection { Input, Output };
enum class GpioPolarity { ActiveHigh, ActiveLow };
enum class GpioPull { NoPull, Up, Down };

template <typename Concrete>
concept GPIOPin = requires {
    { Concrete::mask } -> std::convertible_to<uint32_t>; // GPIO的掩码
    { Concrete::direction } -> std::convertible_to<GpioDirection>;

    Concrete::port; // Requires Port type
    Concrete::pin;
};

template <typename Concrete>
// Firstly, it must BE a pin
concept GPIOOutputPin = GPIOPin<Concrete> && requires {
    Concrete::direction == GpioDirection::Output;
    Concrete::set();    // On
    Concrete::reset();  // Off
    Concrete::toggle(); // Flip
};

template <typename Concrete>
concept GPIOInputPin = GPIOPin<Concrete> && requires {
    Concrete::direction == GpioDirection::Input;
    // Tells level sync, as aysnc requires wrapper of TASK
    { Concrete::level() } -> std::convertible_to<bool>;
};

} // namespace estdx
