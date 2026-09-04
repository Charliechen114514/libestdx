#pragma once
// 时钟树(RCC) —— GPIO/外设时钟使能全靠它,基本必须引入。

#define HAL_RCC_MODULE_ENABLED

// kernel 先行:它带着防环引导和 HAL 内核宏,保证本分片单独被引也成立。
#include "libestdx/boards/stm32f1/hal/kernel.hpp"

// RCC 调频要同步改 flash 等待周期(HAL_RCC_ClockConfig 里调用
// __HAL_FLASH_GET/SET_LATENCY),需要拉入 flash 头里的宏;
// 但不开 HAL_FLASH_MODULE_ENABLED,flash 擦写的 .c 不会编入。
#include "stm32f1xx_hal_flash.h"

#include "stm32f1xx_hal_rcc.h"
