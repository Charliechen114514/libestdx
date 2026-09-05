# EmbededStdExtension: C++ 23 编写的一个嵌入式器件驱动库

> Notes: 这里是跟AELS的[TutorialAwesomeModernCPP](https://github.com/Awesome-Embedded-Learning-Studio/Tutorial_AwesomeModernCPP)一起联动的仓库，因为持续建设而且比较残缺，这里的话就解耦合出去，让需要的人浅克隆，从而减少体积~
> 后续建设基本完善后，将逐渐转移到AELS上，目前先不转正！

## 快速开始

前置:支持 C++23 的 `arm-none-eabi-gcc`、CMake ≥ 3.22、Ninja;
仿真另需 [Renode](https://renode.io/) ≥ 1.16。

```sh
git clone --recursive https://github.com/Charliechen114514/libestdx.git
cd libestdx
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arch/stm32f103c8t6.cmake
cmake --build build
```

固件与 `.bin` 产物在 `build/examples/<name>/` 下,可直接烧录。

### 仿真(可选,无需真机)

```sh
cmake --build build --target sim                 # 01_blinky
cmake --build build --target sim_gpio_example    # 02_gpio
cmake --build build --target sim_led_example     # 03_led
cmake --build build --target sim_button_example  # 04_button
```

目标自带 `--console --disable-xwt`,进入 monitor 后 resc 里的 `watch`
会直播 GPIO ODR,`0x2000` / `0x0000` 交替滚动即灯在闪。按键示例在 monitor
提示符下 `runMacro $press` 注入一次按键(`$down` / `$up` 为持续态)。

## 看起来如何？

`examples/03_led/main.cpp` 全文:

```cpp
#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/device/led.hpp"

using LedPin =
    estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13, estdx::GpioDirection::Output>;
using Led = estdx::LED<LedPin, estdx::GpioPolarity::ActiveLow>;  // BluePill 板载灯低电平亮

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
```

端口、引脚、方向、上下拉、极性全部在类型里;换引脚改一个模板参数,
不碰逻辑。`static_assert(GPIOOutputPin<LedPin>)` 一行即可在编译期证明
"概念 → 家族实现 → 消费者"链路闭合。

## 许可

本仓库代码以 [MIT](LICENSE) 许可发布。`third_party/STM32F1` 遵循 ST
自己的许可条款,保留在 submodule 内,不因本仓库再许可。
