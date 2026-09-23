/**
 * @file level.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Level Vocabulary
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace estdx::logger {

enum class LogLevel : std::uint8_t {
    Trace, // Working when using Trace Level, that shouldnt be work at release
    Debug, // Debug Level
    Info,  // Inform level for most tracing cases
    Hint,  // Hints level above info, which, expectedly using some cases that needs to be noticed
    Warn,  // Oh, something dangerous!
    Error  // Error Occurs, we need take actions :)
};

consteval inline std::string_view LevelName(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::Trace:
            return "Trace";
        case LogLevel::Debug:
            return "Debug";
        case LogLevel::Info:
            return "Info";
        case LogLevel::Hint:
            return "Hint";
        case LogLevel::Warn:
            return "Warn";
        case LogLevel::Error:
            return "Error";
    }
}

/**
 * @brief C++23, Goods!
 *
 */
constexpr std::strong_ordering operator<=>(LogLevel lhs, LogLevel rhs) {
    return std::to_underlying(lhs) <=> std::to_underlying(rhs);
}

} // namespace estdx::logger
