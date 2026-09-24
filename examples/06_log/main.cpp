// USART1 PA9/PA10 one-way log stream: both syntaxes (concatenation and
// format strings), compile-time pruning, timestamps, and a debug line that
// must vanish from the firmware (verify with the map file).
#include <cstdint>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/tick.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/logger/log.hpp"
#include "libestdx/logger/sinks.hpp"
#include "stm32f1xx_hal.h"

namespace {

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;
using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

// MinLevel=Info: the debug line below is not even instantiated, and its
// literal must not survive into the firmware.
using Log = estdx::logger::Log<estdx::logger::UartLogSink<Serial>, estdx::logger::LogLevel::Info,
                               128, estdx::stm32f1::HalTickClock>;

static_assert(estdx::logger::LogSink<estdx::logger::UartLogSink<Serial>>);
static_assert(estdx::logger::LogClock<estdx::stm32f1::HalTickClock>);

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

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Led::off();
    Serial::init();

    Log::error(estdx::logger::Tag("boot"), "libestdx 07_log example");
    Log::warn(estdx::logger::Tag("boot"), "MinLevel=Info, clock=HalTickClock");
    Log::info(estdx::logger::Tag("boot"), "watch PTY /tmp/libestdx-log");
    Log::debug(estdx::logger::Tag("boot"), "debug-marker-must-not-survive-MinLevel-Info");

    for (std::uint32_t n = 1;; ++n) {
        Led::toggle();
        HAL_Delay(500);
        Log::info(estdx::logger::Tag("led"), "blink=", n);
        Log::infof(estdx::logger::Tag("ledf"), "n={} hex={:x}", n, n);
    }
}
