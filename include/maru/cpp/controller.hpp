// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_CONTROLLER_HPP_INCLUDED
#define MARU_CPP_CONTROLLER_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"
#include "maru/cpp/expected.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Controller.
 */
class Controller {
public:
    Controller() = default;
    ~Controller();

    Controller(const Controller& other);
    Controller& operator=(const Controller& other);

    Controller(Controller&& other) noexcept;
    Controller& operator=(Controller&& other) noexcept;

    MARU_Controller* get() const { return m_handle; }
    operator MARU_Controller*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);
    bool isLost() const;

    bool isStandardized() const;
    uint32_t getAnalogCount() const;
    const MARU_ChannelInfo* getAnalogChannelInfo() const;
    const MARU_AnalogInputState* getAnalogStates() const;
    uint32_t getButtonCount() const;
    const MARU_ChannelInfo* getButtonChannelInfo() const;
    const MARU_ButtonState8* getButtonStates() const;
    bool isButtonPressed(uint32_t button_id) const;
    uint32_t getHapticCount() const;
    const MARU_ChannelInfo* getHapticChannelInfo() const;

    MARU_Status setHapticLevels(uint32_t first_haptic, uint32_t count, const MARU_Scalar* intensities);

    explicit Controller(MARU_Controller* handle, bool retain = true);

private:
    MARU_Controller* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_CONTROLLER_HPP_INCLUDED
