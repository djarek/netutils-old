//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/composed_operation.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/asio/steady_timer.hpp>

namespace netu
{

template<typename T>
bool
check_if_can_complete_immediately(T&)
{
    return true;
}

template<typename CompletionToken>
auto
async_test(boost::asio::steady_timer& timer, CompletionToken&& tok)
{
    return run_composed_op<void(boost::system::error_code)>(
      timer,
      std::forward<CompletionToken>(tok),
      [&timer, i = 0](auto yield_token,
                      boost::system::error_code ec = {}) mutable {
          NETU_REENTER(yield_token)
          {
              if (check_if_can_complete_immediately(timer))
                  return std::move(yield_token).post_upcall(ec);

              for (i = 0; i < 5; ++i)
              {
                  timer.expires_from_now(std::chrono::seconds{1});
                  NETU_YIELD return timer.async_wait(std::move(yield_token));
                  if (ec)
                  {
                      break;
                  }
              }

              return std::move(yield_token).upcall(ec);
          }
      });
}

template<typename TimerType>
struct noncopyable_op : boost::noncopyable
{
    template<typename YieldToken>
    upcall_guard operator()(YieldToken yield_token,
                            boost::system::error_code ec = {})
    {
        NETU_REENTER(yield_token)
        {
            if (check_if_can_complete_immediately(timer_))
                return std::move(yield_token).post_upcall(ec);

            for (i_ = 0; i_ < 5; ++i_)
            {
                timer_.expires_from_now(duration_);
                NETU_YIELD return timer_.async_wait(std::move(yield_token));
                if (ec)
                    return yield_token.upcall(ec);
            }
            return std::move(yield_token).upcall(ec);
        }
    }

    TimerType& timer_;
    std::chrono::seconds duration_;
    int i_ = 0;
};

template<typename TimerType, typename CompletionToken>
auto
async_test3(TimerType& timer, std::chrono::seconds d, CompletionToken&& tok)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                   void(boost::system::error_code))
{
    return run_composed_op<void(boost::system::error_code),
                           noncopyable_op<TimerType>>(
      timer,
      std::forward<CompletionToken>(tok),
      std::piecewise_construct,
      timer,
      d);
}

template<typename TimerType, typename CompletionToken>
auto
async_test2(TimerType& timer, std::chrono::seconds d, CompletionToken&& tok)
{
    return run_composed_op<void(boost::system::error_code)>(
      timer,
      std::forward<CompletionToken>(tok),
      [&timer, duration = d, i = 0](auto yield_token,
                                    boost::system::error_code ec = {}) mutable {
          NETU_REENTER(yield_token)
          {
              if (check_if_can_complete_immediately(timer))
                  return std::move(yield_token).post_upcall(ec);

              for (i = 0; i < 5; ++i)
              {
                  timer.expires_from_now(duration);
                  NETU_YIELD return timer.async_wait(std::move(yield_token));
                  if (ec)
                      break;
              }

              return std::move(yield_token).upcall(ec);
          }
      });
}

BOOST_AUTO_TEST_CASE(spawn_coro)
{
    boost::asio::io_context ctx;
    boost::asio::steady_timer timer{ctx};
    int invoked = 0;
    boost::system::error_code ec;
    async_test(timer, [&invoked, &ec](boost::system::error_code ec_arg) {
        ec = ec_arg;
        ++invoked;
    });

    ctx.run();

    BOOST_TEST(invoked == 1);
    BOOST_TEST(!ec);
}

BOOST_AUTO_TEST_CASE(spawn_stable_coro)
{
    boost::asio::io_context ctx;
    boost::asio::steady_timer timer{ctx};
    int invoked = 0;
    boost::system::error_code ec;
    async_test2(timer,
                std::chrono::seconds{1},
                [&invoked, &ec](boost::system::error_code ec_arg) {
                    ec = ec_arg;
                    ++invoked;
                });

    ctx.run();

    BOOST_TEST(invoked == 1);
    BOOST_TEST(!ec);
}
} // namespace netu
