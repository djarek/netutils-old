//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_BASIC_TIMEOUT_HPP
#define NETU_BASIC_TIMEOUT_HPP

#include <netu/completion_handler.hpp>
#include <netu/detail/async_utils.hpp>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/intrusive/set.hpp>

namespace netu
{

namespace detail
{
template<typename Timer>
class basic_timeout_service;
} // namespace detail

template<typename Timer>
class basic_timeout
  : public boost::asio::basic_io_object<detail::basic_timeout_service<Timer>>
{
public:
    using timer_type = Timer;
    using clock_type = typename Timer::clock_type;
    using duration = typename clock_type::duration;
    using executor_type = typename timer_type::executor_type;
    using time_point = typename clock_type::time_point;
    using traits_type = typename Timer::traits_type;

    explicit basic_timeout(boost::asio::io_context& ctx)
      : boost::asio::basic_io_object<detail::basic_timeout_service<Timer>>{ctx}
    {
    }

    // basic_timeout(boost::asio::io_context& ctx, duration d)
    //   : timer_{ctx, std::move(d)}
    // {
    // }

    // basic_timeout(boost::asio::io_context& ctx, time_point tp)
    //   : timer_{ctx, std::move(tp)}
    // {
    // }

    template<typename CompletionToken>
    auto async_wait(CompletionToken&& tok)
      -> detail::wait_completion_result_t<CompletionToken>

    {
        return this->get_service().async_wait(
          this->get_implementation(), std::forward<CompletionToken>(tok));
    }

    bool expires_from_now(duration d)
    {
        return this->get_service().expires_from_now(this->get_implementation(),
                                                    d);
    }

    bool cancel()
    {
        return this->get_service().cancel(this->get_implementation());
    }

    time_point expiry() const
    {
        return this->get_service().expiry(this->get_implementation());
    }
};

} // namespace netu

#include <netu/impl/basic_timeout.hpp>

#endif // NETU_BASIC_TIMEOUT_HPP
