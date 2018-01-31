//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_IMPL_COMPLETION_HANDLER_HPP
#define NETU_IMPL_COMPLETION_HANDLER_HPP

#include <netu/completion_handler.hpp>

namespace netu
{

template<typename R, typename... Ts>
template<typename Handler, class>
completion_handler<R(Ts...)>::completion_handler(Handler&& handler)
{
    detail::allocate_handler<R(Ts...)>(
      storage_, vtable_, std::forward<Handler>(handler));
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>::completion_handler(std::nullptr_t) noexcept
  : completion_handler{}
{
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>::completion_handler(
  completion_handler&& other) noexcept
  : vtable_{detail::exchange(other.vtable_, default_vtable())}
{
    vtable_->move_construct(storage_, other.storage_);
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>::~completion_handler()
{
    vtable_->destroy(storage_);
}

template<typename R, typename... Ts>
template<typename Handler, class>
completion_handler<R(Ts...)>&
completion_handler<R(Ts...)>::operator=(Handler&& handler)
{
    *this = completion_handler{std::forward<Handler>(handler)};
    return *this;
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>&
completion_handler<R(Ts...)>::operator=(completion_handler&& other) noexcept
{
    completion_handler{std::move(other)}.swap(*this);
    return *this;
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>& completion_handler<R(Ts...)>::operator=(
  std::nullptr_t) noexcept
{
    *this = completion_handler{};
    return *this;
}

template<typename R, typename... Ts>
void
completion_handler<R(Ts...)>::swap(completion_handler& other) noexcept
{
    detail::raw_handler_storage tmp;
    vtable_->move_construct(tmp, storage_);
    other.vtable_->move_construct(storage_, other.storage_);
    vtable_->move_construct(other.storage_, tmp);

    using std::swap;
    swap(vtable_, other.vtable_);
}

template<typename R, typename... Ts>
template<typename... Args>
R
completion_handler<R(Ts...)>::invoke(Args&&... args)
{
    // Need to clear the vtable ptr in order to avoid calling the destructor of
    // the stored handler twice, if invocation of the handler throws.
    auto v = detail::exchange(vtable_, default_vtable());
    return v->invoke(storage_, std::forward<Args>(args)...);
}

template<typename R, typename... Ts>
detail::vtable<R(Ts...)> const*
completion_handler<R(Ts...)>::default_vtable()
{
    return &detail::default_vtable_generator<R(Ts...)>::value;
}

template<typename R, typename... Ts>
bool
operator==(completion_handler<R(Ts...)> const& lhs, std::nullptr_t) noexcept
{
    return !lhs;
}

template<typename R, typename... Ts>
bool
operator==(std::nullptr_t lhs, completion_handler<R(Ts...)> const& rhs) noexcept
{
    return rhs == lhs;
}

template<typename R, typename... Ts>
bool
operator!=(completion_handler<R(Ts...)> const& lhs, std::nullptr_t rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename R, typename... Ts>
bool
operator!=(std::nullptr_t lhs, completion_handler<R(Ts...)> const& rhs) noexcept
{
    return rhs != lhs;
}

template<typename R, typename... Ts>
completion_handler<R(Ts...)>::operator bool() const noexcept
{
    return vtable_ != default_vtable();
}

template<typename R, typename... Ts>
void
swap(completion_handler<R(Ts...)>& lhs,
     completion_handler<R(Ts...)>& rhs) noexcept
{
    return lhs.swap(rhs);
}

} // namespace netu

#endif // NETU_IMPL_COMPLETION_HANDLER_HPP
