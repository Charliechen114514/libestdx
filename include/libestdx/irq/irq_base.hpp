#pragma once

namespace estdx::irq {

template <typename H>
concept IrqHandler = requires {
    H::operator()();
};

} // namespace estdx::irq
