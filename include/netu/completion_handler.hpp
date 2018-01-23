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

#include <boost/core/pointer_traits.hpp>
#include <functional>
#include <cassert>

namespace netu
{

namespace detail
{

enum class handler_op
{
    invoke,
    dispatch,
    post,
    defer,
    destroy
};

template <typename Handler, typename Signature>
struct handler_manager;

template <typename Signature>
struct handler_base;

union raw_handler_ptr
{
    raw_handler_ptr() = default;

    explicit raw_handler_ptr(void* ptr) noexcept:
        void_ptr{ptr}
    {
    }

    template<typename U, typename... Vs>
    explicit raw_handler_ptr(U(*fptr)(Vs...)) noexcept:
        func_ptr{reinterpret_cast<void(*)()>(fptr)}
    {
    }

    void* void_ptr;
    void(*func_ptr)(); // never call this without casting to original type
};

template <typename R, typename... Ts>
struct handler_base<R(Ts...)>
{
    using management_func_t = void(*)(raw_handler_ptr, handler_base&) noexcept;
    using invocation_func_t = R(*)(handler_op, raw_handler_ptr, handler_base&, Ts...);

    handler_base() = default;

    template <typename Handler>
    explicit handler_base(handler_manager<Handler, R(Ts...)> hm) noexcept:
        invoke_{hm.invoke},
        manage_{hm.manage}
    {
    }

    invocation_func_t invoke_ = nullptr;
    management_func_t manage_ = nullptr;
};

template <typename Handler, typename R, typename... Ts>
struct handler_manager<Handler, R(Ts...)>
{
    static R invoke(handler_op op, raw_handler_ptr ptr, handler_base<R(Ts...)>& hb, Ts... args)
    {
        auto h = static_cast<Handler*>(ptr.void_ptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                auto handler = std::move(*h);
                // Deallocation-before-invocation guarantee
                manage(ptr, hb);
                return (handler)(detail::move_if_not_ref(std::forward<Ts>(args),
                                                         std::is_reference<Ts>{}) ...);
            }
            default:
                assert(false);
        }
    }

    static void manage(raw_handler_ptr ptr, handler_base<R(Ts...)>& hb) noexcept
    {
        auto h = static_cast<Handler*>(ptr.void_ptr);
        auto alloc = allocators::rebind_associated<Handler>(*h);
        using pointer_t = typename std::allocator_traits<decltype(alloc)>::pointer;
        auto fancy_ptr = boost::pointer_traits<pointer_t>::pointer_to(*h);
        std::allocator_traits<decltype(alloc)>::destroy(alloc, h);
        std::allocator_traits<decltype(alloc)>::deallocate(alloc, fancy_ptr, 1);
        hb = {};
    }
};

template <typename U, typename... Vs, typename R, typename... Ts>
struct handler_manager<U(*)(Vs...), R(Ts...)>
{
    static R invoke(handler_op op, raw_handler_ptr ptr, handler_base<R(Ts...)>& hb, Ts... args)
    {
        auto h = reinterpret_cast<U(*)(Vs...)>(ptr.func_ptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                manage(ptr, hb);
                return (h)(detail::move_if_not_ref(std::forward<Ts>(args), std::is_reference<Ts>{}) ...);
            }
            default:
                assert(false);
        }
    }

    static void manage(raw_handler_ptr, handler_base<R(Ts...)>& hb) noexcept
    {
        hb = {};
    }
};

template <typename Signature, typename U, typename... Ts>
std::pair<raw_handler_ptr, handler_base<Signature>> allocate_handler(U(*p)(Ts...)) noexcept
{
    return {raw_handler_ptr{p}, handler_base<Signature>{handler_manager<decltype(p), Signature>{}}};
}

template <typename Signature, typename Handler>
std::pair<raw_handler_ptr, handler_base<Signature>> allocate_handler(Handler&& handler)
{

    using handler_type = typename std::remove_reference<Handler>::type;
    auto alloc = detail::allocators::rebind_associated<handler_type>(handler);
    auto tmp = detail::allocators::allocate(alloc);
    std::allocator_traits<decltype(alloc)>::construct(alloc, boost::to_address(tmp.get()), std::forward<Handler>(handler));
    return {raw_handler_ptr{tmp.release()}, handler_base<Signature>{handler_manager<handler_type, Signature>{}}};
}

} // namespace detail

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
