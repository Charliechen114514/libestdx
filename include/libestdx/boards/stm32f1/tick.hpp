/**
 * @file tick.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Millisecond Time Source For The Logger Clock Vocabulary
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstdint>

#include "libestdx/boards/stm32f1/hal/kernel.hpp"

namespace estdx::stm32f1 {

// Satisfies logger::LogClock. HAL SysTick at 1 kHz (HAL_Init + the
// firmware's SysTick_Handler). uint32 wraps at ~49.7 days: display-only.
struct HalTickClock {
    static std::uint32_t now_ms() { return HAL_GetTick(); }
};

} // namespace estdx::stm32f1
