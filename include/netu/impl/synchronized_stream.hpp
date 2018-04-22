//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/synchronized_stream.hpp>

namespace netu
{
template<typename NextLayer, typename Executor>
template<typename... StreamArgs, typename... ExecutorArgs>
synchronized_stream<NextLayer, Executor>::synchronized_stream(
  std::piecewise_construct_t,
  std::tuple<StreamArgs...> sa,
  std::tuple<ExecutorArgs...> ea)
  : p_(std::piecewise_construct, std::move(sa), std::move(ea))
{
}
template<typename NextLayer, typename Executor>
template<typename StreamArg, typename ExecutorArg>
synchronized_stream<NextLayer, Executor>::synchronized_stream(StreamArg&& sa,
                                                              ExecutorArg&& ea)
  : p_{std::forward<StreamArg>(sa), std::forward<ExecutorArg>(ea)}
{
}
template<typename NextLayer, typename Executor>
synchronized_stream<NextLayer, Executor>::synchronized_stream(
  boost::asio::io_context& ctx)
  : p_{ctx, ctx.get_executor()}
{
}

template<typename NextLayer, typename Executor>
template<typename CompletionHandler>
class synchronized_stream<NextLayer, Executor>::io_op
{
public:
    using executor_type = typename synchronized_stream::executor_type;
    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    static_assert(!detail::has_executor<CompletionHandler>::value,
                  "CompletionHandler has an associated Executor.");

    io_op(synchronized_stream& s, CompletionHandler&& h)
      : stream_{s}
      , handler_{std::move(h)}
    {
    }

    executor_type get_executor() const noexcept
    {
        return stream_.get_executor();
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        handler_(std::forward<Args>(args)...);
    }

private:
    synchronized_stream& stream_;
    CompletionHandler handler_;
};

template<typename NextLayer, typename Executor>
template<typename MutableBuffers, typename CompletionToken>
auto
synchronized_stream<NextLayer, Executor>::async_read_some(MutableBuffers&& b,
                                                          CompletionToken&& tok)
  -> detail::io_completion_result_t<CompletionToken>
{
    detail::io_completion_t<CompletionToken> init{tok};
    using ch_t = typename decltype(init)::completion_handler_type;

    io_op<ch_t> op{*this, std::move(init.completion_handler)};
    next_layer().async_read_some(std::forward<MutableBuffers>(b),
                                 std::move(op));
    return init.result.get();
}

template<typename NextLayer, typename Executor>
template<typename ConstBuffers, typename CompletionToken>
auto
synchronized_stream<NextLayer, Executor>::async_write_some(
  ConstBuffers&& b,
  CompletionToken&& tok) -> detail::io_completion_result_t<CompletionToken>
{
    detail::io_completion_t<CompletionToken> init{tok};
    using ch_t = typename decltype(init)::completion_handler_type;

    io_op<ch_t> op{*this, std::move(init.completion_handler)};
    next_layer().async_write_some(std::forward<ConstBuffers>(b), std::move(op));
    return init.result.get();
}

} // namespace netu
