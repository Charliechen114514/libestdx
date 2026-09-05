#pragma once
#include <bit>     // countr_zero:从掩码派生脚号(EXTI 要的 index)
#include <cstdint> // uintptr_t

#include "libestdx/boards/stm32f1/hal/gpio.hpp" // GPIO_TypeDef + HAL GPIO 调用
#include "libestdx/boards/stm32f1/hal/rcc.hpp"  // __HAL_RCC_GPIOx_CLK_ENABLE
#include "libestdx/gpio/gpio_base.hpp"          // estdx::GpioDirection + 概念

namespace estdx::stm32f1 {

enum class GpioPort : uintptr_t {
    A = GPIOA_BASE,
    B = GPIOB_BASE,
    C = GPIOC_BASE,
    D = GPIOD_BASE,
    E = GPIOE_BASE,
};

template <GpioPort PORT, uint16_t MASK, estdx::GpioDirection DIRECTION,
          GpioPull PULL = GpioPull::NoPull>
struct Gpio {
    static inline GPIO_TypeDef* const port =
        reinterpret_cast<GPIO_TypeDef*>(static_cast<uintptr_t>(PORT));
    static constexpr uint16_t mask = MASK;
    static constexpr uint8_t pin = static_cast<uint8_t>(std::countr_zero(MASK));
    static constexpr auto direction = DIRECTION;

    static void init() {
        enable_clock();
        GPIO_InitTypeDef def{};
        def.Pin = mask;
        def.Mode = direction_mode();
        def.Pull = pull_mode();
        def.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(port, &def);
    }

    static void set() { HAL_GPIO_WritePin(port, mask, GPIO_PIN_SET); }
    static void reset() { HAL_GPIO_WritePin(port, mask, GPIO_PIN_RESET); }
    static void toggle() { HAL_GPIO_TogglePin(port, mask); }
    static bool level() { return HAL_GPIO_ReadPin(port, mask) == GPIO_PIN_SET; }

  private:
    static constexpr uint32_t direction_mode() {
        if constexpr (DIRECTION == estdx::GpioDirection::Output) {
            return GPIO_MODE_OUTPUT_PP;
        } else {
            return GPIO_MODE_INPUT;
        }
    }

    static constexpr uint32_t pull_mode() {
        if constexpr (PULL == GpioPull::Up) {
            return GPIO_PULLUP;
        } else if constexpr (PULL == GpioPull::Down) {
            return GPIO_PULLDOWN;
        } else {
            return GPIO_NOPULL;
        }
    }

    static void enable_clock() {
        if constexpr (PORT == GpioPort::A) {
            __HAL_RCC_GPIOA_CLK_ENABLE();
        } else if constexpr (PORT == GpioPort::B) {
            __HAL_RCC_GPIOB_CLK_ENABLE();
        } else if constexpr (PORT == GpioPort::C) {
            __HAL_RCC_GPIOC_CLK_ENABLE();
        } else if constexpr (PORT == GpioPort::D) {
            __HAL_RCC_GPIOD_CLK_ENABLE();
        } else if constexpr (PORT == GpioPort::E) {
            __HAL_RCC_GPIOE_CLK_ENABLE();
        }
    }
};

} // namespace estdx::stm32f1
