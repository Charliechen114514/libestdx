/**
 * @file  stm32f1xx_it.c
 * @brief 01_blinky 的中断服务程序:哪些 handler 存在由这份固件决定。
 *
 * HAL_Init() 把 SysTick 配成 1ms 一次中断,HAL_Delay/HAL_GetTick
 * 都指着 HAL_IncTick() 喂数,不写这个所有阻塞 API 永远卡死。
 */
#include "stm32f1xx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}
