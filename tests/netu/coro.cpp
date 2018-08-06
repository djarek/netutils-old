//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/coro.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/asio/steady_timer.hpp>

namespace netu
{

template<typename T>
bool
check_if_can_complete_immediately(T&)
{
    return false;
}

template<typename CompletionToken>
auto
async_test(boost::asio::steady_timer& timer, CompletionToken&& tok)
{
    return spawn_composed_op<void(boost::system::error_code)>(
      timer,
      std::forward<CompletionToken>(tok),
      [&timer, i = 0](auto& yield_token,
                      boost::system::error_code ec = {}) mutable {
          NETU_REENTER(yield_token)
          {
              if (check_if_can_complete_immediately(timer))
              {
                  yield_token.post_upcall(ec);
              }

              for (i = 0; i < 5; ++i)
              {
                  timer.expires_from_now(std::chrono::seconds{1});
                  NETU_YIELD timer.async_wait(std::move(yield_token));
                  if (ec)
                  {
                      break;
                  }
              }

              yield_token.upcall(ec);
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
} // namespace netu
