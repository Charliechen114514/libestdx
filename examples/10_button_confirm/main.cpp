// 10_button_confirm: 确认式消抖(完整版) —— EXTI 沿 arm 20ms 单发确认窗, 到点采样定稳定态;
// 确认事件 + TIM 慢拍同喂 ButtonFsm(换源不换机), 主循环全程 WFI。
// 对拍 07: 消抖不再靠轮询耐心, 靠时间确认; 抖动 = 窗顺延, 毛刺 = 无事件。
#include <cstdint>
#include <type_traits>
#include <variant>

#include "libestdx/base/spsc_queue.hpp"
#include "libestdx/boards/stm32f1/exti.hpp"
#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/tick.hpp"
#include "libestdx/boards/stm32f1/tim.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/device/button.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/logger/log.hpp"
#include "libestdx/logger/sinks.hpp"
#include "stm32f1xx_hal.h"

namespace {

using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

using KeyPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::A, GPIO_PIN_0,
                                    estdx::gpio::GpioDirection::Input,
                                    estdx::gpio::GpioPull::Down>;

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;

using Log = estdx::logger::Log<estdx::logger::UartLogSink<Serial>, estdx::logger::LogLevel::Info,
                               128, estdx::stm32f1::HalTickClock>;
static_assert(estdx::logger::LogSink<estdx::logger::UartLogSink<Serial>>);
static_assert(estdx::logger::LogClock<estdx::stm32f1::HalTickClock>);

using KeyFsm = estdx::device::ButtonFsm<700>;

struct Confirmed {
    bool pressed;
    std::uint32_t now_ms;
};
struct Tick {
    std::uint32_t now_ms;
};
using SourceEvent = std::variant<Confirmed, Tick>;
using SourceQueue = estdx::base::SpscQueue<SourceEvent, 16>;
SourceQueue source_queue;

// confirm ISR 私有: 上次确认的稳定态
bool confirmed_level = false;

struct OnConfirm {
    static void operator()() {
        const bool pressed = KeyPin::level();
        if (pressed != confirmed_level) {
            confirmed_level = pressed;
            (void)source_queue.push(Confirmed{pressed, HAL_GetTick()});
        }
    }
};
using Window = estdx::stm32f1::TimOneShot<
    {.instance = estdx::stm32f1::TimInstance::Tim3, .period_us = 20'000}, OnConfirm>;

struct OnEdge {
    static void operator()() { Window::arm(); }
};
using Touch = estdx::stm32f1::Exti<KeyPin, estdx::stm32f1::ExtiEdge::Both, OnEdge>;

struct OnTick {
    static void operator()() { (void)source_queue.push(Tick{HAL_GetTick()}); }
};
using SlowTick =
    estdx::stm32f1::TimBase<{.instance = estdx::stm32f1::TimInstance::Tim4, .period_us = 10'000},
                            OnTick>;

KeyFsm fsm;
bool stable_level = false;

void report(const estdx::device::ButtonEvent& event, std::uint32_t now_ms) {
    std::visit(
        [&](const auto& e) {
            using E = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<E, estdx::device::Press>) {
                Log::info(estdx::logger::Tag("press"), now_ms);
                Led::on();
            } else if constexpr (std::is_same_v<E, estdx::device::LongPress>) {
                Log::info(estdx::logger::Tag("long"), now_ms);
            } else {
                Log::info(estdx::logger::Tag("release"), now_ms);
                Led::off();
            }
        },
        event);
}

void SystemClock_Config() {
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    osc.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk{};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

} // namespace

extern "C" void EXTI0_IRQHandler() {
    Touch::irq();
}

extern "C" void TIM3_IRQHandler() {
    Window::irq();
}

extern "C" void TIM4_IRQHandler() {
    SlowTick::irq();
}

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Led::off();
    Serial::init();
    Touch::init(2);
    Window::init(3);
    SlowTick::init(4);

    Log::info(estdx::logger::Tag("boot"), "button confirm ready");

    for (;;) {
        __WFI();
        SourceEvent event;
        while (source_queue.pop(event)) {
            std::visit(
                [&](const auto& ev) {
                    using E = std::decay_t<decltype(ev)>;
                    if constexpr (std::is_same_v<E, Confirmed>) {
                        stable_level = ev.pressed;
                        Log::info(estdx::logger::Tag(ev.pressed ? "touch" : "leave"), ev.now_ms);
                        if (const auto out = fsm.feed(ev.pressed, ev.now_ms)) {
                            report(*out, ev.now_ms);
                        }
                    } else {
                        if (const auto out = fsm.feed(stable_level, ev.now_ms)) {
                            report(*out, ev.now_ms);
                        }
                    }
                },
                event);
        }
    }
}
