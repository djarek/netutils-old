//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_TYPE_TRAITS_HPP
#define NETU_DETAIL_TYPE_TRAITS_HPP

#include <type_traits>
#include <utility>

namespace netu
{
namespace detail
{

template<typename T>
T&
move_if_not_ref(T& t, std::true_type)
{
    return t;
}

template<typename T>
T&&
move_if_not_ref(T&& t, std::false_type)
{
    return std::move(t);
}

template<class T, class U = T>
T
exchange(T& obj, U&& val)
{
    T old = std::move(obj);
    obj = std::forward<U>(val);
    return old;
}

template<typename From, typename To>
using disable_conversion_t = typename std::enable_if<
  !std::is_same<To, typename std::decay<From>::type>::value>::type;

} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_TYPE_TRAITS_HPP
