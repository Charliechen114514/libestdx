
#pragma once
#include <cstdint>

namespace estdx::uart {

// Payload bits, excluding the parity bit. F1 uses a nine-bit HAL frame for 8E1/8O1.
enum class WordLength { Bits8, Bits9 };
enum class StopBits { One, Two };
enum class Parity { None, Even, Odd };

struct UartConfig {
    std::uint32_t baud = 115200;
    WordLength word_length = WordLength::Bits8;
    StopBits stop_bits = StopBits::One;
    Parity parity = Parity::None;
};

} // namespace estdx::uart
