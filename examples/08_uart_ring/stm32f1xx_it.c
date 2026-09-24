#include "stm32f1xx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}

// USART1 的向量符号在 main.cpp 定义:IRQHandler 需要 Rx::handle() 的
// 模板实例,纯 C 的本文件拿不到 —— 链接器只认符号名,不挑文件。
