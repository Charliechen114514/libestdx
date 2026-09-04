#pragma once

#include "libestdx/boards/stm32f1/hal/clock.hpp"  // 时钟源基准
#include "libestdx/boards/stm32f1/hal/cortex.hpp" // NVIC / SysTick,必须
#include "libestdx/boards/stm32f1/hal/gpio.hpp"   // 就管 GPIO
#include "libestdx/boards/stm32f1/hal/kernel.hpp" // HAL 总开关 + 时基,必须
#include "libestdx/boards/stm32f1/hal/rcc.hpp"    // 时钟树,必须
