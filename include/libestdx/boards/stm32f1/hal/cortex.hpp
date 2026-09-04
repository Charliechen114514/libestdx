#pragma once
// NVIC / SysTick —— HAL 内核依赖,基本必须引入。

#define HAL_CORTEX_MODULE_ENABLED

// kernel 先行:它带着防环引导和 HAL 内核宏,保证本分片单独被引也成立。
#include "libestdx/boards/stm32f1/hal/kernel.hpp"

#include "stm32f1xx_hal_cortex.h"
