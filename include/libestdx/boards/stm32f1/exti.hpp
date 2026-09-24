#pragma once
#include <cstdint>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/hal/gpio.hpp"
#include "libestdx/boards/stm32f1/hal/rcc.hpp"
#include "libestdx/gpio/gpio_base.hpp"

namespace estdx::stm32f1 {

enum class ExtiEdge { Rising, Falling, Both };

template <typename H>
concept ExtiHandler = requires {
    H::operator()();
};

namespace detail {
inline std::uint16_t exti_line_claims = 0;
} // namespace detail

template <gpio::GPIOInputPin Pin, ExtiEdge EDGE, ExtiHandler Handler>
struct Exti {
    static constexpr std::uint8_t line = Pin::pin;

    static void init(std::uint32_t preempt) {
        claim_line();

        Pin::init();

        __HAL_RCC_AFIO_CLK_ENABLE();
        map_source();

        if constexpr (EDGE == ExtiEdge::Rising || EDGE == ExtiEdge::Both) {
            EXTI->RTSR |= Pin::mask;
        }
        if constexpr (EDGE == ExtiEdge::Falling || EDGE == ExtiEdge::Both) {
            EXTI->FTSR |= Pin::mask;
        }
        EXTI->IMR |= Pin::mask;
        EXTI->PR = Pin::mask;

        const auto irq = irq_number();
        HAL_NVIC_SetPriority(irq, preempt, 0);
        HAL_NVIC_EnableIRQ(irq);
    }

    static void irq() {
        if (EXTI->PR & Pin::mask) {
            EXTI->PR = Pin::mask;
            Handler::operator()();
        }
    }

  private:
    static void claim_line() {
        const auto bit = static_cast<std::uint16_t>(1u << line);
        if (detail::exti_line_claims & bit) {
            __builtin_trap();
        }
        detail::exti_line_claims |= bit;
    }

    static void map_source() {
        volatile auto* const reg = &AFIO->EXTICR[line / 4];
        constexpr auto field_shift = static_cast<unsigned>((line % 4) * 4);
        constexpr auto field_mask = static_cast<std::uint32_t>(0xFu << field_shift);
        *reg = (*reg & ~field_mask) |
               (static_cast<std::uint32_t>(Pin::port_index) << field_shift);
    }

    static constexpr IRQn_Type irq_number() {
        if constexpr (line <= 4) {
            return static_cast<IRQn_Type>(EXTI0_IRQn + line);
        } else if constexpr (line <= 9) {
            return EXTI9_5_IRQn;
        } else {
            return EXTI15_10_IRQn;
        }
    }
};

} // namespace estdx::stm32f1
