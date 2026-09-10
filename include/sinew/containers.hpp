#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <string_view>
#if !defined(SINEW_NO_IOSTREAMS)
#include <ostream>
#endif
#include <cstring>
#include <algorithm>

#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(SINEW_NO_EXCEPTIONS)
#define SINEW_HAS_EXCEPTIONS 1
#include <stdexcept>
#else
#define SINEW_HAS_EXCEPTIONS 0
#endif

namespace sinew {

/**
 * @brief Fixed-capacity, zero-allocation vector suitable for zero-copy Sinew message wire format.
 *
 * Requirements: T must be standard layout and trivially copyable.
 * This guarantees that StaticVector<T, Capacity> is also standard layout and trivially copyable.
 */
template <typename T, size_t Capacity>
class StaticVector {
    static_assert(Capacity > 0, "StaticVector capacity must be greater than zero");
    static_assert(std::is_trivially_copyable_v<T>, "StaticVector value type must be trivially copyable");
    static_assert(std::is_standard_layout_v<T>, "StaticVector value type must be standard layout");

public:
    using value_type = T;
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    T data_[Capacity]{};
    uint32_t size_{0};

    constexpr StaticVector() noexcept = default;

    constexpr size_type size() const noexcept { return size_; }
    static constexpr size_type capacity() noexcept { return static_cast<size_type>(Capacity); }
    static constexpr size_type max_size() noexcept { return static_cast<size_type>(Capacity); }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr bool full() const noexcept { return size_ >= Capacity; }

    constexpr void clear() noexcept { size_ = 0; }

    constexpr bool push_back(const T& value) noexcept {
        if (size_ >= Capacity) {
            return false;
        }
        data_[size_++] = value;
        return true;
    }

    template <typename... Args>
    constexpr bool emplace_back(Args&&... args) noexcept {
        if (size_ >= Capacity) {
            return false;
        }
        data_[size_++] = T{std::forward<Args>(args)...};
        return true;
    }

    constexpr bool pop_back() noexcept {
        if (size_ == 0) {
            return false;
        }
        --size_;
        return true;
    }

    constexpr bool resize(size_type new_size, const T& default_value = T{}) noexcept {
        if (new_size > Capacity) {
            return false;
        }
        for (size_type i = size_; i < new_size; ++i) {
            data_[i] = default_value;
        }
        size_ = new_size;
        return true;
    }

    constexpr reference operator[](size_type index) noexcept {
        return data_[index];
    }

    constexpr const_reference operator[](size_type index) const noexcept {
        return data_[index];
    }

    constexpr reference at(size_type index) {
        if (index >= size_) {
#if SINEW_HAS_EXCEPTIONS
            throw std::out_of_range("StaticVector::at() index out of range");
#else
            return data_[0];
#endif
        }
        return data_[index];
    }

    constexpr const_reference at(size_type index) const {
        if (index >= size_) {
#if SINEW_HAS_EXCEPTIONS
            throw std::out_of_range("StaticVector::at() index out of range");
#else
            return data_[0];
#endif
        }
        return data_[index];
    }

    constexpr pointer data() noexcept { return data_; }
    constexpr const_pointer data() const noexcept { return data_; }

    constexpr reference front() noexcept { return data_[0]; }
    constexpr const_reference front() const noexcept { return data_[0]; }

    constexpr reference back() noexcept { return data_[size_ > 0 ? size_ - 1 : 0]; }
    constexpr const_reference back() const noexcept { return data_[size_ > 0 ? size_ - 1 : 0]; }

    constexpr iterator begin() noexcept { return data_; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }

    constexpr iterator end() noexcept { return data_ + size_; }
    constexpr const_iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }

    constexpr bool operator==(const StaticVector& other) const noexcept {
        if (size_ != other.size_) return false;
        for (size_type i = 0; i < size_; ++i) {
            if (!(data_[i] == other.data_[i])) return false;
        }
        return true;
    }

