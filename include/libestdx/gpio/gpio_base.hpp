#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>
namespace estdx {
enum class GpioDirection { Input, Output };

// 极性:器件"激活"电平的高低,是板级电路事实(源电流/灌电流接法决定)。
// 电平语言的词,和 GpioDirection 同族;LED/Button/继电器/CS 等组件共享。
enum class GpioPolarity { ActiveHigh, ActiveLow };

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
