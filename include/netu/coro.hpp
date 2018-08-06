//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//
#pragma once

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/core/bind_handler.hpp>

namespace netu
{
namespace detail
{
template<typename CoroutineBody,
         typename CompletionHandler,
         typename IoObjectExecutor>
struct coroutine_op
{
    using executor_type =
      boost::asio::associated_executor_t<CompletionHandler, IoObjectExecutor>;

    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    coroutine_op(CoroutineBody&& cb,
                 CompletionHandler&& ch,
                 const IoObjectExecutor& e)
      : upcall_{std::move(ch)}
      , resume_{std::move(cb)}
      , io_object_work_guard_{e}

    {
    }

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
          upcall_, io_object_work_guard_.get_executor());
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(resume_);
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        resume_(*this, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void post_upcall(Args&&... args)
    {
        return boost::asio::post(
          get_executor(),
          boost::beast::bind_handler(std::move(upcall_),
                                     std::forward<Args>(args)...));
    }

    template<typename... Args>
    void upcall(Args&&... args)
    {
        upcall_(std::forward<Args>(args)...);
    }

    boost::asio::coroutine coro_;

private:
    CompletionHandler upcall_;
    CoroutineBody resume_;
    boost::asio::executor_work_guard<IoObjectExecutor> io_object_work_guard_;
};
} // namespace detail

#define NETU_REENTER(token) BOOST_ASIO_CORO_REENTER(token.coro_)
#define NETU_YIELD BOOST_ASIO_CORO_YIELD

template<typename Signature,
         typename CompletionToken,
         typename IoObject,
         typename CoroutineBody>
decltype(auto)
spawn_composed_op(IoObject& iob, CompletionToken&& tok, CoroutineBody&& cb)
{
    boost::asio::async_completion<CompletionToken, Signature> init{tok};

    auto ex = boost::asio::get_associated_executor(init.completion_handler,
                                                   iob.get_executor());
    detail::coroutine_op<typename std::decay<CoroutineBody>::type,
                         typename decltype(init)::completion_handler_type,
                         decltype(ex)>
      op{std::forward<CoroutineBody>(cb),
         std::move(init.completion_handler),
         std::move(ex)};
    op();
    return init.result.get();
}

} // namespace netu
