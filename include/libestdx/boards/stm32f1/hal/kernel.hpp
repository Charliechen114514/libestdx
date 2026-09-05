#pragma once
// HAL 内核行为。
//
// HAL_MODULE_ENABLED 是 stm32f1xx_hal.c 的总开关:
// HAL_Init / HAL_IncTick / HAL_Delay 全在这个 ifdef 里,必须定义。

#define HAL_MODULE_ENABLED

#define USE_RTOS 0U             /* 不上 RTOS,SysTick 时基归 HAL */
#define TICK_INT_PRIORITY 0x0FU /* SysTick 优先级给最低 */
#define PREFETCH_ENABLE 1U      /* F1 flash 预取 */

// HAL 源码到处调用 assert_param,默认置空;排查参数问题时再换真实现
#define assert_param(expr) ((void)0U)

// ── 防环引导 ────────────────────────────────────────────────────────────────
// 官方头存在环:hal_def → stm32f1xx(:253, USE_HAL_DRIVER)→ hal.h(:29)→
// hal_conf → 分片 → 模块头 → hal_def。从 hal.h 入口(TU 先 include 官方总头,
// 所有 ST 的 .c 和 examples 都如此)时 hal.h 的 guard 先立,环在 :253 自然断开。
// 但从配置这一侧进来(分片被单独 include、clangd 单开分片/配置文件)时,
// hal_def 会被半途拉入:hal.h 在 hal_def 主体之前就声明 HAL_Init,拿不到
// HAL_StatusTypeDef。
// 处理:先假立 hal.h 的 guard 挡掉 :253 的回环,让 hal_def 完整跑完(类型就位),
// 再撤掉假 guard 正式引入 hal.h 补上它自己的声明。从 hal.h 进来时 guard 已立,
// 整段是空操作,零开销。
#ifndef __STM32F1xx_HAL_H
#    define __STM32F1xx_HAL_H /* 假 guard,只为断环 */
#    include "stm32f1xx_hal_def.h"
#    undef __STM32F1xx_HAL_H
#    include "stm32f1xx_hal.h"
#endif
