//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/completion_handler.hpp>

#include <netu/basic_timeout.hpp>

#include <boost/asio/post.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/test/unit_test.hpp>

namespace netu
{

class fake_clock
{
public:
    using inner_clock = std::chrono::steady_clock;
    using rep = inner_clock::rep;
    using period = inner_clock::period;

    using duration = inner_clock::duration;
    using time_point = inner_clock::time_point;

    static time_point now()
    {
        return current_time += std::chrono::seconds{1};
    }

private:
    static time_point current_time;
};

fake_clock::time_point fake_clock::current_time =
  std::chrono::steady_clock::now();

class fake_timer
{
public:
    using clock_type = fake_clock;
    using duration = clock_type::duration;
    using executor_type = boost::asio::io_context::executor_type;
    using time_point = clock_type::time_point;
    using traits_type = boost::asio::wait_traits<clock_type>;

    explicit fake_timer(boost::asio::io_context& ctx)
      : ctx_{ctx}
    {
    }

    executor_type get_executor() noexcept
    {
        return ctx_.get_executor();
    }

    template<typename CompletionToken>
    auto async_wait(CompletionToken&& tok)
      -> detail::wait_completion_result_t<CompletionToken>
    {
        detail::wait_completion_t<CompletionToken> init{tok};
        using ch_t = typename decltype(init)::completion_handler_type;
        auto op = detail::suspended_handler<ch_t, executor_type>{
          std::move(init.completion_handler), get_executor()};
        if (expiry() < clock_type::now())
        {
            op(boost::system::error_code{});
        }
        else
        {
            wait_handler_ = std::move(op);
        }

        return init.result.get();
    }

    std::size_t expires_from_now(duration d)
    {
        expiration_time_ = clock_type::now() + d;
        return cancel();
    }

    std::size_t expires_at(time_point tp)
    {
        expiration_time_ = tp;
        return cancel();
    }

    std::size_t cancel()
    {
        if (wait_handler_)
        {
            wait_handler_.invoke(boost::asio::error::operation_aborted);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    time_point expiry() const
    {
        return expiration_time_;
    }

    std::size_t force_expiration()
    {
        if (wait_handler_)
        {
            wait_handler_.invoke(boost::system::error_code{});
            return 1;
        }
        else
        {
            return 0;
        }
    }

private:
    boost::asio::io_context& ctx_;
    completion_handler<void(boost::system::error_code)> wait_handler_;
    time_point expiration_time_;
};

struct basic_timeout_fixture
{
    boost::asio::io_context ctx_;
    basic_timeout<fake_timer> timeout1_{ctx_};
    basic_timeout<fake_timer> timeout2_{ctx_};
};

const auto timeout1 = std::chrono::seconds{3};
const auto timeout2 = std::chrono::seconds{4};

BOOST_AUTO_TEST_CASE(timeout_wait_timed_out)
{
    basic_timeout_fixture f;
    f.timeout1_.expires_from_now(timeout1);
    f.timeout2_.expires_from_now(timeout2);

    int invoked1 = 0;
    int invoked2 = 0;
    f.timeout1_.async_wait([&](boost::system::error_code ec) {
        ++invoked1;
        BOOST_TEST(!ec);
        BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
    });

    f.timeout2_.async_wait([&](boost::system::error_code ec) {
        ++invoked2;
        BOOST_TEST(!ec);
        BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
    });

    auto n = f.ctx_.poll();
    BOOST_TEST(n == 0u);
    // f.timeout1_.get_timer().force_expiration();
    n = f.ctx_.poll();
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 1);
    BOOST_TEST(invoked2 == 0);

    n = f.ctx_.poll();
    BOOST_TEST(n == 0u);
    // f.timeout2_.get_timer().force_expiration();
    n = f.ctx_.poll();
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 1);
    BOOST_TEST(invoked2 == 1);
}

BOOST_AUTO_TEST_CASE(timeout_wait_timed_out_immediately)
{
    basic_timeout_fixture f;
    f.timeout1_.expires_from_now(-timeout1);

    int invoked = 0;
    f.timeout1_.async_wait([&](boost::system::error_code ec) {
        ++invoked;
        BOOST_TEST(!ec);
        BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
    });
    f.ctx_.poll();
    BOOST_TEST(invoked == 1);
}

BOOST_AUTO_TEST_CASE(timeout_wait_cancelled)
{
    int invoked2 = 0;
    {
        basic_timeout_fixture f;
        f.timeout1_.expires_from_now(timeout1);
        f.timeout2_.expires_from_now(timeout2 * 2);

        int invoked1 = 0;
        f.timeout1_.async_wait([&](boost::system::error_code ec) {
            BOOST_TEST(ec == boost::asio::error::operation_aborted);
            ++invoked1;
            BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
        });

        f.timeout2_.async_wait([&](boost::system::error_code ec) {
            BOOST_TEST(ec == boost::asio::error::operation_aborted);
            ++invoked2;
            BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
        });

        auto n = f.ctx_.poll();
        BOOST_TEST(n == 0u);
        BOOST_TEST(f.timeout1_.cancel());
        // f.timeout1_.get_timer().force_expiration();
        n = f.ctx_.poll();
        BOOST_TEST(n > 0u);
        BOOST_TEST(invoked1 == 1);
    }

    BOOST_TEST(invoked2 == 0);
}

BOOST_AUTO_TEST_CASE(timeout_reset)
{
    basic_timeout_fixture f;

    f.timeout1_.expires_from_now(timeout1);
    int invoked1 = 0;
    f.timeout1_.async_wait([&](boost::system::error_code ec) {
        BOOST_TEST(ec != boost::asio::error::operation_aborted);
        ++invoked1;
        BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
    });

    auto n = f.ctx_.poll();
    BOOST_TEST(n == 0);
    f.timeout1_.expires_from_now(timeout1);
    n = f.ctx_.poll();
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 0);

    f.timeout1_.expires_from_now(timeout1);
    n = f.ctx_.poll();
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 0);
}

} // namespace netu
