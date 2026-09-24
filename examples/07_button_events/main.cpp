// 07_button_events: 消抖轮询链 vs EXTI 边沿中断链, 同一串抖动对拍。PA0 触摸模块(TTP223 类): 空闲低, 触摸高。
#include <cstdint>
#include <type_traits>
#include <variant>

#include "libestdx/base/spsc_queue.hpp"
#include "libestdx/boards/stm32f1/exti.hpp"
#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/tick.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/device/button.hpp"
#include "libestdx/device/debouncer.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/logger/log.hpp"
#include "libestdx/logger/sinks.hpp"
#include "stm32f1xx_hal.h"

namespace {

using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

using KeyPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::A, GPIO_PIN_0,
                                    estdx::gpio::GpioDirection::Input, estdx::gpio::GpioPull::Down>;

using Key = estdx::device::Button<KeyPin, estdx::gpio::GpioPolarity::ActiveHigh>;

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;

using Log = estdx::logger::Log<estdx::logger::UartLogSink<Serial>, estdx::logger::LogLevel::Info,
                               128, estdx::stm32f1::HalTickClock>;
static_assert(estdx::logger::LogSink<estdx::logger::UartLogSink<Serial>>);
static_assert(estdx::logger::LogClock<estdx::stm32f1::HalTickClock>);

using KeyDebouncer = estdx::device::Debouncer<20, 3>;
using KeyFsm = estdx::device::ButtonFsm<700>;
using EdgeQueue = estdx::base::SpscQueue<std::uint32_t, 8>;

static_assert(estdx::base::StateMachine<KeyFsm>);

KeyDebouncer debouncer;
KeyFsm fsm;
EdgeQueue edge_queue;

struct OnTouch {
    static void operator()() { edge_queue.push(HAL_GetTick()); }
};
using KeyEvent = estdx::stm32f1::Exti<KeyPin, estdx::stm32f1::ExtiEdge::Rising,
                                      OnTouch>;

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
    clk.APB2CLKDivider = RCC_SYSCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

} // namespace

extern "C" void EXTI0_IRQHandler() {
    KeyEvent::irq();
}

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Led::off();
    Serial::init();
    KeyEvent::init(2);

    Log::info(estdx::logger::Tag("boot"), "button events ready");

    for (;;) {
        std::uint32_t edge_ms = 0;
        while (edge_queue.pop(edge_ms)) {
            Log::info(estdx::logger::Tag("edge"), edge_ms);
        }

        const std::uint32_t now = HAL_GetTick();
        debouncer.update(Key::is_pressed(), now);
        if (const auto event = fsm.feed(debouncer.stable(), now)) {
            report(*event, now);
        }
    }
}
