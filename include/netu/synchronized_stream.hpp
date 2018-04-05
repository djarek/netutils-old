//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_SYNCHRONIZED_STREAM_HPP
#define NETU_SYNCHRONIZED_STREAM_HPP

#include <netu/detail/async_utils.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

namespace netu
{

template<typename NextLayer,
         typename Executor =
           boost::asio::strand<typename NextLayer::executor_type>>
class synchronized_stream
{
public:
    using next_layer_type = NextLayer;
    using lowest_layer_type = typename NextLayer::lowest_layer_type;
    using executor_type = detail::executor_from_context_t<Executor>;


    explicit synchronized_stream(boost::asio::io_context& ctx);

    template<typename... StreamArgs, typename... ExecutorArgs>
    synchronized_stream(std::piecewise_construct_t,
                        std::tuple<StreamArgs...> sa,
                        std::tuple<ExecutorArgs...> ea);

    template<typename StreamArg, typename ExecutorArg>
    synchronized_stream(StreamArg&& sa, ExecutorArg&& ea);

    template<typename MutableBuffers, typename CompletionToken>
    auto async_read_some(MutableBuffers&& b, CompletionToken&& tok)
      -> detail::io_completion_result_t<CompletionToken>;

    template<typename ConstBuffers, typename CompletionToken>
    auto async_write_some(ConstBuffers&& b, CompletionToken&& tok)
      -> detail::io_completion_result_t<CompletionToken>;

    executor_type get_executor() noexcept
    {
        return detail::get_executor_from_context(p_.second);
    }

    lowest_layer_type& lowest_layer()
    {
        return next_layer().lowest_layer();
    }

    lowest_layer_type const& lowest_layer() const
    {
        return next_layer().lowest_layer();
    }

    next_layer_type& next_layer()
    {
        return p_.first;
    }

    next_layer_type const& next_layer() const
    {
        return p_.first;
    }

private:
    template<typename CompletionHandler>
    struct io_op;

    std::pair<next_layer_type, executor_type> p_;
};

} // namespace netu

#include <netu/impl/synchronized_stream.hpp>

#endif // NETU_SYNCHRONIZED_STREAM_HPP
