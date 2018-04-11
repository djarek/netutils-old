//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>

namespace netu
{

namespace detail
{

template<typename CompletionToken>
using io_async_completion_t =
  boost::asio::async_completion<CompletionToken,
                                void(boost::system::error_code, std::size_t)>;

template<typename CompletionToken>
using async_completion_t =
  boost::asio::async_completion<CompletionToken,
                                void(boost::system::error_code, std::size_t)>;

template<typename CompletionToken>
using io_async_completion_result_t =
  BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
void(boost::system::error_code, std::size_t));

template<typename CompletionToken>
using async_completion_result_t =
  BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
void(boost::system::error_code));

} // namespace detail
} // namespace netu