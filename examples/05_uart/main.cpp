// USART1 PA9/PA10: a small, allocation-free command console.
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/uart.hpp"
#include "libestdx/device/led.hpp"
#include "libestdx/uart/uart_base.hpp"
#include "stm32f1xx_hal.h"

using Serial = estdx::stm32f1::Uart<estdx::stm32f1::UartInstance::Usart1>;
using LedPin = estdx::stm32f1::Gpio<estdx::stm32f1::GpioPort::C, GPIO_PIN_13,
                                    estdx::gpio::GpioDirection::Output>;
using Led = estdx::device::LED<LedPin, estdx::gpio::GpioPolarity::ActiveLow>;

static_assert(estdx::uart::UartWriter<Serial> && estdx::uart::UartReader<Serial>);

static void write(std::string_view text) {
    Serial::send(std::as_bytes(std::span<const char>{text.data(), text.size()}));
}

static void run_command(std::string_view line, bool& led_on) {
    if (line == "help") {
        write("Commands: help, ping, led on, led off, status, echo TEXT\r\n");
    } else if (line == "ping") {
        write("pong\r\n");
    } else if (line == "led on") {
        Led::on();
        led_on = true;
        write("LED on\r\n");
    } else if (line == "led off") {
        Led::off();
        led_on = false;
        write("LED off\r\n");
    } else if (line == "status") {
        write(led_on ? "LED: on\r\n" : "LED: off\r\n");
    } else if (line.starts_with("echo ")) {
        write(line.substr(5));
        write("\r\n");
    } else if (!line.empty()) {
        write("Unknown command. Type help.\r\n");
    }
}

static void SystemClock_Config() {
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
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

int main() {
    HAL_Init();
    SystemClock_Config();
    LedPin::init();
    Led::off();
    Serial::init();

    write("UART CMD ready\r\nType help for commands.\r\n> ");

    std::array<char, 48> line{};
    std::array<std::byte, 1> input{};
    std::size_t length = 0;
    bool led_on = false;
    bool overflow = false;
    bool after_cr = false;

    for (;;) {
        Serial::receive(input);
        const auto ch = std::to_integer<unsigned char>(input[0]);

        if (ch == '\n' && after_cr) {
            after_cr = false;
            continue;
        }
        after_cr = ch == '\r';

        if (ch == '\r' || ch == '\n') {
            write("\r\n");
            if (overflow) {
                write("Line too long.\r\n");
            } else {
                run_command({line.data(), length}, led_on);
            }
            length = 0;
            overflow = false;
            write("> ");
        } else if (ch == '\b' || ch == 0x7f) {
            if (length > 0 && !overflow) {
                --length;
                write("\b \b");
            }
        } else if (ch >= 0x20 && ch <= 0x7e) {
            if (!overflow && length < line.size()) {
                line[length++] = static_cast<char>(ch);
                Serial::send(input);
            } else {
                overflow = true;
            }
        }
    }
}
