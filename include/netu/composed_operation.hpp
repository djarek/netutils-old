//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_COMPOSED_OPERATION_HPP
#define NETU_COMPOSED_OPERATION_HPP

#include <netu/detail/composed_operation.hpp>

namespace netu
{

template<typename ComposedOp>
class yield_token
{
public:
    template<typename DeducedOp>
    yield_token(DeducedOp&& op, bool is_continuation)
      : op_{std::forward<DeducedOp>(op)}
      , is_continuation_{is_continuation}

    {
    }

    auto release_operation() && -> typename std::decay<ComposedOp>::type
    {
        return std::move(op_);
    }

    template<typename... Args>
    upcall_guard post_upcall(Args&&... args) &&;

    template<typename... Args>
    upcall_guard direct_upcall(Args&&... args) &&;

    template<typename... Args>
    upcall_guard upcall(Args&&... args) &&;

    bool is_continuation() const
    {
        return is_continuation_;
    }

private:
    ComposedOp op_;
    bool is_continuation_;
};

template<typename T>
struct bound_handler
{
    using executor_type = boost::asio::associated_executor_t<T>;
    using allocator_type = boost::asio::associated_allocator_t<T>;

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(t_);
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(t_);
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        return t_(std::forward<Args>(args)...);
    }

    T t_;
};

template<typename ComposedOp, typename... Args>
auto
bind_token(yield_token<ComposedOp>&& token, Args&&... args) -> bound_handler<
  decltype(boost::beast::bind_handler(std::move(token).release_operation(),
                                      std::forward<Args>(args)...))>
{
    return {boost::beast::bind_handler(std::move(token).release_operation(),
                                       std::forward<Args>(args)...)};
}

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken,
         typename... Args>
auto
run_composed_op(IoObject& iob,
                CompletionToken&& tok,
                std::piecewise_construct_t,
                Args&&... args)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature);

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken>
auto
run_composed_op(IoObject& iob, CompletionToken&& tok, OperationBody&& cb)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature);

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken>
auto
run_composed_op(IoObject& iob, CompletionToken&& tok, OperationBody&& cb)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature);

template<typename OperationBody, typename IoObject, typename CompletionHandler>
using yield_token_t = yield_token<
  detail::composed_op<OperationBody,
                      CompletionHandler,
                      decltype(std::declval<IoObject&>().get_executor())>>;

template<typename OperationBody, typename IoObject, typename CompletionHandler>
using stable_yield_token_t = yield_token<detail::stable_composed_op<
  OperationBody,
  CompletionHandler,
  decltype(std::declval<IoObject&>().get_executor())>>;

} // namespace netu

namespace boost
{
namespace asio
{

template<typename ComposedOp, typename Signature>
class async_result<netu::yield_token<ComposedOp>, Signature>
{
public:
    using return_type = netu::upcall_guard;
    using completion_handler_type = typename std::decay<ComposedOp>::type;

    explicit async_result(completion_handler_type&)
    {
    }

    return_type get()
    {
        return {};
    }
};

template<typename Handler, typename Signature>
class async_result<netu::bound_handler<Handler>, Signature>
{
public:
    using return_type = netu::upcall_guard;
    using completion_handler_type = netu::bound_handler<Handler>;

    explicit async_result(completion_handler_type&)
    {
    }

    return_type get()
    {
        return {};
    }
};

} // namespace asio
} // namespace boost

#include <netu/impl/composed_operation.hpp>

#endif // NETU_COMPOSED_OPERATION_HPP
