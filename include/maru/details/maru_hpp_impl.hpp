// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_HPP_IMPL_HPP_INCLUDED
#define MARU_HPP_IMPL_HPP_INCLUDED

namespace maru {

/* --- Exception Implementations --- */

inline void check(MARU_Status status, const char* message) {
    if (status == MARU_SUCCESS) return;
    if (status == MARU_ERROR_CONTEXT_LOST) throw ContextLostException(message);
    throw Exception(status, message);
}

/* --- Image Implementations --- */

inline Image::~Image() {
    if (m_handle) maru_destroyImage(m_handle);
}

inline Image::Image(Image&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_destroyImage(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Image::getUserData() const { return maru_getImageUserdata(m_handle); }
inline void Image::setUserData(void* userdata) { maru_setImageUserdata(m_handle, userdata); }

/* --- Cursor Implementations --- */

inline Cursor::~Cursor() {
    if (m_handle) maru_destroyCursor(m_handle);
}

inline Cursor::Cursor(Cursor&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Cursor& Cursor::operator=(Cursor&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_destroyCursor(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Cursor::getUserData() const { return maru_getCursorUserdata(m_handle); }
inline void Cursor::setUserData(void* userdata) { maru_setCursorUserdata(m_handle, userdata); }
inline bool Cursor::isSystem() const { return maru_isCursorSystem(m_handle); }

/* --- Monitor Implementations --- */

inline Monitor::Monitor(MARU_Monitor* handle, bool retain) : m_handle(handle) {
    if (m_handle && retain) maru_retainMonitor(m_handle);
}

inline Monitor::~Monitor() {
    if (m_handle) maru_releaseMonitor(m_handle);
}

inline Monitor::Monitor(const Monitor& other) : m_handle(other.m_handle) {
    if (m_handle) maru_retainMonitor(m_handle);
}

inline Monitor& Monitor::operator=(const Monitor& other) {
    if (this != &other) {
        if (m_handle) maru_releaseMonitor(m_handle);
        m_handle = other.m_handle;
        if (m_handle) maru_retainMonitor(m_handle);
    }
    return *this;
}

inline Monitor::Monitor(Monitor&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Monitor& Monitor::operator=(Monitor&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_releaseMonitor(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Monitor::getUserData() const { return maru_getMonitorUserdata(m_handle); }
inline void Monitor::setUserData(void* userdata) { maru_setMonitorUserdata(m_handle, userdata); }
inline const char* Monitor::getName() const { return maru_getMonitorName(m_handle); }
inline MARU_Vec2Dip Monitor::getPhysicalSize() const { return maru_getMonitorPhysicalSize(m_handle); }
inline MARU_VideoMode Monitor::getCurrentMode() const { return maru_getMonitorCurrentMode(m_handle); }
inline MARU_Vec2Dip Monitor::getLogicalPosition() const { return maru_getMonitorLogicalPosition(m_handle); }
inline MARU_Vec2Dip Monitor::getLogicalSize() const { return maru_getMonitorLogicalSize(m_handle); }
inline bool Monitor::isPrimary() const { return maru_isMonitorPrimary(m_handle); }
inline MARU_Scalar Monitor::getScale() const { return maru_getMonitorScale(m_handle); }
inline bool Monitor::isLost() const { return maru_isMonitorLost(m_handle); }

inline std::vector<MARU_VideoMode> Monitor::getModes() const {
    uint32_t count = 0;
    const MARU_VideoMode* modes_ptr = maru_getMonitorModes(m_handle, &count);
    if (!modes_ptr || count == 0) return std::vector<MARU_VideoMode>();
    return std::vector<MARU_VideoMode>(modes_ptr, modes_ptr + count);
}

inline void Monitor::setMode(MARU_VideoMode mode) {
    check(maru_setMonitorMode(m_handle, mode), "Failed to set monitor mode");
}

/* --- Controller Implementations --- */

inline Controller::Controller(MARU_Controller* handle, bool retain) : m_handle(handle) {
    if (m_handle && retain) maru_retainController(m_handle);
}

inline Controller::~Controller() {
    if (m_handle) maru_releaseController(m_handle);
}

inline Controller::Controller(const Controller& other) : m_handle(other.m_handle) {
    if (m_handle) maru_retainController(m_handle);
}

inline Controller& Controller::operator=(const Controller& other) {
    if (this != &other) {
        if (m_handle) maru_releaseController(m_handle);
        m_handle = other.m_handle;
        if (m_handle) maru_retainController(m_handle);
    }
    return *this;
}

inline Controller::Controller(Controller&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Controller& Controller::operator=(Controller&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_releaseController(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Controller::getUserData() const { return maru_getControllerUserdata(m_handle); }
inline void Controller::setUserData(void* userdata) { maru_setControllerUserdata(m_handle, userdata); }
inline bool Controller::isLost() const { return maru_isControllerLost(m_handle); }

inline MARU_ControllerInfo Controller::getInfo() const {
    MARU_ControllerInfo info;
    check(maru_getControllerInfo(m_handle, &info), "Failed to get controller info");
    return info;
}

inline uint32_t Controller::getAnalogCount() const { return maru_getControllerAnalogCount(m_handle); }
inline const MARU_AnalogChannelInfo* Controller::getAnalogChannelInfo() const { return maru_getControllerAnalogChannelInfo(m_handle); }
inline const MARU_AnalogInputState* Controller::getAnalogStates() const { return maru_getControllerAnalogStates(m_handle); }
inline uint32_t Controller::getButtonCount() const { return maru_getControllerButtonCount(m_handle); }
inline const MARU_ButtonChannelInfo* Controller::getButtonChannelInfo() const { return maru_getControllerButtonChannelInfo(m_handle); }
inline const MARU_ButtonState8* Controller::getButtonStates() const { return maru_getControllerButtonStates(m_handle); }
inline bool Controller::isButtonPressed(uint32_t button_id) const { return maru_isControllerButtonPressed(m_handle, button_id); }
inline uint32_t Controller::getHapticCount() const { return maru_getControllerHapticCount(m_handle); }
inline const MARU_HapticChannelInfo* Controller::getHapticChannelInfo() const { return maru_getControllerHapticChannelInfo(m_handle); }

inline void Controller::setHapticLevels(uint32_t first_haptic, uint32_t count, const MARU_Scalar* intensities) {
    check(maru_setControllerHapticLevels(m_handle, first_haptic, count, intensities), "Failed to set haptic levels");
}

/* --- Window Implementations --- */

inline Window::~Window() {
    if (m_handle) maru_destroyWindow(m_handle);
}

inline Window::Window(Window&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_destroyWindow(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Window::getUserData() const { return maru_getWindowUserdata(m_handle); }
inline void Window::setUserData(void* userdata) { maru_setWindowUserdata(m_handle, userdata); }
inline bool Window::isLost() const { return maru_isWindowLost(m_handle); }
inline bool Window::isReady() const { return maru_isWindowReady(m_handle); }
inline bool Window::isFocused() const { return maru_isWindowFocused(m_handle); }
inline bool Window::isMaximized() const { return maru_isWindowMaximized(m_handle); }
inline bool Window::isFullscreen() const { return maru_isWindowFullscreen(m_handle); }
inline bool Window::isVisible() const { return maru_isWindowVisible(m_handle); }
inline bool Window::isMinimized() const { return maru_isWindowMinimized(m_handle); }
inline const char* Window::getTitle() const { return maru_getWindowTitle(m_handle); }
inline MARU_EventMask Window::getEventMask() const { return maru_getWindowEventMask(m_handle); }

inline void Window::getGeometry(MARU_WindowGeometry& out_geometry) const {
    maru_getWindowGeometry(m_handle, &out_geometry);
}

inline void Window::update(uint64_t field_mask, const MARU_WindowAttributes& attributes) {
    check(maru_updateWindow(m_handle, field_mask, &attributes), "Failed to update window attributes");
}

inline void Window::requestFocus() { check(maru_requestWindowFocus(m_handle), "Failed to request window focus"); }
inline void Window::requestFrame() { check(maru_requestWindowFrame(m_handle), "Failed to request window frame"); }
inline void Window::requestAttention() { check(maru_requestWindowAttention(m_handle), "Failed to request window attention"); }

inline VkSurfaceKHR Window::createVkSurface(VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader) {
    VkSurfaceKHR surface = nullptr;
    check(maru_createVkSurface(m_handle, instance, vk_loader, &surface), "Failed to create Vulkan surface");
    return surface;
}

inline void Window::setTitle(const char* title) {
    MARU_WindowAttributes attrs = {};
    attrs.title = title;
    update(MARU_WINDOW_ATTR_TITLE, attrs);
}

inline void Window::setLogicalSize(MARU_Vec2Dip size) {
    MARU_WindowAttributes attrs = {};
    attrs.logical_size = size;
    update(MARU_WINDOW_ATTR_LOGICAL_SIZE, attrs);
}

inline void Window::setFullscreen(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.fullscreen = enabled;
    update(MARU_WINDOW_ATTR_FULLSCREEN, attrs);
}

inline void Window::setMaximized(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.maximized = enabled;
    update(MARU_WINDOW_ATTR_MAXIMIZED, attrs);
}

inline void Window::setVisible(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.visible = enabled;
    update(MARU_WINDOW_ATTR_VISIBLE, attrs);
}

inline void Window::setMinimized(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.minimized = enabled;
    update(MARU_WINDOW_ATTR_MINIMIZED, attrs);
}

inline void Window::setCursorMode(MARU_CursorMode mode) {
    MARU_WindowAttributes attrs = {};
    attrs.cursor_mode = mode;
    update(MARU_WINDOW_ATTR_CURSOR_MODE, attrs);
}

inline void Window::setCursor(MARU_Cursor* cursor) {
    MARU_WindowAttributes attrs = {};
    attrs.cursor = cursor;
    update(MARU_WINDOW_ATTR_CURSOR, attrs);
}

inline void Window::setEventMask(MARU_EventMask mask) {
    MARU_WindowAttributes attrs = {};
    attrs.event_mask = mask;
    update(MARU_WINDOW_ATTR_EVENT_MASK, attrs);
}

/* --- Context Implementations --- */

inline Context::Context() {
    MARU_ContextCreateInfo ci = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    check(maru_createContext(&ci, &m_handle), "Failed to create context");
}

inline Context::Context(const MARU_ContextCreateInfo& create_info) {
    check(maru_createContext(&create_info, &m_handle), "Failed to create context");
}

inline Context::~Context() {
    if (m_handle) maru_destroyContext(m_handle);
}

inline Context::Context(Context&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Context& Context::operator=(Context&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_destroyContext(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline void* Context::getUserData() const { return maru_getContextUserdata(m_handle); }
inline void Context::setUserData(void* userdata) { maru_setContextUserdata(m_handle, userdata); }
inline bool Context::isLost() const { return maru_isContextLost(m_handle); }
inline bool Context::isReady() const { return maru_isContextReady(m_handle); }

inline void Context::update(uint64_t field_mask, const MARU_ContextAttributes& attributes) {
    check(maru_updateContext(m_handle, field_mask, &attributes), "Failed to update context attributes");
}

inline std::vector<const char*> Context::getVkExtensions() const {
    uint32_t count = 0;
    const char** extensions = maru_getVkExtensions(m_handle, &count);
    if (!extensions || count == 0) return std::vector<const char*>();
    return std::vector<const char*>(extensions, extensions + count);
}

inline std::vector<Monitor> Context::getMonitors() {
    uint32_t count = 0;
    MARU_Monitor* const* monitors_ptr = maru_getMonitors(m_handle, &count);
    std::vector<Monitor> result;
    if (monitors_ptr && count > 0) {
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            result.emplace_back(monitors_ptr[i], true);
        }
    }
    return result;
}

inline std::vector<Controller> Context::getControllers() {
    MARU_ControllerList list;
    check(maru_getControllers(m_handle, &list), "Failed to get controllers");
    std::vector<Controller> result;
    if (list.controllers && list.count > 0) {
        result.reserve(list.count);
        for (uint32_t i = 0; i < list.count; ++i) {
            result.emplace_back(list.controllers[i], true);
        }
    }
    return result;
}

inline Window Context::createWindow(const MARU_WindowCreateInfo& create_info) {
    MARU_Window* window_handle = nullptr;
    check(maru_createWindow(m_handle, &create_info, &window_handle), "Failed to create window");
    return Window(window_handle);
}

inline Cursor Context::createCursor(const MARU_CursorCreateInfo& create_info) {
    MARU_Cursor* cursor_handle = nullptr;
    check(maru_createCursor(m_handle, &create_info, &cursor_handle), "Failed to create cursor");
    return Cursor(cursor_handle);
}

inline Image Context::createImage(const MARU_ImageCreateInfo& create_info) {
    MARU_Image* image_handle = nullptr;
    check(maru_createImage(m_handle, &create_info, &image_handle), "Failed to create image");
    return Image(image_handle);
}

inline void Context::pumpEvents(uint32_t timeout_ms, MARU_EventCallback callback, void* userdata) {
    check(maru_pumpEvents(m_handle, timeout_ms, callback, userdata), "Failed to pump events");
}

inline void Context::postEvent(MARU_EventId type, MARU_Window* window, MARU_UserDefinedEvent evt) {
    check(maru_postEvent(m_handle, type, window, evt), "Failed to post event");
}

inline void Context::wake() {
    check(maru_wakeContext(m_handle), "Failed to wake context");
}

} // namespace maru

#endif // MARU_HPP_IMPL_HPP_INCLUDED
