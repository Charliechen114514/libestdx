#pragma once
#include <cstdint>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/hal/kernel.hpp"
#include "libestdx/boards/stm32f1/hal/rcc.hpp"
#include "libestdx/irq/irq_base.hpp"

namespace estdx::stm32f1 {

enum class TimInstance : std::uintptr_t {
    Tim1 = TIM1_BASE,
    Tim2 = TIM2_BASE,
    Tim3 = TIM3_BASE,
    Tim4 = TIM4_BASE,
};

enum class TimChannel : unsigned { Ch1, Ch2, Ch3, Ch4 };

struct TimConfig {
    TimInstance instance;
    std::uint32_t period_us;
    std::uint32_t timclk_hz = 64'000'000;
};

struct TimPwmConfig {
    TimInstance instance;
    TimChannel channel;
    std::uint32_t freq_hz;
    std::uint32_t timclk_hz = 64'000'000;
};

namespace detail {

struct TimDivisors {
    std::uint32_t psc;
    std::uint32_t arr;
    bool ok;
};

consteval TimDivisors resolve_ticks(std::uint64_t ticks) {
    if (ticks < 1 || ticks > 65'536ull * 65'536ull) {
        return {0, 0, false};
    }
    for (std::uint64_t div = 1; div <= 65'536; ++div) {
        if (ticks % div == 0 && ticks / div <= 65'536) {
            return {static_cast<std::uint32_t>(div - 1),
                    static_cast<std::uint32_t>(ticks / div - 1), true};
        }
    }
    const std::uint64_t div = (ticks + 65'535) / 65'536;
    return {static_cast<std::uint32_t>(div - 1), static_cast<std::uint32_t>(ticks / div - 1), true};
}

consteval std::uint64_t ticks_for_period(std::uint64_t clk_hz, std::uint64_t period_us) {
    return (clk_hz * period_us + 500'000) / 1'000'000;
}

consteval std::uint64_t ticks_for_freq(std::uint64_t clk_hz, std::uint64_t freq_hz) {
    return (clk_hz + freq_hz / 2) / freq_hz;
}

consteval std::uint64_t relative_error_ppm(std::uint64_t target, TimDivisors d) {
    const std::uint64_t actual = (std::uint64_t)(d.psc + 1) * (d.arr + 1);
    return actual > target ? (actual - target) * 1'000'000 / target
                           : (target - actual) * 1'000'000 / target;
}

template <TimInstance INSTANCE>
struct TimHardware {
    static TIM_TypeDef* regs() {
        return reinterpret_cast<TIM_TypeDef*>(static_cast<std::uintptr_t>(INSTANCE));
    }

    static void enable_clock() {
        if constexpr (INSTANCE == TimInstance::Tim1) {
            __HAL_RCC_TIM1_CLK_ENABLE();
        } else if constexpr (INSTANCE == TimInstance::Tim2) {
            __HAL_RCC_TIM2_CLK_ENABLE();
        } else if constexpr (INSTANCE == TimInstance::Tim3) {
            __HAL_RCC_TIM3_CLK_ENABLE();
        } else {
            __HAL_RCC_TIM4_CLK_ENABLE();
        }
    }

    static constexpr IRQn_Type irq_number() {
        if constexpr (INSTANCE == TimInstance::Tim1) {
            return TIM1_UP_IRQn;
        } else if constexpr (INSTANCE == TimInstance::Tim2) {
            return TIM2_IRQn;
        } else if constexpr (INSTANCE == TimInstance::Tim3) {
            return TIM3_IRQn;
        } else {
            return TIM4_IRQn;
        }
    }
};

} // namespace detail

template <TimConfig CFG, irq::IrqHandler Handler>
struct TimBase {
    static constexpr auto kDiv =
        detail::resolve_ticks(detail::ticks_for_period(CFG.timclk_hz, CFG.period_us));
    static_assert(kDiv.ok, "period below resolution or beyond 16-bit PSC*ARR at this clock");
    static_assert(detail::relative_error_ppm(detail::ticks_for_period(CFG.timclk_hz, CFG.period_us),
                                              kDiv) <= 1'000,
                  "period misses 0.1% precision at this clock");

    static void init(std::uint32_t preempt) {
        HW::enable_clock();
        auto* const t = HW::regs();
        t->PSC = kDiv.psc;
        t->ARR = kDiv.arr;
        t->EGR = TIM_EGR_UG;
        t->DIER = TIM_DIER_UIE;
        t->SR = ~TIM_SR_UIF;
        t->CR1 = TIM_CR1_CEN;
        NVIC_SetPriority(HW::irq_number(), preempt);
        NVIC_EnableIRQ(HW::irq_number());
    }

    static void irq() {
        auto* const t = HW::regs();
        if (t->SR & TIM_SR_UIF) {
            t->SR = ~TIM_SR_UIF;
            Handler::operator()();
        }
    }

  private:
    using HW = detail::TimHardware<CFG.instance>;
};

template <TimConfig CFG, irq::IrqHandler Handler>
struct TimOneShot {
    static constexpr auto kDiv =
        detail::resolve_ticks(detail::ticks_for_period(CFG.timclk_hz, CFG.period_us));
    static_assert(kDiv.ok, "timeout below resolution or beyond 16-bit PSC*ARR at this clock");
    static_assert(detail::relative_error_ppm(detail::ticks_for_period(CFG.timclk_hz, CFG.period_us),
                                              kDiv) <= 1'000,
                  "timeout misses 0.1% precision at this clock");

    static void init(std::uint32_t preempt) {
        HW::enable_clock();
        auto* const t = HW::regs();
        t->CR1 = 0;
        t->PSC = kDiv.psc;
        t->ARR = kDiv.arr;
        t->EGR = TIM_EGR_UG;
        t->DIER = TIM_DIER_UIE;
        t->SR = ~TIM_SR_UIF;
        NVIC_SetPriority(HW::irq_number(), preempt);
        NVIC_EnableIRQ(HW::irq_number());
    }

    static void arm() {
        auto* const t = HW::regs();
        t->CR1 |= TIM_CR1_OPM;
        t->EGR = TIM_EGR_UG;
        t->SR = ~TIM_SR_UIF;
        t->CR1 |= TIM_CR1_CEN;
    }

    static void disarm() { HW::regs()->CR1 &= ~(TIM_CR1_OPM | TIM_CR1_CEN); }

    static void irq() {
        auto* const t = HW::regs();
        if (t->SR & TIM_SR_UIF) {
            t->SR = ~TIM_SR_UIF;
            Handler::operator()();
        }
    }

  private:
    using HW = detail::TimHardware<CFG.instance>;
};

template <TimPwmConfig CFG>
struct TimPwm {
    static constexpr auto kDiv =
        detail::resolve_ticks(detail::ticks_for_freq(CFG.timclk_hz, CFG.freq_hz));
    static_assert(kDiv.ok, "frequency beyond 16-bit PSC*ARR range at this clock");
    static_assert(detail::relative_error_ppm(detail::ticks_for_freq(CFG.timclk_hz, CFG.freq_hz),
                                              kDiv) <= 1'000,
                  "frequency misses 0.1% precision at this clock");

    static void init() {
        HW::enable_clock();
        init_pin();
        auto* const t = HW::regs();
        t->PSC = kDiv.psc;
        t->ARR = kDiv.arr;
        if constexpr (CFG.channel == TimChannel::Ch1) {
            t->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;
            t->CCER = TIM_CCER_CC1E;
            t->CCR1 = 0;
        } else if constexpr (CFG.channel == TimChannel::Ch2) {
            t->CCMR1 = TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;
            t->CCER = TIM_CCER_CC2E;
            t->CCR2 = 0;
        } else if constexpr (CFG.channel == TimChannel::Ch3) {
            t->CCMR2 = TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;
            t->CCER = TIM_CCER_CC3E;
            t->CCR3 = 0;
        } else {
            t->CCMR2 = TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1;
            t->CCER = TIM_CCER_CC4E;
            t->CCR4 = 0;
        }
        t->EGR = TIM_EGR_UG;
        t->CR1 = TIM_CR1_CEN;
    }

    static void set_duty(std::uint8_t percent) {
        const std::uint32_t load = percent > 100 ? 100 : percent;
        ccr() = static_cast<std::uint32_t>(kDiv.arr + 1) * load / 100;
    }

  private:
    using HW = detail::TimHardware<CFG.instance>;

    static volatile std::uint32_t& ccr() {
        auto* const t = HW::regs();
        if constexpr (CFG.channel == TimChannel::Ch1) {
            return t->CCR1;
        } else if constexpr (CFG.channel == TimChannel::Ch2) {
            return t->CCR2;
        } else if constexpr (CFG.channel == TimChannel::Ch3) {
            return t->CCR3;
        } else {
            return t->CCR4;
        }
    }

    struct PinMap {
        GpioPort port;
        std::uint16_t pin;
    };

    static consteval PinMap pin_map() {
        if constexpr (CFG.instance == TimInstance::Tim2) {
            if constexpr (CFG.channel == TimChannel::Ch1) {
                return {GpioPort::A, GPIO_PIN_0};
            } else if constexpr (CFG.channel == TimChannel::Ch2) {
                return {GpioPort::A, GPIO_PIN_1};
            } else if constexpr (CFG.channel == TimChannel::Ch3) {
                return {GpioPort::A, GPIO_PIN_2};
            } else {
                return {GpioPort::A, GPIO_PIN_3};
            }
        } else if constexpr (CFG.instance == TimInstance::Tim3) {
            if constexpr (CFG.channel == TimChannel::Ch1) {
                return {GpioPort::A, GPIO_PIN_6};
            } else if constexpr (CFG.channel == TimChannel::Ch2) {
                return {GpioPort::A, GPIO_PIN_7};
            } else if constexpr (CFG.channel == TimChannel::Ch3) {
                return {GpioPort::B, GPIO_PIN_0};
            } else {
                return {GpioPort::B, GPIO_PIN_1};
            }
        } else if constexpr (CFG.instance == TimInstance::Tim4) {
            if constexpr (CFG.channel == TimChannel::Ch1) {
                return {GpioPort::B, GPIO_PIN_6};
            } else if constexpr (CFG.channel == TimChannel::Ch2) {
                return {GpioPort::B, GPIO_PIN_7};
            } else if constexpr (CFG.channel == TimChannel::Ch3) {
                return {GpioPort::B, GPIO_PIN_8};
            } else {
                return {GpioPort::B, GPIO_PIN_9};
            }
        } else {
            if constexpr (CFG.channel == TimChannel::Ch1) {
                return {GpioPort::A, GPIO_PIN_8};
            } else if constexpr (CFG.channel == TimChannel::Ch2) {
                return {GpioPort::A, GPIO_PIN_9};
            } else if constexpr (CFG.channel == TimChannel::Ch3) {
                return {GpioPort::A, GPIO_PIN_10};
            } else {
                return {GpioPort::A, GPIO_PIN_11};
            }
        }
    }

    static void init_pin() {
        constexpr auto map = pin_map();
        if constexpr (map.port == GpioPort::A) {
            __HAL_RCC_GPIOA_CLK_ENABLE();
        } else {
            __HAL_RCC_GPIOB_CLK_ENABLE();
        }
        auto* port = reinterpret_cast<GPIO_TypeDef*>(static_cast<std::uintptr_t>(map.port));
        GPIO_InitTypeDef def{};
        def.Pin = map.pin;
        def.Mode = GPIO_MODE_AF_PP;
        def.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(port, &def);
    }
};

} // namespace estdx::stm32f1
