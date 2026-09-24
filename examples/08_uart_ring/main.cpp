// 08_uart_ring: RXNE 中断逐字节进 SPSC 环, 主循环非阻塞消化; 05_uart 的中断化对照篇。
// 命令: help, ping, led on, led off, status, echo TEXT, sleep MS
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/tick.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/boards/stm32f1/uart_irq.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/logger/log.hpp"
#include "libestdx/logger/sinks.hpp"
#include "stm32f1xx_hal.h"

namespace {

using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;
using Rx = estdx::stm32f1::UartRx<estdx::stm32f1::UartInstance::Usart1, 64>;

using Log = estdx::logger::Log<estdx::logger::UartLogSink<Serial>, estdx::logger::LogLevel::Info,
                               128, estdx::stm32f1::HalTickClock>;
static_assert(estdx::logger::LogSink<estdx::logger::UartLogSink<Serial>>);
static_assert(estdx::logger::LogClock<estdx::stm32f1::HalTickClock>);

// 终端机械动作(回显/提示符)走裸字节, 信息行一律走 Log
void write_raw(std::string_view text) {
    Serial::send(std::as_bytes(std::span<const char>{text.data(), text.size()}));
}

enum class ParseError { Unknown, MissingArg, BadNumber };

// 错误带证据: 哪个词不认识/缺的参数名
struct CommandError {
    ParseError code;
    std::string_view token;
};

struct Help {};
struct Ping {};
struct LedOn {};
struct LedOff {};
struct Status {};
struct Echo {
    std::string_view text;
};
struct Sleep {
    std::uint32_t ms;
};
using Command = std::variant<Help, Ping, LedOn, LedOff, Status, Echo, Sleep>;

std::expected<Command, CommandError> parse_line(std::string_view line) {
    if (line == "help") {
        return Help{};
    }
    if (line == "ping") {
        return Ping{};
    }
    if (line == "led on") {
        return LedOn{};
    }
    if (line == "led off") {
        return LedOff{};
    }
    if (line == "status") {
        return Status{};
    }
    if (line.starts_with("echo ")) {
        return Echo{line.substr(5)};
    }
    if (line.starts_with("sleep ")) {
        const auto arg = line.substr(6);
        std::uint32_t ms = 0;
        const auto result = std::from_chars(arg.data(), arg.data() + arg.size(), ms);
        if (arg.empty() || result.ec != std::errc{} || result.ptr != arg.data() + arg.size()) {
            return std::unexpected(CommandError{ParseError::BadNumber, arg});
        }
        return Sleep{ms};
    }
    return std::unexpected(CommandError{ParseError::Unknown, line});
}

bool led_on = false;

void execute(const Command& command) {
    std::visit(
        [&](const auto& cmd) {
            using C = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<C, Help>) {
                Log::info(estdx::logger::Tag("help"),
                          "help, ping, led on, led off, status, echo TEXT, sleep MS");
            } else if constexpr (std::is_same_v<C, Ping>) {
                Log::info(estdx::logger::Tag("ping"), "pong");
            } else if constexpr (std::is_same_v<C, LedOn>) {
                Led::on();
                led_on = true;
                Log::info(estdx::logger::Tag("led"), "on");
            } else if constexpr (std::is_same_v<C, LedOff>) {
                Led::off();
                led_on = false;
                Log::info(estdx::logger::Tag("led"), "off");
            } else if constexpr (std::is_same_v<C, Status>) {
                Log::info(estdx::logger::Tag("led"), led_on ? "LED: on" : "LED: off");
            } else if constexpr (std::is_same_v<C, Echo>) {
                Log::info(estdx::logger::Tag("echo"), cmd.text);
            } else {
                Log::info(estdx::logger::Tag("sleep"), "busy ", cmd.ms);
                HAL_Delay(cmd.ms);
                Log::info(estdx::logger::Tag("sleep"), "done");
            }
        },
        command);
}

void SystemClock_Config() {
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    osc.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk{};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_SYSCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

} // namespace

extern "C" void USART1_IRQHandler() {
    Rx::irq();
}

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Serial::init();
    Rx::start(3);

    Log::info(estdx::logger::Tag("boot"), "ring console ready");
    Log::info(estdx::logger::Tag("boot"), "type help for commands");
    write_raw("> ");

    std::array<char, 48> line{};
    std::size_t length = 0;
    bool overflow = false;
    bool after_cr = false;

    for (;;) {
        std::byte byte = {};
        while (Rx::try_pop(byte)) {
            const auto ch = std::to_integer<unsigned char>(byte);

            if (ch == '\n' && after_cr) {
                after_cr = false;
                continue;
            }
            after_cr = ch == '\r';

            if (ch == '\r' || ch == '\n') {
                write_raw("\r\n");
                if (overflow) {
                    Log::warn(estdx::logger::Tag("cmd"), "line too long");
                } else if (length > 0) {
                    const auto parsed = parse_line({line.data(), length});
                    if (parsed.has_value()) {
                        execute(*parsed);
                    } else {
                        const auto& error = parsed.error();
                        switch (error.code) {
                        case ParseError::Unknown:
                            Log::warn(estdx::logger::Tag("cmd"), "unknown command: ",
                                      error.token);
                            break;
                        case ParseError::MissingArg:
                            Log::warn(estdx::logger::Tag("cmd"), "missing argument: ",
                                      error.token);
                            break;
                        case ParseError::BadNumber:
                            Log::warn(estdx::logger::Tag("cmd"), "not a number: ", error.token);
                            break;
                        }
                    }
                }
                length = 0;
                overflow = false;
                write_raw("> ");
            } else if (ch == '\b' || ch == 0x7f) {
                if (length > 0 && !overflow) {
                    --length;
                    write_raw("\b \b");
                }
            } else if (ch >= 0x20 && ch <= 0x7e) {
                if (!overflow && length < line.size()) {
                    line[length++] = static_cast<char>(ch);
                    Serial::send(std::span<const std::byte, 1>{&byte, 1});
                } else {
                    overflow = true;
                }
            }
        }
    }
}
