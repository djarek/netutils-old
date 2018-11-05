//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_IMPL_SYNCHRONIZED_VALUE_HPP
#define NETU_IMPL_SYNCHRONIZED_VALUE_HPP

#include <netu/synchronized_value.hpp>

namespace netu
{

namespace detail
{

template<typename Lockable>
class adopting_lock_guard
{
public:
    explicit adopting_lock_guard(Lockable& l) noexcept
      : guard_{l, std::adopt_lock}
    {
    }

private:
    std::lock_guard<Lockable> guard_;
};

template<typename Lockable>
auto
lock(Lockable& l) -> Lockable&
{
    l.lock();
    return l;
}

template<typename Lockable1, typename Lockable2, typename... Lockables>
auto
lock(Lockable1& l1, Lockable2& l2, Lockables&... ls)
  -> decltype(std::tie(l1, l2, ls...))
{
    std::lock(l1, l2, ls...);
    return std::tie(l1, l2, ls...);
}

template<typename... Lockables>
class scoped_lock
{
public:
    explicit scoped_lock(Lockables&... ls)
      : guards_{detail::lock(ls...)}
    {
    }

private:
    std::tuple<detail::adopting_lock_guard<Lockables>...> guards_;
};

} // namespace detail

template<typename Callable, typename... Ts, typename... Lockables>
auto
apply(Callable&& f, synchronized_value<Ts, Lockables>&... svs)
  -> decltype(std::forward<Callable>(f)(svs.value_...))
{
    detail::scoped_lock<Lockables...> guard{svs.mutex_...};
    return std::forward<Callable>(f)(svs.value_...);
}

template<typename Callable, typename... Ts, typename... Lockables>
auto
apply(Callable&& f, synchronized_value<Ts, Lockables> const&... svs)
  -> decltype(std::forward<Callable>(f)(svs.value_...))
{
    detail::scoped_lock<Lockables...> guard{svs.mutex_...};
    return std::forward<Callable>(f)(svs.value_...);
}

template<typename Callable, typename SynchronizedValue>
class synchronized_function
{
    Callable func_;
    SynchronizedValue& sv_;

public:
    template<typename U>
    explicit synchronized_function(U&& f, SynchronizedValue& sv)
      : func_{std::forward<U>(f)}
      , sv_{sv}
    {
    }

    template<typename... Args>
    auto operator()(Args&&... args) -> decltype(
      this->func_(std::declval<typename SynchronizedValue::value_type&>(),
                  std::forward<Args>(args)...))
    {
        return apply(
          [this, &args...](typename SynchronizedValue::value_type& t) {
              func_(t, std::forward<Args>(args)...);
          },
          sv_);
    }
};

template<typename Callable, typename T, typename Lockable>
auto
synchronize(Callable&& func, synchronized_value<T, Lockable>& sv)
  -> synchronized_function<typename std::decay<Callable>::type,
                           synchronized_value<T, Lockable>>
{
    return synchronized_function<typename std::decay<Callable>::type,
                                 synchronized_value<T, Lockable>>{
      std::forward<Callable>(func), sv};
}

template<typename Callable, typename T, typename Lockable>
auto
synchronize(Callable&& func, synchronized_value<T, Lockable> const& sv)
  -> synchronized_function<typename std::decay<Callable>::type,
                           const synchronized_value<T, Lockable>>
{
    return synchronized_function<typename std::decay<Callable>::type,
                                 const synchronized_value<T, Lockable>>{
      std::forward<Callable>(func), sv};
}

} // namespace netu

#endif // NETU_SYNCHRONIZED_VALUE_HPP
