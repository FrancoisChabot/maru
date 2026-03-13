// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_HPP_IMPL_HPP_INCLUDED
#define MARU_HPP_IMPL_HPP_INCLUDED

namespace maru {

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
inline MARU_Vec2Mm Monitor::getPhysicalSize() const { return maru_getMonitorPhysicalSize(m_handle); }
inline MARU_VideoMode Monitor::getCurrentMode() const { return maru_getMonitorCurrentMode(m_handle); }
inline MARU_Vec2Dip Monitor::getDipPosition() const { return maru_getMonitorDipPosition(m_handle); }
inline MARU_Vec2Dip Monitor::getDipSize() const { return maru_getMonitorDipSize(m_handle); }
inline bool Monitor::isPrimary() const { return maru_isMonitorPrimary(m_handle); }
inline MARU_Scalar Monitor::getScale() const { return maru_getMonitorScale(m_handle); }
inline bool Monitor::isLost() const { return maru_isMonitorLost(m_handle); }

inline std::vector<MARU_VideoMode> Monitor::getModes() const {
    MARU_VideoModeList list = {};
    MARU_Status status = maru_getMonitorModes(m_handle, &list);
    if (status != MARU_SUCCESS || !list.modes || list.count == 0) return std::vector<MARU_VideoMode>();
    return std::vector<MARU_VideoMode>(list.modes, list.modes + list.count);
}

inline MARU_Status Monitor::setMode(MARU_VideoMode mode) {
    return maru_setMonitorMode(m_handle, mode);
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

inline expected<MARU_ControllerInfo> Controller::getInfo() const {
    MARU_ControllerInfo info;
    MARU_Status status = maru_getControllerInfo(m_handle, &info);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return info;
}

inline uint32_t Controller::getAnalogCount() const { return maru_getControllerAnalogCount(m_handle); }
inline const MARU_ChannelInfo* Controller::getAnalogChannelInfo() const { return maru_getControllerAnalogChannelInfo(m_handle); }
inline const MARU_AnalogInputState* Controller::getAnalogStates() const { return maru_getControllerAnalogStates(m_handle); }
inline uint32_t Controller::getButtonCount() const { return maru_getControllerButtonCount(m_handle); }
inline const MARU_ChannelInfo* Controller::getButtonChannelInfo() const { return maru_getControllerButtonChannelInfo(m_handle); }
inline const MARU_ButtonState8* Controller::getButtonStates() const { return maru_getControllerButtonStates(m_handle); }
inline bool Controller::isButtonPressed(uint32_t button_id) const { return maru_isControllerButtonPressed(m_handle, button_id); }
inline uint32_t Controller::getHapticCount() const { return maru_getControllerHapticCount(m_handle); }
inline const MARU_ChannelInfo* Controller::getHapticChannelInfo() const { return maru_getControllerHapticChannelInfo(m_handle); }

inline MARU_Status Controller::setHapticLevels(uint32_t first_haptic, uint32_t count, const MARU_Scalar* intensities) {
    return maru_setControllerHapticLevels(m_handle, first_haptic, count, intensities);
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
inline bool Window::isReady() const { return maru_isWindowReady(m_handle); }
inline bool Window::isFocused() const { return maru_isWindowFocused(m_handle); }
inline bool Window::isMaximized() const { return maru_isWindowMaximized(m_handle); }
inline bool Window::isFullscreen() const { return maru_isWindowFullscreen(m_handle); }
inline bool Window::isVisible() const { return maru_isWindowVisible(m_handle); }
inline bool Window::isMinimized() const { return maru_isWindowMinimized(m_handle); }
inline MARU_WindowId Window::getId() const { return maru_getWindowId(m_handle); }
inline const char* Window::getTitle() const { return maru_getWindowTitle(m_handle); }

inline void Window::getGeometry(MARU_WindowGeometry& out_geometry) const {
    out_geometry = maru_getWindowGeometry(m_handle);
}

inline MARU_Status Window::update(uint64_t field_mask, const MARU_WindowAttributes& attributes) {
    return maru_updateWindow(m_handle, field_mask, &attributes);
}

inline MARU_Status Window::requestFocus() { return maru_requestWindowFocus(m_handle); }
inline MARU_Status Window::requestFrame() { return maru_requestWindowFrame(m_handle); }
inline MARU_Status Window::requestAttention() { return maru_requestWindowAttention(m_handle); }

inline expected<VkSurfaceKHR> Window::createVkSurface(VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader) {
    VkSurfaceKHR surface = nullptr;
    MARU_Status status = maru_createVkSurface(m_handle, instance, vk_loader, &surface);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return surface;
}

inline MARU_Status Window::setTitle(const char* title) {
    MARU_WindowAttributes attrs = {};
    attrs.title = title;
    return update(MARU_WINDOW_ATTR_TITLE, attrs);
}

inline MARU_Status Window::setDipSize(MARU_Vec2Dip size) {
    MARU_WindowAttributes attrs;
    attrs.dip_size = size;
    return update(MARU_WINDOW_ATTR_DIP_SIZE, attrs);
}

inline MARU_Status Window::setFullscreen(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.fullscreen = enabled;
    return update(MARU_WINDOW_ATTR_FULLSCREEN, attrs);
}

inline MARU_Status Window::setMaximized(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.maximized = enabled;
    return update(MARU_WINDOW_ATTR_MAXIMIZED, attrs);
}

inline MARU_Status Window::setVisible(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.visible = enabled;
    return update(MARU_WINDOW_ATTR_VISIBLE, attrs);
}

inline MARU_Status Window::setMinimized(bool enabled) {
    MARU_WindowAttributes attrs = {};
    attrs.minimized = enabled;
    return update(MARU_WINDOW_ATTR_MINIMIZED, attrs);
}

inline MARU_Status Window::setCursorMode(MARU_CursorMode mode) {
    MARU_WindowAttributes attrs = {};
    attrs.cursor_mode = mode;
    return update(MARU_WINDOW_ATTR_CURSOR_MODE, attrs);
}

inline MARU_Status Window::setCursor(MARU_Cursor* cursor) {
    MARU_WindowAttributes attrs = {};
    attrs.cursor = cursor;
    return update(MARU_WINDOW_ATTR_CURSOR, attrs);
}

/* --- Context Implementations --- */

inline expected<Context> Context::create(const MARU_ContextCreateInfo& create_info) {
    MARU_Context* handle = nullptr;
    MARU_Status status = maru_createContext(&create_info, &handle);
    if (status != MARU_SUCCESS) {
        return unexpected<MARU_Status>(status);
    }
    return Context(handle);
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

inline MARU_Status Context::update(uint64_t field_mask, const MARU_ContextAttributes& attributes) {
    return maru_updateContext(m_handle, field_mask, &attributes);
}

inline std::vector<const char*> Context::getVkExtensions() const {
    MARU_VkExtensionList list = {};
    MARU_Status status = maru_getVkExtensions(m_handle, &list);
    if (status != MARU_SUCCESS || !list.names || list.count == 0) return std::vector<const char*>();
    return std::vector<const char*>(list.names, list.names + list.count);
}

inline expected<std::vector<Monitor>> Context::getMonitors() {
    MARU_MonitorList list = {};
    MARU_Status status = maru_getMonitors(m_handle, &list);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);

    std::vector<Monitor> result;
    if (list.monitors && list.count > 0) {
        result.reserve(list.count);
        for (uint32_t i = 0; i < list.count; ++i) {
            result.emplace_back(list.monitors[i], true);
        }
    }
    return result;
}

inline expected<std::vector<Controller>> Context::getControllers() {
    MARU_ControllerList list;
    MARU_Status status = maru_getControllers(m_handle, &list);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    
    std::vector<Controller> result;
    if (list.controllers && list.count > 0) {
        result.reserve(list.count);
        for (uint32_t i = 0; i < list.count; ++i) {
            result.emplace_back(list.controllers[i], true);
        }
    }
    return result;
}

inline expected<Window> Context::createWindow(const MARU_WindowCreateInfo& create_info) {
    MARU_Window* window_handle = nullptr;
    MARU_Status status = maru_createWindow(m_handle, &create_info, &window_handle);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return Window(window_handle);
}

inline expected<Cursor> Context::createCursor(const MARU_CursorCreateInfo& create_info) {
    MARU_Cursor* cursor_handle = nullptr;
    MARU_Status status = maru_createCursor(m_handle, &create_info, &cursor_handle);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return Cursor(cursor_handle);
}

inline expected<Image> Context::createImage(const MARU_ImageCreateInfo& create_info) {
    MARU_Image* image_handle = nullptr;
    MARU_Status status = maru_createImage(m_handle, &create_info, &image_handle);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return Image(image_handle);
}

inline MARU_Status Context::pumpEvents(uint32_t timeout_ms, MARU_EventMask mask,
                                       MARU_EventCallback callback,
                                       void* userdata) {
    return maru_pumpEvents(m_handle, timeout_ms, mask, callback, userdata);
}

inline bool Context::postEvent(MARU_EventId type, MARU_Window* window, MARU_UserDefinedEvent evt) {
    return maru_postEvent(m_handle, type, window, evt);
}

inline bool Context::wake() {
    return maru_wakeContext(m_handle);
}

/* --- Queue Implementations --- */

inline expected<Queue, bool> Queue::create(const MARU_QueueCreateInfo& create_info) {
    MARU_Queue* handle = nullptr;
    if (!maru_createQueue(&create_info, &handle)) return unexpected<bool>(false);
    return Queue(handle);
}

inline expected<Queue, bool> Queue::create(uint32_t capacity) {
    MARU_QueueCreateInfo create_info = MARU_QUEUE_CREATE_INFO_DEFAULT;
    create_info.capacity = capacity;
    MARU_Queue* handle = nullptr;
    if (!maru_createQueue(&create_info, &handle)) return unexpected<bool>(false);
    return Queue(handle);
}

inline Queue::~Queue() {
    if (m_handle) maru_destroyQueue(m_handle);
}

inline Queue::Queue(Queue&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

inline Queue& Queue::operator=(Queue&& other) noexcept {
    if (this != &other) {
        if (m_handle) maru_destroyQueue(m_handle);
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

inline bool Queue::push(MARU_EventId type, MARU_WindowId window_id,
                               const MARU_Event& event) {
    return maru_pushQueue(m_handle, type, window_id, &event);
}

inline bool Queue::commit() {
    return maru_commitQueue(m_handle);
}

inline void Queue::scan(MARU_EventMask mask, MARU_QueueEventCallback callback,
                        void* userdata) {
    maru_scanQueue(m_handle, mask, callback, userdata);
}

} // namespace maru

#endif // MARU_HPP_IMPL_HPP_INCLUDED
