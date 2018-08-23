//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_ASSUME_HPP
#define NETU_DETAIL_ASSUME_HPP

#include <cassert>

#ifdef __GNUG__
#define NETU_ASSUME(expr)                                                      \
    do                                                                         \
    {                                                                          \
        assert(expr);                                                          \
        if (!(expr))                                                           \
            __builtin_unreachable();                                           \
    } while (0)
#else
#define NETU_ASSUME(expr) assert(expr)
#endif

#endif // NETU_DETAIL_ASSUME_HPP
