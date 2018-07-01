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

bool
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

    explicit basic_timeout_service(boost::asio::io_context& ctx)
      : boost::asio::io_context::service{ctx}
      , timer_{ctx}
    {
    }

    void construct(timeout_entry&)
    {
    }

    void destroy(timeout_entry& e)
    {
        if (e.is_linked())
        {
            timeouts_.erase(list_type::s_iterator_to(e));
        }
        try_invoke(e.handler, boost::asio::error::operation_aborted);
    }

    template<typename CompletionToken>
    auto async_wait(timeout_entry& e, CompletionToken&& tok)
      -> detail::wait_completion_result_t<CompletionToken>
    {
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
    }

    bool expires_from_now(timeout_entry& e, duration d)
    {
        bool needs_reschedule = timeouts_.empty();
        bool const was_linked = e.is_linked();
        if (was_linked)
        {
            needs_reschedule = is_first_in(e, timeouts_);
            timeouts_.erase(list_type::s_iterator_to(e));
        }
        e.expiry = clock_type::now() + d;
        timeouts_.insert(e);

        if (needs_reschedule)
        {
            reschedule();
        }

        return was_linked;
    }

    bool cancel(timeout_entry& e)
    {
        if (e.is_linked())
        {
            bool const needs_reschedule = is_first_in(e, timeouts_);
            timeouts_.erase(list_type::s_iterator_to(e));
            if (needs_reschedule && !timeouts_.empty())
            {
                reschedule();
            }
        }

        return try_invoke(e.handler, boost::asio::error::operation_aborted);
    }

    // private:
    void reschedule()
    {
        if (timeouts_.empty())
        {
            return;
        }

        timer_.expires_at(timeouts_.begin()->expiry);
        timer_.async_wait([this](boost::system::error_code ec) {
            if (ec)
            {
                return;
            }

            auto const now = clock_type::now();
            static auto const rescheduler = [](basic_timeout_service* service) {
                if (!service->timeouts_.empty())
                {
                    service->reschedule();
                }
            };
            std::unique_ptr<basic_timeout_service, decltype(rescheduler)> guard{
              this, rescheduler};

            for (auto it = timeouts_.begin(); it != timeouts_.end();)
            {
                if (!expired(*it, now))
                {
                    break;
                }

                auto& entry = *it;
                it = timeouts_.erase(it);
                try_invoke(entry.handler, ec);
            }
        });
    }

    Timer timer_;
    list_type timeouts_;
};

template<typename Timer>
boost::asio::io_context::id const basic_timeout_service<Timer>::id;

} // namespace detail

} // namespace netu

#endif // NETU_IMPL_BASIC_TIMEOUT_TIMER_HPP
