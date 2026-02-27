// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_ERROR_HPP_INCLUDED
#define MARU_CPP_ERROR_HPP_INCLUDED

#include <stdexcept>

#include "maru/maru.h"

namespace maru {

/**
 * @brief Thrown when a MARU operation fails.
 */
class Exception : public std::runtime_error {
public:
    Exception(MARU_Status status, const char* message)
        : std::runtime_error(message), m_status(status) {}
    
    MARU_Status status() const { return m_status; }

private:
    MARU_Status m_status;
};

/**
 * @brief Thrown when the entire library context is lost.
 */
class ContextLostException : public Exception {
public:
    ContextLostException(const char* message) : Exception(MARU_ERROR_CONTEXT_LOST, message) {}
};

/**
 * @brief Checks a status code and throws if it indicates an error.
 */
void check(MARU_Status status, const char* message);

} // namespace maru

#endif // MARU_CPP_ERROR_HPP_INCLUDED
