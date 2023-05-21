#pragma once

#include <tephra/macros.hpp>
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

namespace tp {

/// Shared base implementation of array views.
template <typename TValue>
class ArrayViewBase {
public:
    using ValueType = TValue;
    using ReferenceType = TValue&;
    using PointerType = TValue*;
    using IteratorType = PointerType;
    using SizeType = std::size_t;

    /// PointerType to the viewed array.
    constexpr PointerType data() const {
        return data_;
    }

    /// Number of elements in the viewed array.
    constexpr SizeType size() const {
        return size_;
    }

    /// The first element in the view.
    constexpr ReferenceType front() const {
        return (*this)[0];
    }

    /// The last element in the view.
    constexpr ReferenceType back() const {
        return (*this)[size_ - 1];
    }

    /// Iterator at the start of the view.
    constexpr IteratorType begin() const {
        return data_;
    }

    /// Iterator at the end of the view.
    constexpr IteratorType end() const {
        return data_ + size_;
    }

    /// Returns true if the viewed array is empty.
    constexpr bool empty() const {
        return size_ == 0;
    }

    /// Returns a reference to the nth element.
    constexpr ReferenceType operator[](SizeType pos) const {
        return data_[pos];
    }

protected:
    PointerType data_ = nullptr;
    SizeType size_ = 0;

    constexpr ArrayViewBase() = default;
    constexpr ArrayViewBase(PointerType data, SizeType size) noexcept : data_(data), size_(size) {}
};

/// Provides a read/write view into any contiguous array with a combination of a pointer and size.
/// The ArrayView does not own the referenced array and cannot add or remove elements from it.
/// It can be used to pass arbitrary arrays as parameters to functions and as a way to integrate
/// C style and C++ style arrays.
/// See the tp::view, tp::viewOne and tp::viewRange functions for convenient ways of conversion to array view.
template <typename T>
class ArrayView : public ArrayViewBase<T> {
    using Base = ArrayViewBase<T>;

public:
    constexpr ArrayView() = default;

    /// Creates a view of a contiguous range of values.
    /// @param data
    ///     The pointer to the start of the viewed range.
    /// @param size
    ///     The number of elements in the viewed range.
    constexpr ArrayView(typename Base::PointerType data, typename Base::SizeType size) noexcept : Base(data, size) {}

