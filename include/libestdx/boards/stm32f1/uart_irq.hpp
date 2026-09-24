#pragma once
#include <cstddef>
#include <cstdint>

#include "libestdx/base/spsc_queue.hpp"
#include "libestdx/boards/stm32f1/hal/uart.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"

namespace estdx::stm32f1 {

namespace detail {

template <UartInstance INSTANCE, std::size_t N>
struct UartRxEngine {
    static_assert(N > 0 && (N & (N - 1)) == 0, "ring capacity must be a power of two");

    static UartRxEngine& Self() noexcept {
        static UartRxEngine AEngine;
        return AEngine;
    }

    void start(std::uint32_t preempt) {
        HAL_NVIC_SetPriority(irq_number(), preempt, 0);
        HAL_NVIC_EnableIRQ(irq_number());
        usart()->CR1 |= USART_CR1_RXNEIE;
    }

    bool try_pop(std::byte& out) { return ring.pop(out); }

    void irq() {
        auto* const regs = usart();
        const auto sr = regs->SR;
        if (sr & USART_SR_RXNE) {
            const auto data = static_cast<std::byte>(regs->DR);
            if ((sr & (USART_SR_FE | USART_SR_NE)) == 0) {
                (void)ring.push(data);
            }
        } else if (sr & USART_SR_ORE) {
            (void)regs->DR;
        }
    }

  private:
    estdx::base::SpscQueue<std::byte, N> ring;

    static USART_TypeDef* usart() {
        return reinterpret_cast<USART_TypeDef*>(static_cast<std::uintptr_t>(INSTANCE));
    }

    static constexpr IRQn_Type irq_number() {
        if constexpr (INSTANCE == UartInstance::Usart1) {
            return USART1_IRQn;
        } else if constexpr (INSTANCE == UartInstance::Usart2) {
            return USART2_IRQn;
        } else {
            return USART3_IRQn;
        }
    }
};

} // namespace detail

template <UartInstance INSTANCE, std::size_t N>
struct UartRx {
    using Engine = detail::UartRxEngine<INSTANCE, N>;

    static void start(std::uint32_t preempt) { Engine::Self().start(preempt); }
    static bool try_pop(std::byte& out) { return Engine::Self().try_pop(out); }
    static void irq() { Engine::Self().irq(); }
};

} // namespace estdx::stm32f1
