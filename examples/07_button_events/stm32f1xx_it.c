#include "stm32f1xx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}

// EXTI0 向量在 main.cpp 定义(KeyEvent::irq 只有 C++ TU 可见)。
