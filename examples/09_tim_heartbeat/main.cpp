// 09_tim_heartbeat: TIM2 1Hz 中断心跳, 主循环 WFI 睡眠; 对照 01_blinky 的 HAL_Delay 轮询。
#include <cstdint>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/tick.hpp"
#include "libestdx/boards/stm32f1/tim.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/logger/log.hpp"
#include "libestdx/logger/sinks.hpp"
#include "stm32f1xx_hal.h"

namespace {

using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;

using Log = estdx::logger::Log<estdx::logger::UartLogSink<Serial>, estdx::logger::LogLevel::Info,
                               128, estdx::stm32f1::HalTickClock>;
static_assert(estdx::logger::LogSink<estdx::logger::UartLogSink<Serial>>);
static_assert(estdx::logger::LogClock<estdx::stm32f1::HalTickClock>);

volatile std::uint32_t beats = 0;

struct OnBeat {
    static void operator()() {
        beats = beats + 1;
        Led::toggle();
    }
};
using Heartbeat =
    estdx::stm32f1::TimBase<{.instance = estdx::stm32f1::TimInstance::Tim2, .period_us = 1'000'000},
                            OnBeat>;

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

extern "C" void TIM2_IRQHandler() {
    Heartbeat::irq();
}

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Led::off();
    Serial::init();
    Heartbeat::init(3);

    Log::info(estdx::logger::Tag("boot"), "heartbeat ready");

    std::uint32_t reported = 0;
    for (;;) {
        __WFI();
        if (beats != reported) {
            reported = beats;
            Log::info(estdx::logger::Tag("beat"), reported);
        }
    }
}
