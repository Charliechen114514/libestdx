#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>
namespace estdx::gpio {
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
// Firstly, it must BE a pin.
// direction 的比较必须放在 requires{} 外面做合取原子约束:
// 块内写 `Concrete::direction == Output` 只验证表达式合法,不验证值为真,
// 配成 Input 的引脚照样通过约束(2026-09-11 why-cpp 实测踩到)。
concept GPIOOutputPin =
    GPIOPin<Concrete> && Concrete::direction == GpioDirection::Output && requires {
        Concrete::set();    // On
        Concrete::reset();  // Off
        Concrete::toggle(); // Flip
    };

template <typename Concrete>
concept GPIOInputPin =
    GPIOPin<Concrete> && Concrete::direction == GpioDirection::Input && requires {
        // Tells level sync, as aysnc requires wrapper of TASK
        { Concrete::level() } -> std::convertible_to<bool>;
    };

} // namespace estdx::gpio
