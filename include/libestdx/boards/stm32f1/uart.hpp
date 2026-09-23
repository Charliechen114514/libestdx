#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "libestdx/boards/stm32f1/gpio.hpp"
#include "libestdx/boards/stm32f1/hal/rcc.hpp"
#include "libestdx/boards/stm32f1/hal/uart.hpp"
#include "libestdx/uart/uart_types.hpp"

namespace estdx::stm32f1 {

enum class UartInstance : std::uintptr_t {
    Usart1 = USART1_BASE, // APB2
    Usart2 = USART2_BASE, // APB1
    Usart3 = USART3_BASE, // APB1
};

namespace detail {
// The HAL handle belongs to a physical USART, independent of its configuration.
template <UartInstance INSTANCE>
struct UartState {
    static inline UART_HandleTypeDef handle{};
};
} // namespace detail

template <UartInstance INSTANCE, uart::UartConfig CONFIG = {}>
struct Uart {
    static_assert(INSTANCE == UartInstance::Usart1 || INSTANCE == UartInstance::Usart2 ||
                  INSTANCE == UartInstance::Usart3);
    static_assert(CONFIG.baud != 0, "UART baud must be nonzero");
    static_assert(CONFIG.word_length == uart::WordLength::Bits8 ||
                  CONFIG.word_length == uart::WordLength::Bits9);
    static_assert(CONFIG.stop_bits == uart::StopBits::One ||
                  CONFIG.stop_bits == uart::StopBits::Two);
    static_assert(CONFIG.parity == uart::Parity::None || CONFIG.parity == uart::Parity::Even ||
                  CONFIG.parity == uart::Parity::Odd);
    static_assert(CONFIG.word_length != uart::WordLength::Bits9 ||
                      CONFIG.parity == uart::Parity::None,
                  "F1 cannot send nine data bits plus parity");

    static void init() {
        enable_clock();
        init_pins();

        auto& handle = detail::UartState<INSTANCE>::handle;
        handle.Instance = reinterpret_cast<USART_TypeDef*>(static_cast<std::uintptr_t>(INSTANCE));
        handle.Init.BaudRate = CONFIG.baud;
        handle.Init.WordLength = word_length();
        handle.Init.StopBits = stop_bits();
        handle.Init.Parity = parity();
        handle.Init.Mode = UART_MODE_TX_RX;
        handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        handle.Init.OverSampling = UART_OVERSAMPLING_16;
        check(HAL_UART_Init(&handle));
    }

    static void send(std::span<const std::byte> data)
        requires(CONFIG.word_length == uart::WordLength::Bits8)
    {
        transfer(data, [](auto* handle, const auto* ptr, auto count) {
            return HAL_UART_Transmit(handle, reinterpret_cast<const std::uint8_t*>(ptr), count,
                                     HAL_MAX_DELAY);
        });
    }

    // Best-effort path for callers that must not die on a dead line
    // (logging): bounded timeout instead of HAL_MAX_DELAY — with the UART
    // clock off, HAL_MAX_DELAY would hang forever; a bound turns the hang
    // into a false return. Timeout derives from the compile-time baud:
    // 8N1 = 10 line bits per byte, doubled for margin (HAL polls in 1 ms
    // ticks; a slow-but-alive line must not time out).
    static bool try_send(std::span<const std::byte> data)
        requires(CONFIG.word_length == uart::WordLength::Bits8)
    {
        auto& handle = detail::UartState<INSTANCE>::handle;
        constexpr auto max_chunk = std::numeric_limits<std::uint16_t>::max();
        for (std::size_t offset = 0; offset < data.size();) {
            const std::size_t remaining = data.size() - offset;
            const auto count =
                static_cast<std::uint16_t>(remaining > max_chunk ? max_chunk : remaining);
            const std::uint32_t timeout =
                static_cast<std::uint32_t>(count) * 20000 / CONFIG.baud + 10;
            if (HAL_UART_Transmit(&handle,
                                  reinterpret_cast<const std::uint8_t*>(data.data() + offset),
                                  count, timeout) != HAL_OK) {
                return false;
            }
            offset += count;
        }
        return true;
    }

