// 01_blinky:BluePill 板载 LED(PC13,低电平点亮)。
// 时钟走 HSI 8MHz 默认配置,点灯不需要更快。

#include "stm32f1xx_hal.h"

int main()
{
    HAL_Init();

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef led{};
    led.Pin   = GPIO_PIN_13;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Pull  = GPIO_NOPULL;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &led);

    for (;;) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(500);
    }
}
