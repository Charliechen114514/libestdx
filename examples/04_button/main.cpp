// 04_button:输入线 —— 按键控制 LED。极性在两端各翻译一次:
// Button 把 PA0 电平译成 is_pressed(),LED 把 PC13 电平译成 on/off,
// 业务代码全程说功能语言,不碰电平。
// 接法:PA0 → 按键 → GND,内部上拉,按下为低(BluePill 无板载键,外接)。

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/device/button.hpp"
#include "libestdx/device/led.hpp"

using LedPin =
    estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13, estdx::GpioDirection::Output>;
using Led = estdx::LED<LedPin, estdx::GpioPolarity::ActiveLow>;

using KeyPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::A, GPIO_PIN_0,
                                    estdx::GpioDirection::Input, estdx::GpioPull::Up>;
using Key = estdx::Button<KeyPin>; // 默认 ActiveLow:按下为低

static_assert(estdx::GPIOInputPin<KeyPin>);

int main() {
    HAL_Init();
    LedPin::init();
    KeyPin::init();

    for (;;) {
        if (Key::is_pressed()) {
            Led::on();
        } else {
            Led::off();
        }
    }
}
