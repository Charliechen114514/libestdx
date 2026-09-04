#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>
namespace estdx {
enum class GpioDirection { Input, Output };

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

} // namespace estdx
