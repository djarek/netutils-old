//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_HANDLER_ERASURE_HPP
#define NETU_DETAIL_HANDLER_ERASURE_HPP

#include <netu/detail/type_traits.hpp>

#include <boost/align/aligned_allocator_adaptor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
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

template<typename Handler, typename Signature>
struct handler_manager;

template<typename Signature>
struct handler_base;

union raw_handler_ptr {
    raw_handler_ptr() = default;

    explicit raw_handler_ptr(void* ptr) noexcept
      : void_ptr{ptr}
    {
    }

    template<typename U, typename... Vs>
    explicit raw_handler_ptr(U (*fptr)(Vs...)) noexcept
      : func_ptr{reinterpret_cast<void (*)()>(fptr)}
    {
    }

    void* void_ptr;
    void (*func_ptr)(); // never call this without casting to original type
};

template<typename R, typename... Ts>
struct handler_base<R(Ts...)>
{
    using management_func_t = void (*)(raw_handler_ptr, handler_base&) noexcept;
    using invocation_func_t = R (*)(handler_op,
                                    raw_handler_ptr,
                                    handler_base&,
                                    Ts...);

    handler_base() = default;

    template<typename Handler>
    explicit handler_base(handler_manager<Handler, R(Ts...)> hm) noexcept
      : invoke_{hm.invoke}
      , manage_{hm.manage}
    {
    }

    invocation_func_t invoke_ = nullptr;
    management_func_t manage_ = nullptr;
};

template<typename Handler, typename R, typename... Ts>
struct handler_manager<Handler, R(Ts...)>
{
    static R invoke(handler_op op,
                    raw_handler_ptr ptr,
                    handler_base<R(Ts...)>& hb,
                    Ts... args)
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
                return (handler)(std::forward<Ts>(args)...);
            }
            default:
                BOOST_ASSERT(false);
        }
    }

    static void manage(raw_handler_ptr ptr, handler_base<R(Ts...)>& hb) noexcept
    {
        auto h = static_cast<Handler*>(ptr.void_ptr);
        auto alloc = boost::asio::get_associated_allocator(*h);
        std::allocator_traits<decltype(alloc)>::destroy(alloc, h);
        std::allocator_traits<decltype(alloc)>::deallocate(alloc, h, 1);
        hb = {};
    }
};

template<typename U, typename... Vs, typename R, typename... Ts>
struct handler_manager<U (*)(Vs...), R(Ts...)>
{
    static R invoke(handler_op op,
                    raw_handler_ptr ptr,
                    handler_base<R(Ts...)>& hb,
                    Ts... args)
    {
        auto h = reinterpret_cast<U (*)(Vs...)>(ptr.func_ptr);
        BOOST_ASSERT(h != nullptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                manage(ptr, hb);
                return (h)(std::forward<Ts>(args)...);
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

template<typename Handler, typename R, typename... Ts>
struct handler_manager<std::reference_wrapper<Handler>, R(Ts...)>
{
    static R invoke(handler_op op,
                    raw_handler_ptr ptr,
                    handler_base<R(Ts...)>& hb,
                    Ts... args)
    {
        auto h = static_cast<Handler*>(ptr.void_ptr);
        BOOST_ASSERT(h != nullptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                manage(ptr, hb);
                return (*h)(std::forward<Ts>(args)...);
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

template<typename Handler>
struct wrapper_selector
{
    using inner_alloc_type =
      detail::allocators::rebound_alloc_t<Handler, wrapper_selector>;
    using inner_pointer_type =
      typename std::allocator_traits<inner_alloc_type>::pointer;
    using inner_value_type =
      typename std::allocator_traits<inner_alloc_type>::value_type;
    using allocator_type = typename std::conditional<
      std::is_same<inner_pointer_type, inner_value_type*>::value,
      inner_alloc_type,
      boost::alignment::aligned_allocator_adaptor<inner_alloc_type>>::type;
    using executor_type = boost::asio::associated_executor_t<Handler>;

    Handler handler_;

    template<typename DeducedHandler>
    explicit wrapper_selector(DeducedHandler&& dh)
      : handler_{std::forward<DeducedHandler>(dh)}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return detail::allocators::rebind_associated<wrapper_selector>(
          handler_);
    }

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(handler_);
    }

    template<typename... Ts>
    auto operator()(Ts&&... ts) -> decltype(handler_(std::forward<Ts>(ts)...))
    {
        return handler_(std::forward<Ts>(ts)...);
    }
};

template<typename Signature, typename U, typename... Ts>
std::pair<raw_handler_ptr, handler_base<Signature>>
allocate_handler(U (*p)(Ts...)) noexcept
{
    return {raw_handler_ptr{p},
            handler_base<Signature>{handler_manager<decltype(p), Signature>{}}};
}

template<typename Signature, typename Handler>
std::pair<raw_handler_ptr, handler_base<Signature>>
allocate_handler(Handler&& handler)
{
    using handler_type =
      wrapper_selector<typename std::remove_reference<Handler>::type>;
    auto alloc = detail::allocators::rebind_associated<handler_type>(handler);
    auto tmp = detail::allocators::allocate(alloc);
    std::allocator_traits<decltype(alloc)>::construct(
      alloc, boost::to_address(tmp), std::forward<Handler>(handler));
    return {
      raw_handler_ptr{tmp.release()},
      handler_base<Signature>{handler_manager<handler_type, Signature>{}}};
}

template<typename Signature, typename Handler>
std::pair<raw_handler_ptr, handler_base<Signature>>
allocate_handler(std::reference_wrapper<Handler> handler)
{
    return {raw_handler_ptr{&handler.get()},
            handler_base<Signature>{
              handler_manager<std::reference_wrapper<Handler>, Signature>{}}};
}

} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_HANDLER_ERASURE_HPP