    constexpr bool operator!=(const StaticVector& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Fixed-capacity, zero-allocation string guaranteed to be null-terminated.
 *
 * Trivially copyable and standard layout so it can be embedded directly in Sinew messages.
 */
template <size_t Capacity>
class StaticString {
    static_assert(Capacity > 0, "StaticString capacity must be greater than zero");

public:
    using value_type = char;
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;
    using reference = char&;
    using const_reference = const char&;
    using pointer = char*;
    using const_pointer = const char*;
    using iterator = char*;
    using const_iterator = const char*;

    char data_[Capacity + 1]{};
    uint32_t size_{0};

    constexpr StaticString() noexcept {
        data_[0] = '\0';
    }

    constexpr StaticString(std::string_view sv) noexcept {
        assign(sv);
    }

    constexpr StaticString(const char* s) noexcept {
        if (s != nullptr) {
            assign(std::string_view(s));
        }
    }

    constexpr StaticString& operator=(std::string_view sv) noexcept {
        assign(sv);
        return *this;
    }

    constexpr StaticString& operator=(const char* s) noexcept {
        if (s != nullptr) {
            assign(std::string_view(s));
        } else {
            clear();
        }
        return *this;
    }

    constexpr void clear() noexcept {
        size_ = 0;
        data_[0] = '\0';
    }

    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type length() const noexcept { return size_; }
    static constexpr size_type capacity() noexcept { return static_cast<size_type>(Capacity); }
    static constexpr size_type max_size() noexcept { return static_cast<size_type>(Capacity); }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr bool full() const noexcept { return size_ >= Capacity; }

    constexpr const char* c_str() const noexcept { return data_; }
    constexpr const char* data() const noexcept { return data_; }
    constexpr char* data() noexcept { return data_; }

    constexpr char& front() noexcept { return data_[0]; }
    constexpr const char& front() const noexcept { return data_[0]; }

    constexpr char& back() noexcept { return data_[size_ > 0 ? size_ - 1 : 0]; }
    constexpr const char& back() const noexcept { return data_[size_ > 0 ? size_ - 1 : 0]; }

    constexpr iterator begin() noexcept { return data_; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }

    constexpr iterator end() noexcept { return data_ + size_; }
    constexpr const_iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }

    constexpr std::string_view view() const noexcept {
        return std::string_view(data_, size_);
    }

    constexpr operator std::string_view() const noexcept {
        return view();
    }

    constexpr bool assign(std::string_view sv) noexcept {
        const size_t copy_len = (sv.size() < Capacity) ? sv.size() : Capacity;
        for (size_t i = 0; i < copy_len; ++i) {
            data_[i] = sv[i];
        }
        data_[copy_len] = '\0';
        size_ = static_cast<uint32_t>(copy_len);
        return sv.size() <= Capacity;
    }

    constexpr bool append(std::string_view sv) noexcept {
        const size_t available = Capacity - size_;
        const size_t copy_len = (sv.size() < available) ? sv.size() : available;
        for (size_t i = 0; i < copy_len; ++i) {
            data_[size_ + i] = sv[i];
        }
        size_ += static_cast<uint32_t>(copy_len);
        data_[size_] = '\0';
        return sv.size() <= available;
    }

    constexpr char operator[](size_type index) const noexcept { return data_[index]; }
    constexpr char& operator[](size_type index) noexcept { return data_[index]; }

    constexpr char at(size_type index) const {
        if (index >= size_) {
#if SINEW_HAS_EXCEPTIONS
            throw std::out_of_range("StaticString::at() index out of range");
#else
            return data_[0];
#endif
        }
        return data_[index];
    }

    constexpr char& at(size_type index) {
        if (index >= size_) {
#if SINEW_HAS_EXCEPTIONS
            throw std::out_of_range("StaticString::at() index out of range");
#else
            return data_[0];
#endif
        }
        return data_[index];
    }

    constexpr bool operator==(std::string_view other) const noexcept {
        return view() == other;
    }

    constexpr bool operator!=(std::string_view other) const noexcept {
        return view() != other;
    }

    constexpr bool operator==(const StaticString& other) const noexcept {
        return view() == other.view();
    }

    constexpr bool operator!=(const StaticString& other) const noexcept {
        return view() != other.view();
    }
};

#if !defined(SINEW_NO_IOSTREAMS)
template <size_t Capacity>
inline std::ostream& operator<<(std::ostream& os, const StaticString<Capacity>& s) {
    return os << s.view();
}
#endif

} // namespace sinew
