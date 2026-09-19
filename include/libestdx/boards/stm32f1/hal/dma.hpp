#pragma once
// The vendor UART translation unit also compiles its DMA entry points.
#define HAL_DMA_MODULE_ENABLED

#include "libestdx/boards/stm32f1/hal/kernel.hpp"
#include "stm32f1xx_hal_dma.h"
