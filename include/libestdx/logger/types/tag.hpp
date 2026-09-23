/**
 * @file tag.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Tag: A Text Label Carrying Its Call Site
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <concepts>
#include <source_location>
#include <string_view>
#include <utility>

namespace estdx::logger {

// Any argument that already views as text: string literals, const char*,
// string_view, std::string. The const-lvalue form mirrors the consumers'
// actual expressions; shared by Tag's constructor and the Formatter layer.
template <typename IsTypeSource>
concept TextSource = std::convertible_to<const IsTypeSource&, std::string_view>;

// The tag type absorbs the call site through a constrained converting
// constructor: exactly one user-defined conversion (char[N] -> Tag), so
// source_location::current() captures the log call's line. Neither a bare
// string_view parameter (two conversions) nor a defaulted source_location
// parameter ahead of a pack (deduction fails) can do this.
struct Tag {
    // Tags Are Requested for some cases
    std::string_view text;
    std::source_location location;

    template <TextSource S>
    Tag(S&& s, const std::source_location& l = std::source_location::current())
        : text(std::forward<S>(s)), location(l) {}
};

} // namespace estdx::logger
