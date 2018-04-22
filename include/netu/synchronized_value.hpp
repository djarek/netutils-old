//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_SYNCHRONIZED_VALUE_HPP
#define NETU_SYNCHRONIZED_VALUE_HPP

#include <mutex>
#include <utility>

namespace netu
{

template<typename T, typename Lockable = std::mutex>
class synchronized_value
{
public:
    using value_type = T;
    using mutex_type = Lockable;

    synchronized_value() = default;

    template<typename Arg1, typename... Args>
    explicit synchronized_value(Arg1&& arg1, Args&&... args)
      : value_{std::forward<Arg1>(arg1), std::forward<Args>(args)...}
    {
    }

    synchronized_value(synchronized_value&&) = delete;
    synchronized_value(synchronized_value const&) = delete;

    synchronized_value& operator=(synchronized_value&&) = delete;
    synchronized_value& operator=(synchronized_value const&) = delete;

    ~synchronized_value() = default;

    template<typename Callable, typename... Ts, typename... Lockables>
    friend auto apply(Callable&& f, synchronized_value<Ts, Lockables>&... svs)
      -> decltype(std::forward<Callable>(f)(svs.value_...));

    template<typename Callable, typename... Ts, typename... Lockables>
    friend auto apply(Callable&& f,
                      synchronized_value<Ts, Lockables> const&... svs)
      -> decltype(std::forward<Callable>(f)(svs.value_...));

private:
    T value_;
    mutable Lockable mutex_;
};

} // namespace netu

#include <netu/impl/synchronized_value.hpp>

#endif // NETU_SYNCHRONIZED_VALUE_HPP
