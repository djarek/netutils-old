//
// Copyright (c) 2017 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_HANDLER_ERASURE_HPP
#define NETU_DETAIL_HANDLER_ERASURE_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/core/pointer_traits.hpp>

#include <boost/assert.hpp>

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
        BOOST_ASSERT(h != nullptr);
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
                BOOST_ASSERT(false);
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
        BOOST_ASSERT(h != nullptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                manage(ptr, hb);
                return (h)(detail::move_if_not_ref(std::forward<Ts>(args), std::is_reference<Ts>{}) ...);
            }
            default:
                BOOST_ASSERT(false);
        }
    }

    static void manage(raw_handler_ptr, handler_base<R(Ts...)>& hb) noexcept
    {
        hb = {};
    }
};

template <typename Handler, typename R, typename... Ts>
struct handler_manager<std::reference_wrapper<Handler>, R(Ts...)>
{
    static R invoke(handler_op op, raw_handler_ptr ptr, handler_base<R(Ts...)>& hb, Ts... args)
    {
        auto h = static_cast<Handler*>(ptr.void_ptr);
        BOOST_ASSERT(h != nullptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                manage(ptr, hb);
                return (*h)(detail::move_if_not_ref(std::forward<Ts>(args), std::is_reference<Ts>{}) ...);
            }
            default:
                BOOST_ASSERT(false);
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

template <typename Signature, typename Handler>
std::pair<raw_handler_ptr, handler_base<Signature>> allocate_handler(std::reference_wrapper<Handler> handler)
{
    return {raw_handler_ptr{&handler.get()}, handler_base<Signature>{handler_manager<std::reference_wrapper<Handler>, Signature>{}}};
}

} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_HANDLER_ERASURE_HPP