    static void receive(std::span<std::byte> buffer)
        requires(CONFIG.word_length == uart::WordLength::Bits8)
    {
        transfer(buffer, [](auto* handle, auto* ptr, auto count) {
            return HAL_UART_Receive(handle, reinterpret_cast<std::uint8_t*>(ptr), count,
                                    HAL_MAX_DELAY);
        });
    }

    static void send(std::span<const std::uint16_t> data)
        requires(CONFIG.word_length == uart::WordLength::Bits9)
    {
        transfer(data, [](auto* handle, const auto* ptr, auto count) {
            return HAL_UART_Transmit(handle, reinterpret_cast<const std::uint8_t*>(ptr), count,
                                     HAL_MAX_DELAY);
        });
    }

    static void receive(std::span<std::uint16_t> buffer)
        requires(CONFIG.word_length == uart::WordLength::Bits9)
    {
        transfer(buffer, [](auto* handle, auto* ptr, auto count) {
            return HAL_UART_Receive(handle, reinterpret_cast<std::uint8_t*>(ptr), count,
                                    HAL_MAX_DELAY);
        });
    }

  private:
    struct PinMap {
        GpioPort port;
        std::uint16_t tx;
        std::uint16_t rx;
    };

    static consteval PinMap pin_map() {
        if constexpr (INSTANCE == UartInstance::Usart1) {
            return {GpioPort::A, GPIO_PIN_9, GPIO_PIN_10};
        } else if constexpr (INSTANCE == UartInstance::Usart2) {
            return {GpioPort::A, GPIO_PIN_2, GPIO_PIN_3};
        } else {
            static_assert(INSTANCE == UartInstance::Usart3);
            return {GpioPort::B, GPIO_PIN_10, GPIO_PIN_11};
        }
    }

    static void enable_clock() {
        if constexpr (INSTANCE == UartInstance::Usart1) {
            __HAL_RCC_USART1_CLK_ENABLE();
        } else if constexpr (INSTANCE == UartInstance::Usart2) {
            __HAL_RCC_USART2_CLK_ENABLE();
        } else {
            __HAL_RCC_USART3_CLK_ENABLE();
        }
    }

    static void init_pins() {
        constexpr auto map = pin_map();
        if constexpr (map.port == GpioPort::A) {
            __HAL_RCC_GPIOA_CLK_ENABLE();
        } else {
            __HAL_RCC_GPIOB_CLK_ENABLE();
        }
        auto* port = reinterpret_cast<GPIO_TypeDef*>(static_cast<std::uintptr_t>(map.port));
        GPIO_InitTypeDef def{};
        def.Pin = map.tx;
        def.Mode = GPIO_MODE_AF_PP;
        def.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(port, &def);
        def.Pin = map.rx;
        def.Mode = GPIO_MODE_INPUT;
        def.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(port, &def);
    }

    static consteval std::uint32_t word_length() {
        if constexpr (CONFIG.word_length == uart::WordLength::Bits9 ||
                      CONFIG.parity != uart::Parity::None) {
            return UART_WORDLENGTH_9B;
        } else {
            return UART_WORDLENGTH_8B;
        }
    }

    static consteval std::uint32_t stop_bits() {
        if constexpr (CONFIG.stop_bits == uart::StopBits::Two) {
            return UART_STOPBITS_2;
        } else {
            return UART_STOPBITS_1;
        }
    }

    static consteval std::uint32_t parity() {
        if constexpr (CONFIG.parity == uart::Parity::Even) {
            return UART_PARITY_EVEN;
        } else if constexpr (CONFIG.parity == uart::Parity::Odd) {
            return UART_PARITY_ODD;
        } else {
            return UART_PARITY_NONE;
        }
    }

    template <typename Span, typename Operation>
    static void transfer(Span data, Operation operation) {
        constexpr auto max_chunk = std::numeric_limits<std::uint16_t>::max();
        auto& handle = detail::UartState<INSTANCE>::handle;
        for (std::size_t offset = 0; offset < data.size();) {
            const auto remaining = data.size() - offset;
            const auto count =
                static_cast<std::uint16_t>(remaining > max_chunk ? max_chunk : remaining);
            check(operation(&handle, data.data() + offset, count));
            offset += count;
        }
    }

    static void check(HAL_StatusTypeDef status) {
        if (status != HAL_OK) {
            // Preserve the void contract: returning here would claim a full transfer.
            __builtin_trap();
        }
    }
};

} // namespace estdx::stm32f1
