#pragma once

#include <tephra/tools/array.hpp>
#include <tephra/macros.hpp>

namespace tp {

/// A strongly typed mask for a set of enum flags. Requires that the underlying enum values are powers of two.
template <typename Enum>
class EnumBitMask {
public:
    using EnumType = Enum;
    using EnumValueType = typename std::underlying_type<Enum>::type;

    /// The raw value of the bit mask.
    EnumValueType value;

    EnumBitMask() : EnumBitMask(Enum(0)) {}

    constexpr EnumBitMask(Enum value) : EnumBitMask(static_cast<EnumValueType>(value)) {}

    explicit constexpr EnumBitMask(EnumValueType value) : value(value) {
        static_assert(std::is_enum<Enum>::value, "Template parameter is not an enum");
    }

    explicit constexpr operator Enum() const {
        return static_cast<Enum>(value);
    }

    explicit constexpr operator EnumValueType() const {
        return value;
    }

    /// Returns `true` if this bit mask contains the given flag.
    constexpr bool contains(Enum flag) const {
        return (value & static_cast<EnumValueType>(flag)) != 0;
    }

    /// Returns `true` if this bit mask contains *any* flag.
    constexpr bool containsAny() const {
        return value != 0;
    }

    /// Returns `true` if this bit mask contains *any* of the flags of the other bit mask.
    /// Meaning the intersection of the sets of flags the bit masks represent is not empty.
    constexpr bool containsAny(EnumBitMask<Enum> other) const {
        return (value & other.value) != 0;
    }

    /// Returns `true` if this bit mask contains *all* of the flags of the other bit mask.
    /// Meaning the set of flags represented by this bit mask is a superset of the other.
    constexpr bool containsAll(EnumBitMask<Enum> other) const {
        return (value & other.value) == other.value;
    }

    /// Returns the number of flags set in the bit mask.
    uint32_t countFlagsSet() const {
        // Brian Kernighan's bit count
        EnumValueType v = value;
        uint32_t c;
        for (c = 0; v; c++) {
            v &= v - 1;
        }
        return c;
    }

    EnumBitMask<Enum>& operator=(Enum value) {
        this->value = static_cast<EnumValueType>(value);
        return *this;
    }

    EnumBitMask<Enum>& operator|=(EnumBitMask<Enum> other) {
        value |= other.value;
        return *this;
    }

    EnumBitMask<Enum>& operator|=(Enum other) {
        value |= static_cast<EnumValueType>(other);
        return *this;
    }

    EnumBitMask<Enum>& operator&=(EnumBitMask<Enum> other) {
        value &= other.value;
        return *this;
    }

    EnumBitMask<Enum>& operator&=(Enum other) {
        value &= static_cast<EnumValueType>(other);
        return *this;
    }

    EnumBitMask<Enum>& operator^=(EnumBitMask<Enum> other) {
        value ^= other.value;
        return *this;
    }

    EnumBitMask<Enum>& operator^=(Enum other) {
        value ^= static_cast<EnumValueType>(other);
        return *this;
    }

    /// Returns a bit mask with no bits set.
    static constexpr EnumBitMask<Enum> None() {
        return Enum(0);
    }
};

template <typename Enum>
constexpr bool operator==(EnumBitMask<Enum> lhs, EnumBitMask<Enum> rhs) {
    return lhs.value == rhs.value;
}

template <typename Enum>
constexpr bool operator==(EnumBitMask<Enum> lhs, Enum rhs) {
    return lhs.value == static_cast<typename std::underlying_type<Enum>::type>(rhs);
}

template <typename Enum>
constexpr bool operator==(Enum lhs, EnumBitMask<Enum> rhs) {
    return static_cast<typename std::underlying_type<Enum>::type>(lhs) == rhs.value;
}

template <typename Enum>
constexpr bool operator!=(EnumBitMask<Enum> lhs, EnumBitMask<Enum> rhs) {
    return lhs.value != rhs.value;
}

template <typename Enum>
constexpr bool operator!=(EnumBitMask<Enum> lhs, Enum rhs) {
    return lhs.value != static_cast<typename std::underlying_type<Enum>::type>(rhs);
}

template <typename Enum>
constexpr bool operator!=(Enum lhs, EnumBitMask<Enum> rhs) {
    return static_cast<typename std::underlying_type<Enum>::type>(lhs) != rhs.value;
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator|(EnumBitMask<Enum> lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs.value | rhs.value);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator|(EnumBitMask<Enum> lhs, Enum rhs) {
    return lhs | EnumBitMask<Enum>(rhs);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator|(Enum lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs) | rhs;
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator&(EnumBitMask<Enum> lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs.value & rhs.value);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator&(EnumBitMask<Enum> lhs, Enum rhs) {
    return lhs & EnumBitMask<Enum>(rhs);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator&(Enum lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs) & rhs;
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator^(EnumBitMask<Enum> lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs.value ^ rhs.value);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator^(EnumBitMask<Enum> lhs, Enum rhs) {
    return lhs ^ EnumBitMask<Enum>(rhs);
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator^(Enum lhs, EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(lhs) ^ rhs;
}

template <typename Enum>
constexpr EnumBitMask<Enum> operator~(EnumBitMask<Enum> rhs) {
    return EnumBitMask<Enum>(~rhs.value);
}

#define TEPHRA_MAKE_ENUM_BIT_MASK(enumMaskName, enumName) \
    /** A bitmask of tp::##enumName values. */ \
    using enumMaskName = EnumBitMask<enumName>; \
    constexpr enumMaskName operator|(enumName lhs, enumName rhs) { \
        return enumMaskName(lhs) | enumMaskName(rhs); \
    } \
    constexpr enumMaskName operator~(enumName rhs) { \
        return ~enumMaskName(rhs); \
    }

/// An iterator over a tp::ContiguousEnumView.
template <typename Enum>
class ContiguousEnumIterator {
public:
    using EnumType = Enum;
    using EnumValueType = typename std::underlying_type<Enum>::type;
    EnumValueType value;

    constexpr ContiguousEnumIterator(EnumValueType value) : value(value) {}

    explicit constexpr ContiguousEnumIterator(Enum element) {
        static_assert(std::is_enum<Enum>::value, "Template parameter is not an enum");
        value = static_cast<EnumValueType>(element);
    }

    constexpr ContiguousEnumIterator operator++() {
        ++value;
        return *this;
    }

    constexpr Enum operator*() const {
        return static_cast<Enum>(value);
    }

    constexpr bool operator!=(const ContiguousEnumIterator& other) const {
        return value != other.value;
    }
};

/// Represents an array view of all the values of a contiguous enum type. The values of the enum need to be consecutive
/// and the largest value needs to be known.
template <typename Enum, Enum Last>
class ContiguousEnumView {
public:
    using EnumType = Enum;
    using EnumValueType = typename std::underlying_type<Enum>::type;

    static_assert(std::is_enum<Enum>::value, "Template parameter is not an enum");

    /// Returns an iterator to the first enum value.
    constexpr ContiguousEnumIterator<Enum> begin() const {
        return ContiguousEnumIterator<Enum>(0);
    }

    /// Returns an iterator to the last enum value.
    constexpr ContiguousEnumIterator<Enum> end() const {
        return ++ContiguousEnumIterator<Enum>(Last);
    }

    /// Returns the number of consecutive enum values in the view.
    static constexpr std::size_t size() {
        return static_cast<std::size_t>(Last) + 1;
    }
};

#define TEPHRA_MAKE_CONTIGUOUS_ENUM_VIEW(enumViewName, enumName, lastValue) \
    using enumViewName = ContiguousEnumView<enumName, enumName::lastValue>

}
