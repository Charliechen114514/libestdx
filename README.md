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
cmake --build build --target sim_register_led   # 01_register_led, bare-register PC13
cmake --build build --target sim_gpio_example    # 02_gpio
cmake --build build --target sim_led_example     # 03_led
cmake --build build --target sim_button_example  # 04_button
cmake --build build --target sim_uart_example    # 05_uart, USART1 PTY at /tmp/libestdx-uart
cmake --build build --target check_uart_renode   # UART command console check
cmake --build build --target sim_log_example     # 06_log, USART1 PTY at /tmp/libestdx-log
cmake --build build --target check_log_renode    # log stream check (banner/pruning/timestamps)
cmake --build build --target sim_button_events   # 07_button_events, USART1 PTY at /tmp/libestdx-button
cmake --build build --target check_button_renode # EXTI chain check (3 bounce edges vs 1 debounced press)
cmake --build build --target sim_ring_example    # 08_uart_ring, USART1 PTY at /tmp/libestdx-ring
cmake --build build --target check_ring_renode   # interrupt ring console check (burst under busy loop)
```

目标自带 `--console --disable-xwt`。前四个示例的 `watch` 会直播 GPIO ODR；
按键示例在 monitor 提示符下用 `runMacro $press` 注入一次按键
（`$down` / `$up` 为持续态）。

UART 示例使用 USART1 默认 PA9/PA10、115200 8N1。Renode 的 PTY
`/tmp/libestdx-uart` 会显示 `UART CMD ready` 提示符。命令有 `help`、
`ping`、`led on`、`led off`、`status`、`echo TEXT`；输入支持退格和 CR/LF，
最长 48 个 ASCII 字符。`check_uart_renode` 自动核对命令响应。
同一物理 USART 应只由一份配置初始化；不同配置类型共享该 USART 的 HAL 句柄。

想手动试命令行，在一个终端保持 Renode 运行，再用 PTY 终端连接
`/tmp/libestdx-uart`：

```sh
# 终端 A
cmake --build build --target sim_uart_example
# 终端 B（仅连接 Renode 的虚拟串口）
screen /tmp/libestdx-uart 115200
```

### 日志（06_log）

`libestdx/logger/` 提供零开销日志：拼接式与 `std::format` 风格两种语法、
编译期级别裁剪、无宏的 `source_location` 元数据、可选毫秒时间戳。

```cpp
using Log = estdx::logger::Logger<estdx::logger::UartLogSink<Serial>,
                                  estdx::logger::LogLevel::Info, 128,
                                  estdx::stm32f1::HalTickClock>;

Log::Self().info(Tag("led"), "blink=", n);            // 拼接式
Log::Self().infof(Tag("ledf"), "n={} hex={:x}", n, n); // format 语法({} / {:x} / {:N})
```

行格式 `[123ms][Info ][tag][file:line] payload`（时间戳与位置段按配置裁剪）。
低于 `MinLevel` 的日志**连同字符串字面量一起**不进固件——用
`arm-none-eabi-strings` 对比 `MinLevel=Info/Debug` 的 `.bin` 可实证
（06_log 里埋了验证 marker）。自定义类型特化
`estdx::logger::Formatter<MyType>` 即可入日志，形状同 `std::formatter`。
格式串错误（字段数不符、非法 spec、`{:x}` 用于非整数）在**编译期**被
非 constexpr 信标函数拒绝，错误信息即函数名。
v1 边界：阻塞发送、仅线程上下文（ISR 禁用）、无 float、单 sink。


退出 `screen`：按 `Ctrl-A`、`K`、`Y`；退出 Renode：在 monitor 输入 `quit`。

### 按键事件（07_button_events）

输入线的两幕对拍:EXTI 下降沿中断（`libestdx/boards/stm32f1/exti.hpp`）
把边沿时间戳经 SPSC 队列（`libestdx/base/spsc_queue.hpp`）交给主循环;
采样一致性消抖（`device/debouncer.hpp`）+ 状态机（`base/state_machine.hpp` 概念+框架,`device/button.hpp` 里的 ButtonFsm 实例化,满足 `base::StateMachine` 概念）
产出 `variant<Press, Release, LongPress>` 事件。`check_button_renode` 注入
一串抖动（3 个下降沿）:中断链报 3 行 `edge`,消抖链只认 1 次 `press`,
700ms 长按与松开顺序一并核对。monitor 注入用 `runMacro $press` / `$long` /
`$bounce`;时序注入一律 `pause` → `emulation RunFor`（monitor 的 `sleep`
等的是 host 时间,虚拟时间不成比例推进）。

### 中断接收与环形缓冲（08_uart_ring）

接收线的中断化对照篇（对 05_uart）:RXNE 中断逐字节进 64 字节 SPSC 环
（`boards/stm32f1/uart_irq.hpp`）,主循环非阻塞消化;命令解析的错误路径走
`std::expected`（失败带原因返回）。新增 `sleep MS` 命令模拟主循环被占:
它忙 300ms 期间 burst 到达的命令照单全收、醒来按序响应 ——
`check_ring_renode` 正是核对这个场景。相比 05 的轮询阻塞版,整条异步化
+ expected/variant 解析约 +732B flash / +104B RAM。

### STM32F103C8T6 真机烧录与串口

UART 固件的 ELF 和 BIN 分别是 `build/examples/05_uart/uart_example` 和
`build/examples/05_uart/uart_example.bin`，链接地址从 `0x08000000` 起。
`flash_uart_example` 使用 ST-Link + OpenOCD 烧录 ELF，并执行校验和复位：

```sh
cmake --build build --target flash_uart_example
```

ST-Link 的 SWDIO 接 PA13、SWCLK 接 PA14、GND 共地，目标板须供电；
串口适配器的 RX 接 PA9、TX 接 PA10、GND 共地，使用 3.3 V TTL 电平。
将 BOOT0 置低，让复位后从用户 Flash 启动。BluePill 的原生 USB 口
不会自动变成这个 USART1 串口；需要外接 USB-TTL 适配器，或把带 VCP
的调试器串口实际接到 PA9/PA10。
若 USB-TTL 适配器留在 Windows，直接在 Windows 串口终端打开其 COM 端口，
设置为 115200、8N1；无需把该适配器挂入 WSL。

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
