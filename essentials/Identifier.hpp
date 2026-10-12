#pragma once

#include <compare>
#include <cstdint>
#include <functional>

class Identifier {
public:
    static constexpr int32_t INVALID = -1;

    constexpr Identifier() = default;
    constexpr Identifier(int32_t value) : m_value(value) {}

    constexpr Identifier& operator=(int32_t value) { m_value = value; return *this; }

    // Classic C++ operators
    constexpr Identifier& operator++() { ++m_value; return *this; }
    constexpr Identifier operator++(int) { Identifier old = *this; ++(*this); return old; }

    // Classic C--... Ahm.. Sorry... C++ -- operators
    constexpr Identifier& operator--() { --m_value; return *this; }
    constexpr Identifier operator--(int) { Identifier old = *this; --(*this); return old; }

    // I am sure you also love the C++20 spaceship operator
    constexpr auto operator<=>(const Identifier&) const = default;

    // Returns internal value
    [[nodiscard]] constexpr int32_t getValue() const { return m_value; }

private:
    int32_t m_value = INVALID;
};

template<>
struct std::hash<Identifier> {
    constexpr std::size_t operator()(Identifier identifier) const noexcept {
        return std::hash<int32_t>{}(identifier.getValue());
    }
};

