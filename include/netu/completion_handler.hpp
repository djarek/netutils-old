//
// Copyright (c) 2017 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_COMPLETION_HANDLER_HPP
#define NETU_COMPLETION_HANDLER_HPP

#include <netu/detail/allocators.hpp>
#include <netu/detail/type_traits.hpp>
#include <netu/detail/handler_erasure.hpp>

#include <functional>

namespace netu
{

template <typename Signature>
class completion_handler;

template <typename R, typename... Ts>
class completion_handler<R(Ts...)>
{
public:
    completion_handler() = default;

    completion_handler(completion_handler const& ) = delete;
    completion_handler(completion_handler && ) noexcept;

    template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler>>
    completion_handler(Handler&& handler);

    completion_handler& operator=(completion_handler && ) noexcept;

    completion_handler& operator=(completion_handler const& ) = delete;

    ~completion_handler();

    template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler>>
    completion_handler& operator=(Handler&& handler);

    completion_handler& operator=(std::nullptr_t) noexcept;

    void swap(completion_handler& other) noexcept;

    template <typename... DeducedArgs>
    R invoke(DeducedArgs&&... args);

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(base_.manage_);
    }

    template <typename U, typename... Vs>
    friend bool operator==(completion_handler<U(Vs...)> const& lhs, std::nullptr_t) noexcept;

    template <typename U, typename... Vs>
    friend bool operator==(std::nullptr_t lhs, completion_handler<U(Vs...)> const& rhs) noexcept;

    template <typename U, typename... Vs>
    friend bool operator!=(completion_handler<U(Vs...)> const& lhs, std::nullptr_t rhs) noexcept;

    template <typename U, typename... Vs>
    friend bool operator!=(std::nullptr_t lhs, completion_handler<U(Vs...)> const& rhs) noexcept;

private:
    detail::raw_handler_ptr hptr_;
    detail::handler_base<R(Ts...)> base_;
};

template <typename R, typename... Ts>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<R(Ts...)>>>
completion_handler<R(Ts...)>::completion_handler(Handler&& handler)
{
    std::tie(hptr_, base_) = detail::allocate_handler<R(Ts...)>(std::forward<Handler>(handler));
}

template <typename R, typename... Ts>
completion_handler<R(Ts...)>::completion_handler(completion_handler&& other) noexcept:
    hptr_{other.hptr_},
    base_{detail::exchange(other.base_, {})}
{
}

template <typename R, typename... Ts>
completion_handler<R(Ts...)>::~completion_handler()
{
    if (base_.manage_)
    {
        base_.manage_(hptr_, base_);
    }
}

template <typename R, typename... Ts>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<R(Ts...)>>>
completion_handler<R(Ts...)>& completion_handler<R(Ts...)>::operator=(Handler&& handler)
{
    completion_handler tmp{std::forward<Handler>(handler)};
    this->swap(tmp);
    return *this;
}

template <typename R, typename... Ts>
completion_handler<R(Ts...)>& completion_handler<R(Ts...)>::operator=(completion_handler&& other) noexcept
{
    completion_handler tmp{std::move(other)};
    this->swap(tmp);
    return *this;
}

template <typename R, typename... Ts>
completion_handler<R(Ts...)>& completion_handler<R(Ts...)>::operator=(std::nullptr_t) noexcept
{
    completion_handler tmp;
    this->swap(tmp);
    return *this;
}

template <typename R, typename... Ts>
void completion_handler<R(Ts...)>::swap(completion_handler& other) noexcept
{
    using std::swap;
    swap(hptr_, other.hptr_);
    swap(base_, other.base_);
}

template <typename R, typename... Ts>
template <typename... Args>
R completion_handler<R(Ts...)>::invoke(Args&&... args)
{
    if (!base_.manage_)
    {
        throw std::bad_function_call{};
    }

    return base_.invoke_(detail::handler_op::invoke, hptr_, base_, std::forward<Args>(args)...);
}

template <typename R, typename... Ts>
bool operator==(completion_handler<R(Ts...)> const& lhs, std::nullptr_t) noexcept
{
    return lhs.base_.manage_ == nullptr;
}

template <typename R, typename... Ts>
bool operator==(std::nullptr_t lhs, completion_handler<R(Ts...)> const& rhs) noexcept
{
    return rhs == lhs;
}

template <typename R, typename... Ts>
bool operator!=(completion_handler<R(Ts...)> const& lhs, std::nullptr_t rhs) noexcept
{
    return !(lhs == rhs);
}

template <typename R, typename... Ts>
bool operator!=(std::nullptr_t lhs, completion_handler<R(Ts...)> const& rhs) noexcept
{
    return rhs != lhs;
}

template <typename R, typename... Ts>
void swap(completion_handler<R(Ts...)>& lhs, completion_handler<R(Ts...)>& rhs) noexcept
{
    return lhs.swap(rhs);
}

} // namespace netu

#endif // NETU_COMPLETION_HANDLER_HPP
