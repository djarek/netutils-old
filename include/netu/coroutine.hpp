//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_COROUTINE_HPP
#define NETU_COROUTINE_HPP

namespace netu
{

namespace detail
{
class coroutine_ref;
} // namespace detail

class coroutine
{
public:
    bool is_complete() const
    {
        return state_ < 0;
    }

    bool is_continuation() const
    {
        return state_ > 0;
    }

    friend detail::coroutine_ref;

private:
    int state_ = 0;
};

#define NETU_REENTER(c)                                                        \
    switch (::netu::detail::coroutine_ref _coro_value{c})                      \
    case -1:                                                                   \
        if (_coro_value)                                                       \
        {                                                                      \
            default:                                                           \
                BOOST_ASSERT(false && "Corrupt coro state.");                  \
                __builtin_unreachable();                                       \
        }                                                                      \
        else /* fall-through */                                                \
        case 0:

#define NETU_YIELD_IMPL(n)                                                     \
    for (_coro_value = (n);;)                                                  \
        /* fallthrough */                                                      \
        if (false)                                                             \
        {                                                                      \
            static_assert((n) <= std::numeric_limits<int>::max(),              \
                          "Label index exceeded maximal value.");              \
            case (n):;                                                         \
                break;                                                         \
        }                                                                      \
        else                                                                   \
            return

#define NETU_YIELD NETU_YIELD_IMPL(__LINE__)

#define NETU_RETURN                                                            \
    for (_coro_value.release();;)                                              \
    return

} // namespace netu

#include <netu/detail/coroutine.hpp>

#endif // NETU_COROUTINE_HPP
