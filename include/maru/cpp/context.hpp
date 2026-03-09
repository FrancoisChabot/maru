// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_CONTEXT_HPP_INCLUDED
#define MARU_CPP_CONTEXT_HPP_INCLUDED

#include <chrono>
#include <vector>

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"
#include "maru/cpp/expected.hpp"
#include "maru/cpp/window.hpp"
#include "maru/cpp/monitor.hpp"
#include "maru/cpp/controller.hpp"
#include "maru/cpp/cursor.hpp"
#include "maru/cpp/image.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Context.
 */
class Context {
public:
    [[nodiscard]] static expected<Context> create(const MARU_ContextCreateInfo& create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT);

    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Context(Context&& other) noexcept;
    Context& operator=(Context&& other) noexcept;

    MARU_Context* get() const { return m_handle; }
    operator MARU_Context*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    bool isLost() const;
    bool isReady() const;

    MARU_Status update(uint64_t field_mask, const MARU_ContextAttributes& attributes);
    
    std::vector<const char*> getVkExtensions() const;
    expected<std::vector<Monitor>> getMonitors();
    expected<std::vector<Controller>> getControllers();

    [[nodiscard]] expected<Window> createWindow(const MARU_WindowCreateInfo& create_info);
    [[nodiscard]] expected<Cursor> createCursor(const MARU_CursorCreateInfo& create_info);
    [[nodiscard]] expected<Image> createImage(const MARU_ImageCreateInfo& create_info);

    MARU_Status pumpEvents(uint32_t timeout_ms, MARU_EventCallback callback, void* userdata = nullptr);

#if __cplusplus >= 202002L
    template <typename Visitor>
    MARU_Status pumpEvents(EventDispatcher<Visitor>& dispatcher, uint32_t timeout_ms = 0);

    template <typename Visitor, typename Rep, typename Period>
    MARU_Status pumpEvents(EventDispatcher<Visitor>& dispatcher, std::chrono::duration<Rep, Period> timeout) {
        return pumpEvents(dispatcher, (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    }
#endif

    template <typename Rep, typename Period>
    MARU_Status pumpEvents(std::chrono::duration<Rep, Period> timeout, MARU_EventCallback callback, void* userdata = nullptr) {
        return pumpEvents((uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count(), callback, userdata);
    }

    MARU_Status postEvent(MARU_EventId type, MARU_Window* window, MARU_UserDefinedEvent evt);
    MARU_Status wake();

private:
    explicit Context(MARU_Context* handle) : m_handle(handle) {}
    MARU_Context* m_handle = nullptr;
};

} // namespace maru

#endif // MARU_CPP_CONTEXT_HPP_INCLUDED
