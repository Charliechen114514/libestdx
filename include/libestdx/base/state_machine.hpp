#pragma once

#include <concepts>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace estdx::base {

// StateMachine Tells how to transfer state when processing an event
// thats the handle do
template <typename IsAStateMachine>
concept StateMachine = std::default_initializable<IsAStateMachine> &&
                       requires(IsAStateMachine m, typename IsAStateMachine::Event e) {
                           typename IsAStateMachine::Event;
                           typename IsAStateMachine::Output;
                           {
                               m.handle(e)
                           } -> std::same_as<std::optional<typename IsAStateMachine::Output>>;
                       };

// HandlesEvent defines the event on
template <typename S, typename E, typename Transition>
concept HandlesEvent = requires(const S& s, const E& e) {
    { s.on(e) } -> std::same_as<Transition>;
};

// Results
namespace detail {
template <typename T>
struct IsVariant : std::false_type {};
template <typename... Ts>
struct IsVariant<std::variant<Ts...>> : std::true_type {};
} // namespace detail

template <typename T>
concept Variant = detail::IsVariant<std::remove_cv_t<T>>::value;

template <typename Output, typename StateVariant>
    requires std::movable<Output> && Variant<StateVariant>
struct Outcome {
    StateVariant next;
    std::optional<Output> output;
};

template <typename EventT, typename OutputT, typename... States>
    requires(sizeof...(States) > 0 && (std::is_move_constructible_v<States> && ...) &&
             Variant<EventT> && std::movable<EventT> && std::movable<OutputT>)
class VariantStateMachine {
  public:
    static_assert(std::is_default_constructible_v<std::tuple_element_t<0, std::tuple<States...>>>,
                  "initial state (first in the pack) must be default constructible");

    using Event = EventT;
    using Output = OutputT;
    using State = std::variant<States...>;
    using Transition = Outcome<Output, State>;

    std::optional<Output> handle(const Event& event) {
        return std::visit(
            [this](const auto& concrete) -> std::optional<Output> { return step(concrete); },
            event);
    }

    template <typename S>
    bool is() const {
        return std::holds_alternative<S>(state_);
    }

    template <typename S>
    const S& get() const {
        return std::get<S>(state_);
    }

  private:
    template <typename E>
    std::optional<Output> step(const E& event) {
        return std::visit(
            [&](auto& current) -> std::optional<Output> {
                using Current = std::remove_reference_t<decltype(current)>;
                if constexpr (HandlesEvent<Current, E, Transition>) {
                    auto outcome = current.on(event);
                    auto output = std::move(outcome.output);
                    state_ = std::move(outcome.next);
                    return output;
                } else {
                    return std::nullopt; // Keep
                }
            },
            state_);
    }

    State state_;
};

} // namespace estdx::base
