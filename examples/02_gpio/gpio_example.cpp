// 02_gpio:类型即配置 —— 引脚身份(端口/掩码/方向)编进类型,
// LED 类型不携带任何运行时状态,操作内联到 HAL 调用。
// 板载 LED:PC13,低电平点亮(此处先 set/reset 语义对齐 HAL,不看电路极性)。

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "stm32f1xx_hal.h" // SystemClock_Config 直接用 RCC API,显式引入

using Led =
    estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13, estdx::GpioDirection::Output>;

static_assert(estdx::GPIOOutputPin<Led>);

// 真机时钟配置(抄 TAMCPP):HSI 8M ÷2 ×16 = 64M PLL,APB1 ÷2,flash 延迟 2。
static void SystemClock_Config() {
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2; // 8M/2 = 4M 进 PLL
    osc.PLL.PLLMUL = RCC_PLL_MUL16;             // 4M × 16 = 64M
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk{};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1; // HCLK = 64M
    clk.APB1CLKDivider = RCC_HCLK_DIV2;  // APB1 = 32M
    clk.APB2CLKDivider = RCC_HCLK_DIV1;  // APB2 = 64M
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

int main() {
    HAL_Init();
    SystemClock_Config();
    Led::init();

    for (;;) {
        Led::toggle();
        HAL_Delay(500);
    }
}
