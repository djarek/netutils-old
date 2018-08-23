//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_IMPL_COMPOSED_OPERATION_HPP
#define NETU_IMPL_COMPOSED_OPERATION_HPP

#include <netu/composed_operation.hpp>

namespace netu
{

template<typename ComposedOp>
template<typename... Args>
upcall_guard
yield_token<ComposedOp>::post_upcall(Args&&... args) &&
{
    return op_.post_upcall(std::forward<Args>(args)...);
}

template<typename ComposedOp>
template<typename... Args>
upcall_guard
yield_token<ComposedOp>::direct_upcall(Args&&... args) &&
{
    BOOST_ASSERT(is_continuation_ && "Direct upcall can only be used in a "
                                     "continuation. Use post_upcall instead.");
    BOOST_ASSERT(detail::running_in_this_thread(op_.get_executor(), nullptr) &&
                 "Direct upcall must not be performed outside of the "
                 "CompletionHandler's Executor context.");
    return op_.upcall(std::forward<Args>(args)...);
}

template<typename ComposedOp>
template<typename... Args>
upcall_guard
yield_token<ComposedOp>::upcall(Args&&... args) &&
{
    if (is_continuation_)
        return std::move(*this).direct_upcall(std::forward<Args>(args)...);
    else
        return std::move(*this).post_upcall(std::forward<Args>(args)...);
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
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    return detail::run_composed_op<Signature, OperationBody>(
      iob,
      std::forward<CompletionToken>(tok),
      std::is_move_constructible<OperationBody>{},
      std::forward<Args>(args)...);
}

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken>
auto
run_composed_op(IoObject& iob, CompletionToken&& tok, OperationBody&& cb)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    using op_body_t = typename std::decay<OperationBody>::type;
    return detail::run_composed_op<Signature, op_body_t>(
      iob,
      std::forward<CompletionToken>(tok),
      std::is_move_constructible<OperationBody>{},
      std::forward<OperationBody>(cb));
}
template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken,
         typename... Args>
auto
run_stable_composed_op(IoObject& iob,
                       CompletionToken&& tok,
                       std::piecewise_construct_t,
                       Args&&... args)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    return detail::run_composed_op<Signature, OperationBody>(
      iob,
      std::forward<CompletionToken>(tok),
      std::false_type{},
      std::forward<Args>(args)...);
}

template<typename Signature,
         typename OperationBody,
         typename IoObject,
         typename CompletionToken>
auto
run_stable_composed_op(IoObject& iob, CompletionToken&& tok, OperationBody&& cb)
  -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, Signature)
{
    using op_body_t = typename std::decay<OperationBody>::type;
    return detail::run_composed_op<Signature, op_body_t>(
      iob,
      std::forward<CompletionToken>(tok),
      std::false_type{},
      std::forward<OperationBody>(cb));
}

} // namespace netu

#endif // NETU_IMPL_COMPOSED_OPERATION_HPP
