#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/device/led.hpp"

using LedPin =
    estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13, estdx::GpioDirection::Output>;
using Led = estdx::LED<LedPin, estdx::GpioPolarity::ActiveLow>;

int main() {
    HAL_Init();
    LedPin::init();

    for (;;) {
        Led::on();
        HAL_Delay(500);
        Led::off();
        HAL_Delay(500);
    }
}
