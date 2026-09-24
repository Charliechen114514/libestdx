/**
 * @file log.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Static Call Surface For The Logger
 * @version 0.1
 * @date 2026-09-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <type_traits>
#include <utility>

#include "libestdx/logger/logger.hpp"

namespace estdx::logger {

// Log::info(...) / Log::warnf(...): the per-level surface as static calls,
// same dialect as the other board types. Engine state stays in Logger.
template <LogSink ASink, LogLevel kMinimumLevel = LogLevel::Info,
          std::size_t kLineBytes = 128, LogClockOrNone AClock = NoClock,
          bool kLocation = false>
struct Log {
    using Engine = Logger<ASink, kMinimumLevel, kLineBytes, AClock, kLocation>;

    template <typename... Parts>
    static void trace(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Trace>(tag, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void debug(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Debug>(tag, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void info(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Info>(tag, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void hint(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Hint>(tag, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void warn(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Warn>(tag, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void error(Tag tag, Parts&&... parts) {
        Engine::Self().template log<LogLevel::Error>(tag, std::forward<Parts>(parts)...);
    }

    template <typename... Parts>
    static void tracef(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                       Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Trace>(tag, fmt, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void debugf(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                       Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Debug>(tag, fmt, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void infof(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                      Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Info>(tag, fmt, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void hintf(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                      Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Hint>(tag, fmt, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void warnf(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                      Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Warn>(tag, fmt, std::forward<Parts>(parts)...);
    }
    template <typename... Parts>
    static void errorf(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt,
                       Parts&&... parts) {
        Engine::Self().template logf<LogLevel::Error>(tag, fmt, std::forward<Parts>(parts)...);
    }

    static void setLevel(LogLevel level) { Engine::Self().setLevel(level); }
    static LogLevel level() { return Engine::Self().level(); }
};

} // namespace estdx::logger
