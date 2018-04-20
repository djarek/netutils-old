//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_TEST_STREAM_HPP
#define NETU_TEST_STREAM_HPP

#include <netu/completion_handler.hpp>
#include <netu/detail/async_utils.hpp>
#include <netu/synchronized_value.hpp>

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <boost/asio/basic_io_object.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <memory>
#include <mutex>

namespace netu
{

namespace test
{

class stream
{
public:
    using executor_type = boost::asio::io_context::executor_type;

    executor_type get_executor() noexcept;

    explicit stream(boost::asio::io_context& ctx)
    {
    }

    template<typename MutableBuffers, typename CompletionToken>
    auto async_read_some(MutableBuffers&& mb, CompletionToken&& tok)
      -> netu::detail::io_completion_result_t<CompletionToken>;

    template<typename ConstBuffers, typename CompletionToken>
    auto async_write_some(ConstBuffers&& b, CompletionToken&& tok)
      -> netu::detail::io_completion_result_t<CompletionToken>;

private:
    struct shared_state;

    template<typename CompletionHandler, typename MutableBuffers>
    class read_some_op;

    template<typename CompletionHandler, typename ConstBuffers>
    class write_some_op;

    using shared_state_ptr = std::shared_ptr<synchronized_value<shared_state>>;
    using weak_state_ptr = std::weak_ptr<synchronized_value<shared_state>>;

    shared_state_ptr local_;
    weak_state_ptr remote_;
};

struct stream::shared_state
{
    explicit shared_state(boost::asio::io_context& ctx)
      : ctx_{ctx}
    {
    }

    completion_handler<void(boost::system::error_code)> read_op_;

    boost::beast::flat_buffer read_buffer_;
    boost::asio::io_context& ctx_;
};

stream::executor_type
stream::get_executor() noexcept
{
    return netu::apply([](shared_state& s) { return s.ctx_.get_executor(); },
                       *local_);
}

template<typename CompletionHandler, typename ConstBuffers>
class stream::write_some_op
{
public:
    using executor_type =
      boost::asio::associated_executor_t<CompletionHandler,
                                         boost::asio::io_context>;
    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    template<typename Handler, typename Buffers>
    write_some_op(Handler&& ch,
                  boost::asio::io_context& ctx,
                  Buffers&& b,
                  weak_state_ptr l,
                  weak_state_ptr r)
      : buffers_{std::forward<Buffers>(b)}
      , ctx_{ctx}
      , weak_local_{std::move(l)}
      , weak_remote_{std::move(r)}
      , handler_{std::forward<Handler>(ch)}
    {
    }

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(handler_, ctx_);
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    void operator()()
    {
        shared_state_ptr local_ptr = weak_local_.lock();
        shared_state_ptr remote_ptr = weak_remote_.lock();
        if (!local_ptr || !remote_ptr)
        {
            return upcall(boost::asio::error::eof, 0);
        }

        auto n = netu::apply(
          [this](shared_state&, shared_state& remote) {
              auto mb = remote.read_buffer_.prepare(1500);
              auto n = boost::asio::buffer_copy(mb, buffers_);
              remote.read_buffer_.commit(n);
              if (remote.read_op_)
              {
                  remote.read_op_.invoke(boost::system::error_code{});
              }
              return n;
          },
          *local_ptr,
          *remote_ptr);

        upcall(boost::system::error_code{}, n);
    }

    void upcall(boost::system::error_code ec, std::size_t n)
    {
        boost::asio::post(
          boost::beast::bind_handler(std::move(handler_), ec, n), ctx_);
    }

private:
    CompletionHandler handler_;
    ConstBuffers buffers_;
    boost::asio::io_context& ctx_;
    weak_state_ptr weak_local_;
    weak_state_ptr weak_remote_;
};

template<typename CompletionHandler, typename MutableBuffers>
class stream::read_some_op
{
public:
    using executor_type =
      boost::asio::associated_executor_t<CompletionHandler,
                                         boost::asio::io_context>;
    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    template<typename Handler, typename Buffers>
    read_some_op(Handler&& ch,
                 boost::asio::io_context& ctx,
                 Buffers&& b,
                 weak_state_ptr l,
                 weak_state_ptr r)
      : buffers_{std::forward<Buffers>(b)}
      , ctx_{ctx}
      , weak_local_{std::move(l)}
      , weak_remote_{std::move(r)}
      , handler_{std::forward<Handler>(ch)}
    {
    }

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(handler_, ctx_);
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    void operator()(boost::system::error_code ec, bool post = true)
    {
        if (post)
        {
            return boost::asio::post(
              boost::beast::bind_handler(std::move(*this), ec, false), ctx_);
        }

        if (!ec)
        {
            // TODO: copy buffer
        }

        handler_(ec, 0);
    }

    void operator()()
    {
        shared_state_ptr local_ptr = weak_local_.lock();
        shared_state_ptr remote_ptr = weak_remote_.lock();

        if (!local_ptr || !remote_ptr)
        {
            return (*this)(boost::asio::error::eof);
        }

        bool suspend = netu::apply(
          [&](shared_state& local, shared_state&) {
              boost::system::error_code ec;
              std::size_t n =
                boost::asio::buffer_copy(buffers_, local.read_buffer_.data());
              if (n > 0)
              {
                  return false;
              }

              local.read_buffer_ = std::move(*this);
              return true;

          },
          *local_ptr,
          *remote_ptr);

        if (!suspend)
        {
            (*this)(boost::system::error_code{});
        }
        // upcall(ec, n);
    }

private:
    CompletionHandler handler_;
    MutableBuffers buffers_;
    boost::asio::io_context& ctx_;
    weak_state_ptr weak_local_;
    weak_state_ptr weak_remote_;
};

template<typename MutableBuffers, typename CompletionToken>
auto
stream::async_read_some(MutableBuffers&& b, CompletionToken&& tok)
  -> netu::detail::io_completion_result_t<CompletionToken>
{
    netu::detail::io_completion_t<CompletionToken> init{tok};

    return init.result.get();
}

template<typename ConstBuffers, typename CompletionToken>
auto
stream::async_write_some(ConstBuffers&& b, CompletionToken&& tok)
  -> netu::detail::io_completion_result_t<CompletionToken>
{
    netu::detail::io_completion_t<CompletionToken> init{tok};
    using ch_t = typename decltype(init)::completion_handler_type;
    using buf_t = typename std::decay<ConstBuffers>::type;
    write_some_op<ch_t, buf_t> op{
      std::move(init.completion_handler),
      std::forward<ConstBuffers>(b),
      get_executor().context(),
      local_,
      remote_,
    };
    op();
    return init.result.get();
}

} // namespace test

} // namespace netu

#endif // NETU_TEST_STREAM_HPP
