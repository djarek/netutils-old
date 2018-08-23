//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_COMPOSED_OPERATION_HPP
#define NETU_DETAIL_COMPOSED_OPERATION_HPP

#include <netu/composed_operation.hpp>
#include <netu/upcall_guard.hpp>

#include <boost/asio/post.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>

namespace netu
{

template<typename ComposedOp>
class yield_token;

namespace detail
{

template<typename Executor>
auto
running_in_this_thread(Executor const& ex, std::nullptr_t)
  -> decltype(ex.running_in_this_thread())
{
    return ex.running_in_this_thread();
}

template<typename Executor>
bool
running_in_this_thread(Executor const&, ...)
{
    return true;
}

template<typename OperationBody,
         typename CompletionHandler,
         typename IoObjectExecutor>
struct composed_op
{
    template<typename Handler, typename... BodyArgs>
    composed_op(Handler&& h, IoObjectExecutor const& ex, BodyArgs&&... args)
      : upcall_{std::forward<Handler>(h)}
      , resume_{std::forward<BodyArgs>(args)...}
      , io_object_work_guard_{ex}
    {
    }

    template<typename DeducedOp>
    explicit composed_op(yield_token<DeducedOp>&& token)
      : composed_op{std::move(token).release_operation()}
    {
    }

    using executor_type =
      boost::asio::associated_executor_t<CompletionHandler, IoObjectExecutor>;

    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
          upcall_, io_object_work_guard_.get_executor());
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(upcall_);
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        (void)resume_(yield_token<composed_op&>{*this, true},
                      std::forward<Args>(args)...);
    }

    void run()
    {
        (void)resume_(yield_token<composed_op&>{*this, false});
    }

    template<typename... Args>
    upcall_guard post_upcall(Args&&... args)
    {
        auto const ex = io_object_work_guard_.get_executor();
        boost::asio::post(ex,
                          boost::beast::bind_handler(
                            std::move(upcall_), std::forward<Args>(args)...));
        return {};
    }

    template<typename... Args>
    upcall_guard upcall(Args&&... args)
    {
        upcall_(std::forward<Args>(args)...);
        return {};
    }

    CompletionHandler upcall_;
    OperationBody resume_;
    boost::asio::executor_work_guard<IoObjectExecutor> io_object_work_guard_;
};

template<typename OperationBody,
         typename CompletionHandler,
         typename IoObjectExecutor>
struct stable_composed_op
{
    using executor_type =
      boost::asio::associated_executor_t<CompletionHandler, IoObjectExecutor>;

    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    template<typename Handler, typename... BodyArgs>
    stable_composed_op(Handler&& h,
                       IoObjectExecutor const& ex,
                       BodyArgs&&... args)
      : frame_ptr_{std::forward<Handler>(h),
                   ex,
                   std::forward<BodyArgs>(args)...}
    {
    }

    template<typename DeducedOp>
    explicit stable_composed_op(yield_token<DeducedOp>&& token)
      : stable_composed_op{std::move(token).release_operation()}
    {
    }

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
          frame_ptr_.handler(),
          frame_ptr_->io_object_work_guard_.get_executor());
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(frame_ptr_.handler());
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        (void)frame_ptr_->resume_(yield_token<stable_composed_op&>{*this, true},
                                  std::forward<Args>(args)...);
    }

    void run()
    {
        (void)frame_ptr_->resume_(
          yield_token<stable_composed_op&>{*this, false});
    }

    template<typename... Args>
    upcall_guard post_upcall(Args&&... args)
    {
        auto const work_guard = std::move(frame_ptr_->io_object_work_guard_);
        boost::asio::post(
          work_guard.get_executor(),
          boost::beast::bind_handler(frame_ptr_.release_handler(),
                                     std::forward<Args>(args)...));
        return upcall_guard{};
    }

    template<typename... Args>
    upcall_guard upcall(Args&&... args)
    {
        auto const work_guard = std::move(frame_ptr_->io_object_work_guard_);
        frame_ptr_.invoke(std::forward<Args>(args)...);
        return upcall_guard{};
    }

    struct frame
    {
        template<typename... Args>
        frame(CompletionHandler const&,
              IoObjectExecutor const& ex,
              Args&&... args)
          : io_object_work_guard_{ex}
          , resume_{std::forward<Args>(args)...}
        {
        }

        boost::asio::executor_work_guard<IoObjectExecutor>
          io_object_work_guard_;
        OperationBody resume_;
    };

    boost::beast::handler_ptr<frame, CompletionHandler> frame_ptr_;
};

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken,
         typename... Args>
auto
run_composed_op(IoObject& iob,
                CompletionToken&& tok,
                std::true_type,
                Args&&... args)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    boost::asio::async_completion<CompletionToken, Signature> init{tok};

    auto const ex = boost::asio::get_associated_executor(
      init.completion_handler, iob.get_executor());
    using op_t = composed_op<OperationBody,
                             typename decltype(init)::completion_handler_type,
                             decltype(ex)>;
    op_t op{
      std::move(init.completion_handler), ex, std::forward<Args>(args)...};
    op.run();
    return init.result.get();
}

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken,
         typename... Args>
auto
run_composed_op(IoObject& iob,
                CompletionToken&& tok,
                std::false_type,
                Args&&... args)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    boost::asio::async_completion<CompletionToken, Signature> init{tok};

    auto const ex = boost::asio::get_associated_executor(
      init.completion_handler, iob.get_executor());
    using op_t =
      detail::stable_composed_op<typename std::decay<OperationBody>::type,
                                 typename decltype(
                                   init)::completion_handler_type,
                                 decltype(ex)>;
    op_t op{
      std::move(init.completion_handler), ex, std::forward<Args>(args)...};
    op.run();
    return init.result.get();
}

} // namespace detail

} // namespace netu

#endif // NETU_DETAIL_COMPOSED_OPERATION_HPP
