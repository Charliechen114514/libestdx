#include "stm32f1xx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}

// TIM2 向量在 main.cpp 定义(Heartbeat::irq 只有 C++ TU 可见)。
