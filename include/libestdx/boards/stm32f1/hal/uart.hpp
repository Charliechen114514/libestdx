#pragma once
// UART —— 串口收发。

#define HAL_UART_MODULE_ENABLED

// UART HAL 的 C 源文件也编译 DMA 路径;DMA 分片先定义宏并引入 kernel。
#include "libestdx/boards/stm32f1/hal/dma.hpp"

#include "stm32f1xx_hal_uart.h"
