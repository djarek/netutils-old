//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_IMPL_BASIC_TIMEOUT_HPP
#define NETU_IMPL_BASIC_TIMEOUT_HPP

#include <netu/basic_timeout.hpp>
#include <netu/synchronized_value.hpp>

namespace netu
{
namespace detail
{

struct timeout_entry : boost::intrusive::set_base_hook<>
{
    std::chrono::steady_clock::time_point expiry;
    completion_handler<void(boost::system::error_code)> handler;

    friend bool operator<(timeout_entry const& lhs,
                          timeout_entry const& rhs) noexcept
    {
        return lhs.expiry < rhs.expiry;
    }
};

inline bool
try_invoke(completion_handler<void(boost::system::error_code)>& handler,
           boost::system::error_code ec)
{
    if (handler)
    {
        handler.invoke(ec);
        return true;
    }
    else
    {
        return false;
    }
}

template<typename TimePoint>
bool
expired(timeout_entry const& e, TimePoint now)
{
    return e.expiry < now;
}

template<typename T>
bool
is_first_in(T const& t, boost::intrusive::set<T> const& l)
{
    auto const it = boost::intrusive::set<T>::s_iterator_to(t);
    return it == l.begin();
}

template<typename CompletionHandler, typename Executor>
class suspended_handler
{
public:
    using allocator_type =
      boost::asio::associated_allocator_t<CompletionHandler>;

    explicit suspended_handler(CompletionHandler&& handler, Executor e)
      : handler_{std::forward<CompletionHandler>(handler)}
      , guard_{std::move(e)}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        boost::asio::post(
          guard_.get_executor(),
          boost::beast::bind_handler(std::move(handler_), args...));
    }

private:
    CompletionHandler handler_;
    boost::asio::executor_work_guard<Executor> guard_;
};

template<typename Timer>
class basic_timeout_service : public boost::asio::io_context::service
{
public:
    static const boost::asio::io_context::id id;

    using implementation_type = timeout_entry;
    using list_type = boost::intrusive::set<timeout_entry>;
    using timer_type = Timer;
    using clock_type = typename Timer::clock_type;
    using duration = typename clock_type::duration;
    using time_point = typename clock_type::time_point;

    struct state
    {
        explicit state(boost::asio::io_context& ctx)
          : timer_{ctx}
        {
        }

        Timer timer_;
        list_type timeouts_;
    };

    explicit basic_timeout_service(boost::asio::io_context& ctx)
      : boost::asio::io_context::service{ctx}
      , synchronized_state_{ctx}
    {
    }

    void construct(timeout_entry&)
    {
    }

    void move_construct(timeout_entry& to, timeout_entry& from)
    {
        apply([&to, &from](state&) { to = detail::exchange(from, {}); },
              synchronized_state_);
    }

    void move_assign(timeout_entry& to,
                     basic_timeout_service& other,
                     timeout_entry& from)
    {
        if (&other == this)
        {
            apply([&to, &from](state&) { to = detail::exchange(from, {}); },
                  synchronized_state_);
        }
        else
        {
            apply(
              [&to, &from](state&, state&) { to = detail::exchange(from, {}); },
              synchronized_state_,
              other.synchronized_state_);
        }
    }

    void destroy(timeout_entry& e)
    {
        apply(
          [&e](state& s) {
              if (e.is_linked())
              {
                  s.timeouts_.erase(list_type::s_iterator_to(e));
              }
              try_invoke(e.handler, boost::asio::error::operation_aborted);
          },
          synchronized_state_);
    }

    template<typename CompletionToken>
    auto async_wait(timeout_entry& e, CompletionToken&& tok)
      -> detail::wait_completion_result_t<CompletionToken>
    {

        return apply(
          [&e, &tok, this](state&) {
              detail::wait_completion_t<CompletionToken> init{tok};
              using ch_t = typename decltype(init)::completion_handler_type;
              auto handler =
                suspended_handler<ch_t, boost::asio::io_context::executor_type>{
                  std::move(init.completion_handler),
                  this->get_io_context().get_executor()};

              if (expired(e, clock_type::now()))
              {
                  auto const ec = boost::system::error_code{};
                  handler(ec);
              }
              else
              {
                  e.handler = std::move(handler);
              }

              return init.result.get();
          },
          synchronized_state_);
    }

    bool expires_from_now(timeout_entry& e, duration d)
    {
        return apply(
          [&e, d, this](state& s) {
              bool needs_reschedule = s.timeouts_.empty();
              bool const was_linked = e.is_linked();
              if (was_linked)
              {
                  needs_reschedule = is_first_in(e, s.timeouts_);
                  s.timeouts_.erase(list_type::s_iterator_to(e));
              }
              e.expiry = clock_type::now() + d;
              s.timeouts_.insert(e);

              if (needs_reschedule)
              {
                  reschedule(s);
              }

              return was_linked;
          },
          synchronized_state_);
    }

    bool cancel(timeout_entry& e)
    {
        return apply(
          [&e, this](state& s) {
              if (e.is_linked())
              {
                  bool const needs_reschedule = is_first_in(e, s.timeouts_);
                  s.timeouts_.erase(list_type::s_iterator_to(e));
                  if (needs_reschedule && !s.timeouts_.empty())
                  {
                      reschedule(s);
                  }
              }

              return try_invoke(e.handler,
                                boost::asio::error::operation_aborted);
          },
          synchronized_state_);
    }

    // private:
    void reschedule(state& s)
    {
        BOOST_ASSERT(!s.timeouts_.empty());

        s.timer_.expires_at(s.timeouts_.begin()->expiry);
        s.timer_.async_wait(synchronize(
          [this](state& s, boost::system::error_code ec) {
              if (ec)
              {
                  return;
              }

              auto const now = clock_type::now();
              static auto const rescheduler = [this](state* s) {
                  if (!s->timeouts_.empty())
                  {
                      reschedule(*s);
                  }
              };
              std::unique_ptr<state, decltype(rescheduler)> guard{&s,
                                                                  rescheduler};

              for (auto it = s.timeouts_.begin(); it != s.timeouts_.end();)
              {
                  if (!expired(*it, now))
                  {
                      break;
                  }

                  auto& entry = *it;
                  it = s.timeouts_.erase(it);
                  try_invoke(entry.handler, ec);
              }
          },
          synchronized_state_));
    }

    synchronized_value<state> synchronized_state_;
};

template<typename Timer>
boost::asio::io_context::id const basic_timeout_service<Timer>::id;

} // namespace detail

} // namespace netu

#endif // NETU_IMPL_BASIC_TIMEOUT_TIMER_HPP
