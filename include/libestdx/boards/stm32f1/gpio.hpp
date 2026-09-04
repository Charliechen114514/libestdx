#pragma once
// GPIO —— 引脚读写/复用配置。

#define HAL_GPIO_MODULE_ENABLED

// kernel 先行:它带着防环引导和 HAL 内核宏,保证本分片单独被引也成立。
#include "libestdx/boards/stm32f1/kernel.hpp"

#include "stm32f1xx_hal_gpio.h"
