// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_WINDOW_HPP_INCLUDED
#define MARU_CPP_WINDOW_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Window.
 */
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    MARU_Window* get() const { return m_handle; }
    operator MARU_Window*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    
    bool isLost() const;
    bool isReady() const;
    bool isFocused() const;
    bool isMaximized() const;
    bool isFullscreen() const;
    bool isVisible() const;
    bool isMinimized() const;

    const char* getTitle() const;
    MARU_EventMask getEventMask() const;
    void getGeometry(MARU_WindowGeometry& out_geometry) const;

    void update(uint64_t field_mask, const MARU_WindowAttributes& attributes);
    void requestFocus();
    void requestFrame();
    void requestAttention();

    VkSurfaceKHR createVkSurface(VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader);

    // Convenience setters
    void setTitle(const char* title);
    void setLogicalSize(MARU_Vec2Dip size);
    void setFullscreen(bool enabled);
    void setMaximized(bool enabled);
    void setVisible(bool enabled);
    void setMinimized(bool enabled);
    void setCursorMode(MARU_CursorMode mode);
    void setCursor(MARU_Cursor* cursor);
    void setEventMask(MARU_EventMask mask);

    explicit Window(MARU_Window* handle) : m_handle(handle) {}

private:
    MARU_Window* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_WINDOW_HPP_INCLUDED
