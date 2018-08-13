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

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>

namespace netu
{

// TODO: allow users to disable the use of [[nodiscard]] (causes warnings
// with -pedantic in C++14 or lower)
struct [[nodiscard]] upcall_guard{};

template<typename ComposedOp>
struct yield_token
{
    ComposedOp& op_;
    bool is_continuation = false; // TODO: leverage the coro state for this?

    template<typename... Args>
    upcall_guard post_upcall(Args&&... args);

    template<typename... Args>
    upcall_guard upcall(Args&&... args);
};

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
    using completion_handler_type = ComposedOp;

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

// TODO: don't use stuff from detail?
#define NETU_REENTER(c)                                                        \
    switch (::boost::asio::detail::coroutine_ref _coro_value = c.op_.coro_)    \
    case -1:                                                                   \
        if (_coro_value)                                                       \
        {                                                                      \
            default:                                                           \
                __builtin_unreachable();                                       \
        }                                                                      \
        else /* fall-through */                                                \
        case 0:

#define NETU_YIELD_IMPL(n)                                                     \
    for (_coro_value = (n);;)                                                  \
        if (_coro_value == 0)                                                  \
        {                                                                      \
            case (n):;                                                         \
                break;                                                         \
        }                                                                      \
        else

#if defined(_MSC_VER)
#define NETU_YIELD NETU_YIELD_IMPL(__COUNTER__ + 1)
#else // defined(_MSC_VER)
#define NETU_YIELD NETU_YIELD_IMPL(__LINE__)
#endif // defined(_MSC_VER)

#include <netu/impl/composed_operation.hpp>

#endif // NETU_COMPOSED_OPERATION_HPP
