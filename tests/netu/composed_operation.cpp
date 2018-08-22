//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/composed_operation.hpp>
#include <netu/coroutine.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>

namespace netu
{

// template<typename CompletionToken>
// auto
// async_test(boost::asio::steady_timer& timer,
//            std::chrono::milliseconds duration,
//            int i,
//            CompletionToken&& tok)
// {
//     netu::coroutine coro_state;
//     return run_composed_op<void(boost::system::error_code)>(
//       timer,
//       std::forward<CompletionToken>(tok),
//       [&timer, duration, i, coro_state](
//         auto yield_token, boost::system::error_code ec = {}) mutable {
//           NETU_REENTER(coro_state)
//           {
//               if (i == 0)
//                   NETU_RETURN std::move(yield_token).post_upcall(ec);

//               for (i = 0; i < 5; ++i)
//               {
//                   timer.expires_from_now(duration);
//                   NETU_YIELD timer.async_wait(std::move(yield_token));
//                   if (ec)
//                   {
//                       break;
//                   }
//               }

//               NETU_RETURN std::move(yield_token).upcall(ec);
//           }
//       });
// }

struct async_test_noncopyable
{

    template<typename TimerType>
    struct noncopyable_op
    {
        noncopyable_op(noncopyable_op const&) = delete;
        noncopyable_op(noncopyable_op&&) = delete;

        template<typename YieldToken>
        upcall_guard operator()(YieldToken&& yield_token,
                                boost::system::error_code ec = {})
        {
            NETU_REENTER(coro_state_)
            {
                if (i_ == 0)
                    NETU_RETURN std::move(yield_token).upcall(ec);

                for (i_ = 0; i_ < 5; ++i_)
                {
                    timer_.expires_from_now(duration_);
                    NETU_YIELD timer_.async_wait(std::move(yield_token));
                    if (ec)
                        NETU_RETURN std::move(yield_token).direct_upcall(ec);
                }
                NETU_RETURN std::move(yield_token).direct_upcall(ec);
            }
        }

        TimerType& timer_;
        std::chrono::milliseconds duration_;
        int i_;
        netu::coroutine coro_state_{};
    };

    template<typename TimerType, typename CompletionToken>
    auto operator()(TimerType& timer,
                    std::chrono::milliseconds d,
                    int i,
                    CompletionToken&& tok)
      -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                       void(boost::system::error_code))
    {
        return run_stable_composed_op<void(boost::system::error_code),
                                      noncopyable_op<TimerType>>(
          timer,
          std::forward<CompletionToken>(tok),
          std::piecewise_construct,
          timer,
          d,
          i);
    }
};

struct async_test_tag_dispatch
{
    template<typename TimerType>
    struct tag_dispatch_op
    {
        struct timer_tag_t
        {
        };

        template<typename YieldToken>
        upcall_guard operator()(YieldToken yield_token)
        {
            return (*this)(std::move(yield_token),
                           timer_tag_t{},
                           boost::system::error_code{});
        }

        template<typename YieldToken>
        upcall_guard operator()(YieldToken yield_token,
                                timer_tag_t,
                                boost::system::error_code ec)
        {
            if (i_-- <= 0)
            {
                return std::move(yield_token).upcall(ec);
            }

            timer_.expires_from_now(duration_);
            return timer_.async_wait(bind_token(
              std::move(yield_token), timer_tag_t{}, std::placeholders::_1));
        }

        TimerType& timer_;
        std::chrono::milliseconds duration_;
        int i_;
    };

    template<typename TimerType, typename CompletionToken>
    auto operator()(TimerType& timer,
                    std::chrono::milliseconds d,
                    int i,
                    CompletionToken&& tok)
      -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                       void(boost::system::error_code))
    {
        return run_composed_op<void(boost::system::error_code),
                               tag_dispatch_op<TimerType>>(
          timer,
          std::forward<CompletionToken>(tok),
          std::piecewise_construct,
          timer,
          d,
          i);
    }
};

using op_type_list =
  boost::mpl::list<async_test_noncopyable, async_test_tag_dispatch>;

BOOST_AUTO_TEST_CASE_TEMPLATE(composed_operation, AsyncOpType, op_type_list)
{
    for (int i = 0; i < 2; ++i)
    {
        boost::asio::io_context ctx;
        boost::asio::steady_timer timer{ctx};
        int invoked = 0;
        boost::system::error_code ec;
        AsyncOpType async_op;

        async_op(timer,
                 std::chrono::milliseconds{1},
                 i,
                 [&invoked, &ec](boost::system::error_code ec_arg) {
                     ec = ec_arg;
                     ++invoked;
                 });

        ctx.run();

        BOOST_TEST(invoked == 1);
        BOOST_TEST(!ec);
    }
}

} // namespace netu