    constexpr operator ArrayView<const typename Base::ValueType>() const {
        return ArrayView<const typename Base::ValueType>(
            static_cast<const typename Base::PointerType>(Base::data_), Base::size_);
    }
};

template <typename T>
constexpr bool operator==(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
constexpr bool operator!=(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return !std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
constexpr bool operator<(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
constexpr bool operator>=(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return !std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
constexpr bool operator>(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return std::lexicographical_compare(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
}

template <typename T>
constexpr bool operator<=(const ArrayView<T>& lhs, const ArrayView<T>& rhs) {
    return !std::lexicographical_compare(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
}

/// Similar to tp::ArrayView, it provides read/write view of a contiguous array.
/// It is used to view arrays that may have a temporary lifetime, such as `std::initializer_list`.
template <typename T>
class ArrayParameter : public ArrayViewBase<T> {
    using Base = ArrayViewBase<T>;

public:
    constexpr ArrayParameter() = default;

    /// Creates a view of a contiguous range of values that may have a temporary lifetime.
    /// @param data
    ///     The pointer to the start of the viewed range.
    /// @param size
    ///     The number of elements in the viewed range.
    constexpr ArrayParameter(typename Base::PointerType data, typename Base::SizeType size) noexcept
        : Base(data, size) {}

    /// Implicit conversion constructor from tp::ArrayView.
    template <typename ViewValueType>
    constexpr ArrayParameter(ArrayView<ViewValueType> arrayView) noexcept
        : Base(static_cast<typename Base::PointerType>(arrayView.data()), arrayView.size()) {}

    /// Implicit conversion constructor from std::initializer_list.
    constexpr ArrayParameter(std::initializer_list<T> list) noexcept
        : Base(list.begin(), static_cast<typename Base::SizeType>(list.end() - list.begin())) {}
};

/// Creates an array view of a single element.
template <typename T>
constexpr ArrayView<T> viewOne(T& value) noexcept {
    return ArrayView<T>(&value, 1);
}

template <typename T>
constexpr ArrayView<const T> viewOne(const T& value) noexcept {
    return ArrayView<const T>(&value, 1);
}

template <typename T>
constexpr ArrayParameter<T> viewOne(T&& value) noexcept {
    return ArrayParameter<T>(&value, 1);
}

/// Creates an array view of a contiguous array of elements.
template <typename T>
constexpr ArrayView<T> view(T* ptr, std::size_t size) noexcept {
    return ArrayView<T>(ptr, size);
}

template <typename T>
constexpr ArrayView<const T> view(const T* ptr, std::size_t size) noexcept {
    return ArrayView<const T>(ptr, size);
}

template <typename T, std::size_t N>
constexpr ArrayView<T> view(T (&array)[N]) noexcept {
    return ArrayView<T>(array, N);
}

template <typename T, std::size_t N>
constexpr ArrayView<const T> view(const T (&array)[N]) noexcept {
    return ArrayView<const T>(array, N);
}

template <typename T>
constexpr ArrayView<T> view(ArrayView<T> view) noexcept {
    return view;
}

template <typename T>
constexpr ArrayParameter<T> view(std::initializer_list<T> list) noexcept {
    return ArrayParameter<T>(list);
}

template <typename T, std::size_t Size>
constexpr ArrayView<T> view(std::array<T, Size>& list) noexcept {
    return ArrayView<T>(list.data(), Size);
}

template <typename T, std::size_t Size>
constexpr ArrayView<const T> view(const std::array<T, Size>& list) noexcept {
    return ArrayView<const T>(list.data(), Size);
}

template <typename T, std::size_t Size>
constexpr ArrayParameter<T> view(std::array<T, Size>&& list) noexcept {
    return ArrayParameter<T>(list.data(), Size);
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayView<T> view(std::vector<T, Allocator>& list) noexcept {
    return ArrayView<T>(list.data(), list.size());
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayView<const T> view(const std::vector<T, Allocator>& list) noexcept {
    return ArrayView<const T>(list.data(), list.size());
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayParameter<T> view(std::vector<T, Allocator>&& list) noexcept {
    return ArrayParameter<T>(list.data(), list.size());
}

/// Creates an array view of a contiguous range of an array of elements.
template <typename T>
constexpr ArrayView<T> viewRange(ArrayView<T>& view, std::size_t start, std::size_t count) noexcept {
    return ArrayView<T>(view.data() + start, count);
}

template <typename T>
constexpr ArrayView<const T> viewRange(const ArrayView<T>& view, std::size_t start, std::size_t count) noexcept {
    return ArrayView<const T>(view.data() + start, count);
}

template <typename T>
constexpr ArrayView<T> viewRange(T* ptr, std::size_t start, std::size_t count) noexcept {
    return ArrayView<T>(ptr + start, count);
}

template <typename T>
constexpr ArrayView<const T> viewRange(const T* ptr, std::size_t start, std::size_t count) noexcept {
    return ArrayView<const T>(ptr + start, count);
}

template <typename T, std::size_t Size>
constexpr ArrayView<T> viewRange(std::array<T, Size>& list, std::size_t start, std::size_t count) noexcept {
    return ArrayView<T>(list.data() + start, count);
}

template <typename T, std::size_t Size>
constexpr ArrayView<const T> viewRange(const std::array<T, Size>& list, std::size_t start, std::size_t count) noexcept {
    return ArrayView<const T>(list.data() + start, count);
}

template <typename T, std::size_t Size>
constexpr ArrayParameter<T> viewRange(std::array<T, Size>&& list, std::size_t start, std::size_t count) noexcept {
    return ArrayParameter<T>(list.data() + start, count);
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayView<T> viewRange(std::vector<T, Allocator>& list, std::size_t start, std::size_t count) noexcept {
    return ArrayView<T>(list.data() + start, count);
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayView<const T> viewRange(
    const std::vector<T, Allocator>& list,
    std::size_t start,
    std::size_t count) noexcept {
    return ArrayView<const T>(list.data() + start, count);
}

template <typename T, class Allocator = std::allocator<T>>
constexpr ArrayParameter<T> viewRange(std::vector<T, Allocator>&& list, std::size_t start, std::size_t count) noexcept {
    return ArrayParameter<T>(list.data() + start, count);
}

}
