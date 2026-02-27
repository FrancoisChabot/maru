// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_FWD_HPP_INCLUDED
#define MARU_CPP_FWD_HPP_INCLUDED

#include "maru/maru.h"

namespace maru {

class Context;
class Window;
class Monitor;
class Cursor;
class Image;
class Controller;

#if __cplusplus >= 202002L
template <typename Visitor>
class EventDispatcher;
#endif

} // namespace maru

#endif // MARU_CPP_FWD_HPP_INCLUDED
