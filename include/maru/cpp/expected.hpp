// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_EXPECTED_HPP_INCLUDED
#define MARU_CPP_EXPECTED_HPP_INCLUDED

#include "maru/maru.h"
#include <type_traits>
#include <utility>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace maru {

#if __cplusplus >= 202302L
template <typename T, typename E = MARU_Status>
using expected = std::expected<T, E>;

template <typename E = MARU_Status> using unexpected = std::unexpected<E>;

#else

template <typename E = MARU_Status> class unexpected {
public:
  constexpr explicit unexpected(E err) : m_err(std::move(err)) {}
  constexpr const E &error() const & { return m_err; }
  constexpr E &&error() && { return std::move(m_err); }

private:
  E m_err;
};

template <typename T, typename E = MARU_Status> class expected {
public:
  constexpr expected(T val) : m_has_value(true), m_val(std::move(val)) {}
  constexpr expected(unexpected<E> err)
      : m_has_value(false), m_err(std::move(err.error())) {}

  expected(expected &&other) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : m_has_value(other.m_has_value) {
    if (m_has_value) {
      new (&m_val) T(std::move(other.m_val));
    } else {
      new (&m_err) E(std::move(other.m_err));
    }
  }

  expected &operator=(expected &&other) noexcept(
      std::is_nothrow_move_assignable<T>::value) {
    if (this != &other) {
      this->~expected();
      m_has_value = other.m_has_value;
      if (m_has_value) {
        new (&m_val) T(std::move(other.m_val));
      } else {
        new (&m_err) E(std::move(other.m_err));
      }
    }
    return *this;
  }

  expected(const expected &) = delete;
  expected &operator=(const expected &) = delete;

  ~expected() {
    if (m_has_value)
      m_val.~T();
    else
      m_err.~E();
  }

  constexpr explicit operator bool() const noexcept { return m_has_value; }
  constexpr bool has_value() const noexcept { return m_has_value; }

  constexpr T *operator->() noexcept { return &m_val; }
  constexpr const T *operator->() const noexcept { return &m_val; }
  constexpr T &operator*() noexcept { return m_val; }
  constexpr const T &operator*() const noexcept { return m_val; }

  constexpr E error() const noexcept { return m_err; }

private:
  bool m_has_value;
  union {
    T m_val;
    E m_err;
  };
};

#endif

} // namespace maru

#endif // MARU_CPP_EXPECTED_HPP_INCLUDED
