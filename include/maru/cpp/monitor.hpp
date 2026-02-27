// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_MONITOR_HPP_INCLUDED
#define MARU_CPP_MONITOR_HPP_INCLUDED

#include <vector>

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Monitor.
 */
class Monitor {
public:
    Monitor() = default;
    ~Monitor();

    Monitor(const Monitor& other);
    Monitor& operator=(const Monitor& other);

    Monitor(Monitor&& other) noexcept;
    Monitor& operator=(Monitor&& other) noexcept;

    MARU_Monitor* get() const { return m_handle; }
    operator MARU_Monitor*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    
    const char* getName() const;
    MARU_Vec2Dip getPhysicalSize() const;
    MARU_VideoMode getCurrentMode() const;
    MARU_Vec2Dip getLogicalPosition() const;
    MARU_Vec2Dip getLogicalSize() const;
    bool isPrimary() const;
    MARU_Scalar getScale() const;
    bool isLost() const;

    std::vector<MARU_VideoMode> getModes() const;
    void setMode(MARU_VideoMode mode);

    explicit Monitor(MARU_Monitor* handle, bool retain = true);

private:
    MARU_Monitor* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_MONITOR_HPP_INCLUDED
