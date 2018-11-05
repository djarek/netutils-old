//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_ASYNC_UTILS_HPP
#define NETU_DETAIL_ASYNC_UTILS_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/is_executor.hpp>
#include <boost/system/error_code.hpp>

#include <type_traits>

namespace netu
{
namespace detail
{

template<typename CompletionToken>
using io_completion_t =
  boost::asio::async_completion<CompletionToken,
                                void(boost::system::error_code, std::size_t)>;

template<typename CompletionToken>
using io_completion_result_t =
  BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                void(boost::system::error_code, std::size_t));

template<typename CompletionToken>
using io_completion_handler_t =
  typename io_completion_result_t<CompletionToken>::completion_handler_type;

template<typename CompletionToken>
using wait_completion_t =
  boost::asio::async_completion<CompletionToken,
                                void(boost::system::error_code)>;

template<typename CompletionToken>
using wait_completion_result_t =
  BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                void(boost::system::error_code));

template<typename CompletionToken>
using wait_completion_handler_t =
  typename wait_completion_t<CompletionToken>::completion_handler_type;

template<typename T, typename = void>
struct has_executor : std::false_type
{
};

template<typename T>
struct has_executor<T, decltype(std::declval<T&>().get_executor())>
  : std::true_type
{
};

template<typename T>
auto
get_executor_from_context(T& t, std::true_type) -> T
{
    return t;
}

template<typename T>
auto
get_executor_from_context(T& t, std::false_type) -> typename T::executor_type
{
    return t.get_executor();
}

template<typename T>
auto
get_executor_from_context(T& t) -> decltype(
  netu::detail::get_executor_from_context(t, boost::asio::is_executor<T>{}))
{
    return get_executor_from_context(t, boost::asio::is_executor<T>{});
}

template<typename T>
using executor_from_context_t =
  decltype(netu::detail::get_executor_from_context(std::declval<T&>()));

} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_ASYNC_UTILS_HPP
