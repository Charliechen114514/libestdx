#pragma once
#include <cstdint>
#include <optional>
#include <variant>

#include "libestdx/base/state_machine.hpp"
#include "libestdx/gpio/gpio_base.hpp"

namespace estdx::device {

template <gpio::GPIOInputPin Pin, gpio::GpioPolarity POLARITY = gpio::GpioPolarity::ActiveLow>
struct Button {
    static bool is_pressed() {
        if constexpr (POLARITY == gpio::GpioPolarity::ActiveLow) {
            return !Pin::level();
        } else {
            return Pin::level();
        }
    }
};

struct Press {};     // 孩子们我按下了
struct Release {};   // 孩子们我松手了
struct LongPress {}; // 孩子们我在长按
using ButtonEvent = std::variant<Press, Release, LongPress>;

struct Pressed {
    std::uint32_t now_ms;
};
struct Released {};
struct Tick {
    std::uint32_t now_ms;
};

template <std::uint32_t LONG_PRESS_MS>
struct ButtonFsm {
    struct Idle;
    struct Down;
    struct LongDown;

    using Event = std::variant<Pressed, Released, Tick>;
    using Output = ButtonEvent;
    using State = std::variant<Idle, Down, LongDown>;
    using Transition = base::Outcome<Output, State>;

    struct Idle {
        Transition on(const Pressed& e) const { return Transition{Down{e.now_ms}, Press{}}; }
    };

    struct Down {
        std::uint32_t since_ms;
        Transition on(const Tick& t) const {
            if (t.now_ms - since_ms >= LONG_PRESS_MS) {
                return Transition{LongDown{}, LongPress{}};
            }
            return Transition{*this};
        }
        Transition on(const Released&) const { return Transition{Idle{}, Release{}}; }
    };

    struct LongDown {
        Transition on(const Released&) const { return Transition{Idle{}, Release{}}; }
    };

    std::optional<Output> feed(bool pressed, std::uint32_t now_ms) {
        if (pressed != last_stable_) {
            last_stable_ = pressed;
            return pressed ? handle(Event{Pressed{now_ms}}) : handle(Event{Released{}});
        }
        return handle(Event{Tick{now_ms}});
    }

    std::optional<Output> handle(const Event& event) { return machine_.handle(event); }

  private:
    base::VariantStateMachine<Event, Output, Idle, Down, LongDown> machine_;
    bool last_stable_ = false;
};

static_assert(base::StateMachine<ButtonFsm<700>>); // <- 这里塞一下，正常Button是一个经典的状态机

} // namespace estdx::device
