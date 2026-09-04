// 02_gpio:类型即配置 —— 引脚身份(端口/掩码/方向)编进类型,
// LED 类型不携带任何运行时状态,操作内联到 HAL 调用。
// 板载 LED:PC13,低电平点亮(此处先 set/reset 语义对齐 HAL,不看电路极性)。

#include "libestdx/boards/stm32f1/gpio.hpp"

using Led =
    estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13, estdx::GpioDirection::Output>;

static_assert(estdx::GPIOOutputPin<Led>);

int main() {
    HAL_Init();
    Led::init();

    for (;;) {
        Led::toggle();
        HAL_Delay(500);
    }
}
