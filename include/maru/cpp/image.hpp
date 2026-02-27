// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_IMAGE_HPP_INCLUDED
#define MARU_CPP_IMAGE_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Image.
 */
class Image {
public:
    Image() = default;
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    MARU_Image* get() const { return m_handle; }
    operator MARU_Image*() const { return m_handle; }

    void* getUserData() const;
    void setUserData(void* userdata);

    explicit Image(MARU_Image* handle) : m_handle(handle) {}

private:
    MARU_Image* m_handle = nullptr;

    friend class Context;
};

} // namespace maru

#endif // MARU_CPP_IMAGE_HPP_INCLUDED
