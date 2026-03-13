// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_WINDOW_HPP_INCLUDED
#define MARU_CPP_WINDOW_HPP_INCLUDED

#include "maru/cpp/fwd.hpp"
#include "maru/cpp/expected.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Window.
 */
class Window {
public:
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    MARU_Window* get() const { return m_handle; }
    operator MARU_Window*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    
    bool isReady() const;
    bool isFocused() const;
    bool isMaximized() const;
    bool isFullscreen() const;
    bool isVisible() const;
    bool isMinimized() const;

    MARU_WindowId getId() const;
    const char* getTitle() const;
    void getGeometry(MARU_WindowGeometry& out_geometry) const;

    MARU_Status update(uint64_t field_mask, const MARU_WindowAttributes& attributes);
    MARU_Status requestFocus();
    MARU_Status requestFrame();
    MARU_Status requestAttention();

    // Convenience setters
    MARU_Status setTitle(const char* title);
    MARU_Status setDipSize(MARU_Vec2Dip size);
    MARU_Status setFullscreen(bool enabled);
    MARU_Status setMaximized(bool enabled);
    MARU_Status setVisible(bool enabled);
    MARU_Status setMinimized(bool enabled);
    MARU_Status setCursorMode(MARU_CursorMode mode);
    MARU_Status setCursor(const MARU_Cursor* cursor);

private:
    explicit Window(MARU_Window* handle) : m_handle(handle) {}
    MARU_Window* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_WINDOW_HPP_INCLUDED
