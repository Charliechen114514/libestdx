#include "stm32f1xx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}

// EXTI0/TIM3/TIM4 向量在 main.cpp 定义。
