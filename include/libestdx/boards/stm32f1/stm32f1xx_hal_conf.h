#pragma once
// STM32F1 HAL 指名要 include "stm32f1xx_hal_conf.h"(stm32f1xx_hal.h:29 写死),
// 所以文件名保留,内容只是 libestdx 分片配置的拼装入口:
// 引入哪个分片,哪个外设才会被编进固件。当前目标:GPIO,点个灯。

#include "libestdx/boards/stm32f1/kernel.hpp" // HAL 总开关 + 时基,必须
#include "libestdx/boards/stm32f1/clock.hpp"  // 时钟源基准
#include "libestdx/boards/stm32f1/cortex.hpp" // NVIC / SysTick,必须
#include "libestdx/boards/stm32f1/rcc.hpp"    // 时钟树,必须
#include "libestdx/boards/stm32f1/gpio.hpp"   // 就管 GPIO
