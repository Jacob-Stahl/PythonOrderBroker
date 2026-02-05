#pragma once

#include <ostream>
#include <type_traits>

struct tick {
    using rep = unsigned long;

private:
    rep value_{0};

public:
    constexpr tick() noexcept = default;
    constexpr explicit tick(rep v) noexcept : value_(v) {}
    static constexpr tick from_raw(rep v) noexcept { return tick(v); }
    constexpr rep raw() const noexcept { return value_; }

    // Compound assignments
    constexpr tick& operator+=(tick other) noexcept { value_ += other.value_; return *this; }
    constexpr tick& operator+=(rep other) noexcept { value_ += other; return *this; }
    constexpr tick& operator-=(tick other) noexcept { value_ -= other.value_; return *this; }
    constexpr tick& operator-=(rep other) noexcept { value_ -= other; return *this; }
    constexpr tick& operator*=(rep rhs) noexcept { value_ *= rhs; return *this; }
    constexpr tick& operator/=(rep rhs) noexcept { value_ /= rhs; return *this; }
    constexpr tick& operator%=(rep rhs) noexcept { value_ %= rhs; return *this; }

    // Increment / decrement
    constexpr tick& operator++() noexcept { ++value_; return *this; }
    constexpr tick operator++(int) noexcept { tick tmp = *this; ++value_; return tmp; }
    constexpr tick& operator--() noexcept { --value_; return *this; }
    constexpr tick operator--(int) noexcept { tick tmp = *this; --value_; return tmp; }

    // Unary
    constexpr tick operator+() const noexcept { return *this; }
    constexpr tick operator-() const noexcept { return tick(static_cast<rep>(0 - value_)); }

    // Comparisons
    friend constexpr bool operator==(tick a, tick b) noexcept { return a.value_ == b.value_; }
    friend constexpr bool operator!=(tick a, tick b) noexcept { return a.value_ != b.value_; }
    friend constexpr bool operator<(tick a, tick b) noexcept { return a.value_ < b.value_; }
    friend constexpr bool operator<=(tick a, tick b) noexcept { return a.value_ <= b.value_; }
    friend constexpr bool operator>(tick a, tick b) noexcept { return a.value_ > b.value_; }
    friend constexpr bool operator>=(tick a, tick b) noexcept { return a.value_ >= b.value_; }

    // Arithmetic (ticks <op> ticks)
    friend constexpr tick operator+(tick a, tick b) noexcept { return tick(a.value_ + b.value_); }
    friend constexpr tick operator-(tick a, tick b) noexcept { return tick(a.value_ - b.value_); }
    friend constexpr rep   operator/(tick a, tick b) noexcept { return a.value_ / b.value_; }
    friend constexpr rep   operator%(tick a, tick b) noexcept { return a.value_ % b.value_; }

    // Arithmetic with rep (ticks <op> rep) and (rep <op> ticks)
    friend constexpr tick operator+(tick a, rep b) noexcept { return tick(a.value_ + b); }
    friend constexpr tick operator+(rep a, tick b) noexcept { return tick(a + b.value_); }
    friend constexpr tick operator-(tick a, rep b) noexcept { return tick(a.value_ - b); }
    friend constexpr tick operator-(rep a, tick b) noexcept { return tick(a - b.value_); }
    friend constexpr tick operator*(tick a, rep b) noexcept { return tick(a.value_ * b); }
    friend constexpr tick operator*(rep a, tick b) noexcept { return tick(a * b.value_); }
    friend constexpr tick operator/(tick a, rep b) noexcept { return tick(a.value_ / b); }
    friend constexpr tick operator%(tick a, rep b) noexcept { return tick(a.value_ % b); }

    // Stream
    friend inline std::ostream& operator<<(std::ostream& os, tick t) {
        return os << t.value_;
    }
};

static_assert(std::is_standard_layout<tick>::value, "ticks should have standard layout");
