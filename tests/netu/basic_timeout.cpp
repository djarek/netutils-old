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
#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>

namespace netu
{

struct basic_timeout_fixture
{
    boost::asio::io_context ctx_;
    basic_timeout<boost::asio::steady_timer> timeout1_{ctx_};
    basic_timeout<boost::asio::steady_timer> timeout2_{ctx_};
};

const auto duration1 = std::chrono::milliseconds{100};
const auto duration2 = std::chrono::milliseconds{300};

BOOST_AUTO_TEST_CASE(timeout_wait_timed_out)
{
    basic_timeout_fixture f;
    f.timeout1_.expires_from_now(duration1);
    f.timeout2_.expires_from_now(duration2);

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

    n = f.ctx_.run_for(2 * duration1);
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 1);
    BOOST_TEST(invoked2 == 0);

    n = f.ctx_.poll();
    BOOST_TEST(n == 0u);
    n = f.ctx_.run_for(2 * duration1);
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 1);
    BOOST_TEST(invoked2 == 1);
}

BOOST_AUTO_TEST_CASE(timeout_wait_timed_out_immediately)
{
    basic_timeout_fixture f;
    f.timeout1_.expires_from_now(-duration1);

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
        f.timeout1_.expires_from_now(duration1);
        f.timeout2_.expires_from_now(duration2 * 2);

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
        n = f.ctx_.poll();
        BOOST_TEST(n > 0u);
        BOOST_TEST(invoked1 == 1);
    }

    BOOST_TEST(invoked2 == 0);
}

BOOST_AUTO_TEST_CASE(timeout_destructor_cancels)
{
    boost::asio::io_context ctx;
    boost::optional<basic_timeout<boost::asio::steady_timer>> t{ctx};
    t->expires_from_now(duration1);

    int invoked = 0;
    t->async_wait([&ctx, &invoked](boost::system::error_code ec) {
        BOOST_TEST(ec == boost::asio::error::operation_aborted);
        ++invoked;
        BOOST_ASSERT(ctx.get_executor().running_in_this_thread());
    });

    boost::asio::post(ctx.get_executor(), [&t]() { t.reset(); });
    ctx.run();

    BOOST_TEST(invoked == 1);
}

BOOST_AUTO_TEST_CASE(timeout_move_assignment_into_active_cancels)
{
    boost::asio::io_context ctx;
    basic_timeout<boost::asio::steady_timer> t{ctx};

    int invoked = 0;
    t.expires_from_now(duration1);
    t.async_wait([&ctx, &invoked](boost::system::error_code ec) {
        BOOST_TEST(ec == boost::asio::error::operation_aborted);
        ++invoked;
        BOOST_ASSERT(ctx.get_executor().running_in_this_thread());
    });

    boost::asio::post(ctx.get_executor(), [&ctx, &t]() {
        t = basic_timeout<boost::asio::steady_timer>{ctx};
    });
    ctx.run();

    BOOST_TEST(invoked == 1);
}

BOOST_AUTO_TEST_CASE(timeout_reset)
{
    basic_timeout_fixture f;

    f.timeout1_.expires_from_now(duration1);
    int invoked1 = 0;
    f.timeout1_.async_wait([&](boost::system::error_code ec) {
        BOOST_TEST(ec != boost::asio::error::operation_aborted);
        ++invoked1;
        BOOST_ASSERT(f.ctx_.get_executor().running_in_this_thread());
    });

    auto n = f.ctx_.poll();
    BOOST_TEST(n == 0);
    f.timeout1_.expires_from_now(duration1);
    n = f.ctx_.run_for(duration1 / 2);
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 0);

    f.timeout1_.expires_from_now(duration1);
    n = f.ctx_.run_for(duration1 / 2);
    BOOST_TEST(n > 0u);
    BOOST_TEST(invoked1 == 0);
}

} // namespace netu
