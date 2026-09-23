/**
 * @file sinks.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Sink Adapters
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

// Sink adapters: the logger's text vocabulary (char lines) meets the board
// vocabulary (byte UART) and the host's stdio. The LogSink contract is
// best-effort — swallow errors, never trap, never block unboundedly.

#include <cstddef>
#include <cstdio>
#include <span>

#include "libestdx/logger/types/core.hpp"
#include "libestdx/uart/uart_base.hpp"

namespace estdx::logger {

// Adapts an 8-bit UART. try_send is the best-effort path (bounded timeout,
// never traps): logging must not kill the device. The 9-bit configuration
// has no UartWriter overload — logs are an ASCII stream.
template <typename AUart>
    requires uart::UartWriter<AUart> &&
             requires(std::span<const std::byte> data) { AUart::try_send(data); }
struct UartLogSink {
    static void write(std::span<const char> line) {
        (void)AUart::try_send(std::as_bytes(line));
    }
};

// Host smoke tests and tutorials; firmware does not link it.
struct StdioSink {
    static void write(std::span<const char> line) {
        std::fwrite(line.data(), 1, line.size(), stdout);
        std::fflush(stdout);
    }
};

} // namespace estdx::logger
