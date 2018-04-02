//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <boost/asio/async_result.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/test/unit_test.hpp>
namespace netu
{
namespace detail
{

template<typename AsyncStream, typename Executor, typename Timer>
struct connection_state
{
    using executor_type = Executor;
    using next_layer_type = AsyncStream;
    using lowest_layer_type = typename AsyncStream::lowest_layer_type;

    explicit connection_state(boost::asio::io_context& ctx)
      : stream_{ctx}
      , read_timer_{ctx}
      , write_timer_{ctx}
      , executor_{ctx.get_executor()}
    {
    }

    Executor get_executor() const noexcept { return executor_; }

    AsyncStream stream_;
    Timer read_timer_;
    Timer write_timer_;

private:
    Executor executor_;
};
template<typename CompletionToken>
using io_completion_t =
  boost::asio::async_completion<CompletionToken,
                                void(boost::system::error_code, std::size_t)>;

template<typename CompletionToken>
using io_completion_result_t =
  BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                                void(boost::system::error_code, std::size_t));

template<typename State>
struct shutdown_op
{
    using socket_t = typename State::lowest_layer_type;
    using shutdown_type_t = typename socket_t::shutdown_type;
    using executor_type = typename State::executor_type;
    static_assert(
      std::is_same<executor_type, typename State::executor_type>::value,
      "Expected the same executor.");

    shutdown_op(shutdown_type_t type, std::shared_ptr<State> const& s)
      : state_{s}
      , type_{type}
    {
    }

    executor_type get_executor() const noexcept { return state_->get_executor(); }

    void operator()(boost::system::error_code ec)
    {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }

        state_->stream_.lowest_layer().shutdown(type_, ec);
    }

private:
    std::shared_ptr<State> state_;
    shutdown_type_t type_;
};

} // namespace detail

template<typename AsyncStream,
         typename Executor =
           boost::asio::strand<typename AsyncStream::executor_type>,
         typename Timer = boost::asio::steady_timer>
class basic_connection
{
    using state_t = detail::connection_state<AsyncStream, Executor, Timer>;

    template <typename CompletionHandler>
    struct read_op;

    struct timer_op;
public:
    using next_layer_type = AsyncStream;
    using lowest_layer_type = typename AsyncStream::lowest_layer_type;

    using executor_type = Executor;

    explicit basic_connection(boost::asio::io_context& ctx):
        state_{std::make_shared<state_t>(ctx)}
    {
    }

    executor_type get_executor() const noexcept
    {
        return state_->get_executor();
    }

    template<typename Initiator, typename Duration, typename Arg, typename CompletionToken>
    auto async_read(Initiator&& i, Duration&& d, Arg&& arg, CompletionToken&& tok)
      -> detail::io_completion_result_t<CompletionToken>
    {
        detail::io_completion_t<CompletionToken> init{tok};
        using ch_t = typename decltype(init)::completion_handler_type;

        read_op<ch_t> rop{std::move(init.completion_handler), state_};
        detail::shutdown_op<state_t> sop{lowest_layer_type::shutdown_receive, state_};
        std::forward<Initiator>(i)(state_->stream_, std::forward<Arg>(arg), std::move(rop));
        state_->read_timer_.expires_from_now(d);
        state_->read_timer_.async_wait(std::move(sop));

        return init.result.get();
    }

    next_layer_type const& next_layer() const noexcept
    {
        return state_->stream_;
    }

    next_layer_type& next_layer() noexcept { return state_->stream_; }

    lowest_layer_type const& lowest_layer() const noexcept
    {
        return state_->stream_.lowest_layer();
    }

    lowest_layer_type& lowest_layer() noexcept
    {
        return state_->stream_.lowest_layer();
    }

    void cancel();

private:
    std::shared_ptr<state_t> state_;
};

template<typename AsyncStream,
         typename Executor,
         typename Timer>
template <typename CompletionHandler>
struct basic_connection<AsyncStream, Executor, Timer>::read_op
{
    using executor_type = Executor;
    using allocator_type = boost::asio::associated_allocator_t<CompletionHandler>;

    read_op(CompletionHandler&& h, std::shared_ptr<state_t> const& s):
        state_{s},
        handler_{std::move(h)}
    {}

    executor_type get_executor() const noexcept
    {
        return state_->get_executor();
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    void operator()(boost::system::error_code ec, std::size_t n)
    {
        auto ec2 = ec;
        state_->read_timer_.cancel(ec2);
        // TODO(djarek): Correctly handle addional executor for handler
        boost::asio::dispatch(boost::beast::bind_handler(std::move(handler_), ec, n));
    }

private:
    std::shared_ptr<state_t> state_;
    CompletionHandler handler_;
};

struct read_some_initiator
{
    template <typename AsyncReadStream, typename MutableBuffers, typename CompletionToken>
    void operator()(AsyncReadStream& s, MutableBuffers&& mb, CompletionToken&& tok)
    {
        s.async_read_some(std::forward<MutableBuffers>(mb), std::forward<CompletionToken>(tok));
    }
};

BOOST_AUTO_TEST_CASE(test)
{
    boost::asio::io_context ctx;

    boost::asio::local::stream_protocol::socket socket{ctx};
    
    basic_connection<boost::asio::local::stream_protocol::socket> conn{ctx};

    boost::asio::local::connect_pair(socket, conn.lowest_layer());

    boost::beast::flat_buffer buffer;
    auto mb = buffer.prepare(1024);
    conn.async_read(read_some_initiator{}, std::chrono::seconds{1}, mb, [](boost::system::error_code ec, std::size_t n){
        printf("err: %s, %zu\n", ec.message().c_str(), n);
    });

    std::string str = "test";

    socket.write_some(boost::asio::buffer(str));

    ctx.run();
}

} // namespace netu