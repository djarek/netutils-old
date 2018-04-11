//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/detail/async_completion.hpp>
#include <netu/completion_handler.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <boost/beast/test/stream.hpp>
#include <boost/test/unit_test.hpp>

namespace netu
{

template<typename AsyncStream, typename Timer = boost::asio::steady_timer>
class timed_read_stream
{
    template<typename CompletionHandler>
    class timed_read_some_op;

    template<typename Executor, typename Allocator>
    class timer_op;

public:
    using next_layer_type = AsyncStream;
    using lowest_layer_type = typename AsyncStream::lowest_layer_type;
    using executor_type = typename AsyncStream::executor_type;
    using duration_type = typename Timer::duration;

    explicit timed_read_stream(boost::asio::io_context& ctx);

    template<typename ConstBuffers, typename CompletionToken>
    auto async_write_some(ConstBuffers&& buffers, CompletionToken&& tok)
      -> detail::io_async_completion_result_t<CompletionToken>;

    template<typename MutableBuffers, typename CompletionToken>
    auto async_read_some(MutableBuffers&& buffers, CompletionToken&& tok)
      -> detail::io_async_completion_result_t<CompletionToken>;

    template<typename Duration, typename CompletionToken>
    auto async_await_timeout(Duration read_timeout, CompletionToken&& tok)
      -> detail::async_completion_result_t<CompletionToken>;

    executor_type get_executor() noexcept
    {
        return stream_.get_executor();
    }

    next_layer_type const& next_layer() const noexcept
    {
        return stream_;
    }

    next_layer_type& next_layer() noexcept
    {
        return stream_;
    }

    lowest_layer_type const& lowest_layer() const noexcept
    {
        return stream_.lowest_layer();
    }

    lowest_layer_type& lowest_layer() noexcept
    {
        return stream_.lowest_layer();
    }

    std::size_t cancel_timer()
    {
        timeout_ = duration_type{};
        return timer_.cancel();
    }

private:
    void reset_timeout()
    {
        if (timeout_ == duration_type{})
        {
            return;
        }

        timer_.expires_from_now(timeout_);
    }

    next_layer_type stream_;
    duration_type timeout_;
    completion_handler<void(boost::system::error_code)> timeout_handler_;
    Timer timer_;
};

template<typename AsyncStream, typename Timer>
template<typename Executor, typename Allocator>
class timed_read_stream<AsyncStream, Timer>::timer_op
{
public:
    using executor_type = Executor;
    using allocator_type = Allocator;

    executor_type get_executor() const noexcept
    {
        return std::get<1>(data_);
    }

    allocator_type get_allocator() const noexcept
    {
        return std::get<2>(data_);
    }

    timer_op(timed_read_stream& s, Executor ex, Allocator a)
      : data_{s, std::move(ex), std::move(a)}
    {
    }

    void operator()()
    {
        auto& s = stream();
        s.timer_.expires_from_now(s.timeout_);
        s.timer_.async_wait(std::move(*this));
    }

    void operator()(boost::system::error_code ec)
    {
        auto& s = stream();
        if (!ec || (ec == boost::asio::error::operation_aborted && s.timeout_ == duration_type{}))
        {
            s.timeout_handler_.invoke(boost::asio::error::timed_out);
            return;
        }

        s.timer_.expires_from_now(s.timeout_);
        s.timer_.async_wait(std::move(*this));
    }

private:
    timed_read_stream& stream()
    {
        return std::get<0>(data_);
    }

    std::tuple<timed_read_stream&, Executor, Allocator> data_;
};

template<typename AsyncStream, typename Timer>
template<typename CompletionHandler>
class timed_read_stream<AsyncStream, Timer>::timed_read_some_op
{
public:
    using executor_type = boost::asio::associated_executor_t<
      CompletionHandler,
      typename timed_read_stream::executor_type>;
    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(handler_,
                                                    stream_.get_executor());
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    timed_read_some_op(timed_read_stream& s, CompletionHandler&& h)
      : stream_{s}
      , handler_{std::forward<CompletionHandler>(h)}
    {
    }

    void operator()(boost::system::error_code ec, std::size_t n)
    {
        stream_.reset_timeout();
        handler_(ec, n);
    }

private:
    timed_read_stream& stream_;
    CompletionHandler handler_;
};

template<typename AsyncStream, typename Timer>
timed_read_stream<AsyncStream, Timer>::timed_read_stream(
  boost::asio::io_context& ctx)
  : stream_{ctx}
  , timer_{ctx}
{
}

template<typename AsyncStream, typename Timer>
template<typename ConstBuffers, typename CompletionToken>
auto
timed_read_stream<AsyncStream, Timer>::async_write_some(ConstBuffers&& buffers,
                                                        CompletionToken&& tok)
  -> detail::io_async_completion_result_t<CompletionToken>
{
    return next_layer().async_write_some(std::forward<ConstBuffers>(buffers),
                                         std::forward<CompletionToken>(tok));
}

template<typename AsyncStream, typename Timer>
template<typename MutableBuffers, typename CompletionToken>
auto
timed_read_stream<AsyncStream, Timer>::async_read_some(MutableBuffers&& buffers,
                                                       CompletionToken&& tok)
  -> detail::io_async_completion_result_t<CompletionToken>
{
    detail::io_async_completion_t<CompletionToken> init{tok};
    using ch_t = typename detail::io_async_completion_t<
      CompletionToken>::completion_handler_type;
    timed_read_some_op<ch_t> op{*this, std::move(init.completion_handler)};
    next_layer().async_read_some(std::forward<MutableBuffers>(buffers),
                                 std::move(op));
    init.result.get();
}

template<typename AsyncStream, typename Timer>
template<typename Duration, typename CompletionToken>
auto
timed_read_stream<AsyncStream, Timer>::async_await_timeout(Duration timeout,
                                                       CompletionToken&& tok)
  -> detail::async_completion_result_t<CompletionToken>
{
    detail::async_completion_t<CompletionToken> init{tok};
    
    timeout_ = timeout;
    auto alloc = boost::asio::get_associated_allocator(init.completion_handler);
    auto ex = boost::asio::get_associated_executor(init.completion_handler, get_executor());
    timeout_handler_ = std::move(init.completion_handler);
    timer_op<decltype(ex), decltype(alloc)> op{*this, ex, alloc};
    op();
    return init.result.get();
}

BOOST_AUTO_TEST_CASE(construction)
{
    // using namespace boost::asio::local;

    boost::asio::io_context ctx;
    netu::timed_read_stream<boost::beast::test::stream> stream{ctx};

    stream.async_await_timeout(std::chrono::seconds{1}, [&stream](boost::system::error_code ec){
        if (ec)
        {
            //stream.next_layer().close();
        }
    });

    std::array<char, 64> arr;

    stream.async_read_some(boost::asio::buffer(arr), [](boost::system::error_code ec, std::size_t n)
    {

    });
    // boost::asio::steady_timer timer{ctx};
    // stream_protocol::socket socket1{ctx};
    // stream_protocol::socket socket2{ctx};
    // boost::asio::local::connect_pair(socket1, socket2);

    // boost::beast::flat_buffer buffer;
    // auto mb = buffer.prepare(1024);

    // async_read_some(socket1,
    //                 timer,
    //                 std::move(mb),
    //                 std::chrono::seconds{1},
    //                 [](boost::system::error_code ec, std::size_t n) {
    //                     printf("Err: %s, %zu\n ", ec.message().c_str(), n);
    //                 });

    // std::string str = "hello";

    //     socket2.write_some(boost::asio::buffer(str));

   // ctx.run();
}

} // namespace netu