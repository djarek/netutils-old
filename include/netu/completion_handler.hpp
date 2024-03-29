//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_COMPLETION_HANDLER_HPP
#define NETU_COMPLETION_HANDLER_HPP

#include <netu/detail/handler_erasure.hpp>
#include <netu/detail/type_traits.hpp>

namespace netu
{

template<typename Signature>
class completion_handler;

template<typename R, typename... Ts>
class completion_handler<R(Ts...)>
{
public:
    completion_handler() = default;

    completion_handler(std::nullptr_t) noexcept;

    completion_handler(completion_handler const&) = delete;
    completion_handler(completion_handler&&) noexcept;

    template<typename Handler,
             class = detail::disable_conversion_t<Handler, completion_handler>>
    completion_handler(Handler&& handler);

    completion_handler& operator=(completion_handler&&) noexcept;

    completion_handler& operator=(completion_handler const&) = delete;

    ~completion_handler();

    template<typename Handler,
             class = detail::disable_conversion_t<Handler, completion_handler>>
    completion_handler& operator=(Handler&& handler);

    completion_handler& operator=(std::nullptr_t) noexcept;

    void swap(completion_handler& other) noexcept;

    template<typename... Args>
    R invoke(Args&&... args);

    explicit operator bool() const noexcept;

    template<typename U, typename... Vs>
    friend bool operator==(completion_handler<U(Vs...)> const& lhs,
                           std::nullptr_t) noexcept;

    template<typename U, typename... Vs>
    friend bool operator==(std::nullptr_t lhs,
                           completion_handler<U(Vs...)> const& rhs) noexcept;

    template<typename U, typename... Vs>
    friend bool operator!=(completion_handler<U(Vs...)> const& lhs,
                           std::nullptr_t rhs) noexcept;

    template<typename U, typename... Vs>
    friend bool operator!=(std::nullptr_t lhs,
                           completion_handler<U(Vs...)> const& rhs) noexcept;

private:
    static detail::vtable<R(Ts...)> const* default_vtable();

    detail::raw_handler_storage storage_;
    detail::vtable<R(Ts...)> const* vtable_ = default_vtable();
};

} // namespace netu

#include <netu/impl/completion_handler.hpp>

#endif // NETU_COMPLETION_HANDLER_HPP
