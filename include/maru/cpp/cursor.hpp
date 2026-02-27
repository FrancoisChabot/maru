// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_CURSOR_HPP_INCLUDED
#define MARU_CPP_CURSOR_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Cursor.
 */
class Cursor {
public:
    Cursor() = default;
    ~Cursor();

    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    Cursor(Cursor&& other) noexcept;
    Cursor& operator=(Cursor&& other) noexcept;

    MARU_Cursor* get() const { return m_handle; }
    operator MARU_Cursor*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    bool isSystem() const;

    explicit Cursor(MARU_Cursor* handle) : m_handle(handle) {}

private:
    MARU_Cursor* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_CURSOR_HPP_INCLUDED
