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

#include <netu/detail/allocators.hpp>
#include <netu/detail/type_traits.hpp>

#include <boost/align/aligned_allocator_adaptor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/core/pointer_traits.hpp>

#include <boost/assert.hpp>
#include <functional>

namespace netu
{

namespace detail
{

template<typename Handler, typename Signature>
struct vtable_generator;

template<typename Signature>
struct default_vtable_generator;

template<typename Signature>
struct vtable;

union raw_handler_storage {
    using func_ptr_t = void (*)();
    // users of ASIO love to shove shared_ptr's into their CompletionHandlers,
    // so should take sizeof shared_ptr as the max of SBO
    using sbo_storage_type = std::aligned_union<1, std::shared_ptr<void>>::type;

    raw_handler_storage() = default;

    raw_handler_storage(raw_handler_storage const&) = delete;
    raw_handler_storage(raw_handler_storage&&) = delete;

    raw_handler_storage& operator=(raw_handler_storage const&) = delete;
    raw_handler_storage& operator=(raw_handler_storage&&) = delete;

    void* void_ptr;
    func_ptr_t func_ptr; // never call this without casting to original type
    sbo_storage_type buffer;
};

template<typename R, typename... Ts>
struct vtable<R(Ts...)>
{
    using destructor_t = void (*)(raw_handler_storage&) /*noexcept*/;
    using move_construct_t =
      void (*)(raw_handler_storage& /*dst*/,
               raw_handler_storage& /*src*/) /*noexcept*/;
    using invoke_t = R (*)(raw_handler_storage&, Ts...);

    invoke_t invoke;
    move_construct_t move_construct;
    destructor_t destroy;
};

template<typename R, typename... Ts>
struct default_vtable_generator<R(Ts...)>
{
    static R invoke(raw_handler_storage&, Ts...)
    {
        throw std::bad_function_call{};
    }

    static void move_construct(raw_handler_storage&,
                               raw_handler_storage&) noexcept
    {
    }

    static void destroy(raw_handler_storage&) noexcept {}

    static constexpr vtable<R(Ts...)> value{invoke, move_construct, destroy};
};

template<typename R, typename... Ts>
constexpr vtable<R(Ts...)> default_vtable_generator<R(Ts...)>::value;

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

template<typename Handler>
struct small_functor
{
    using allocator_type =
      detail::allocators::rebound_alloc_t<Handler, small_functor<Handler>>;
    using executor_type = boost::asio::associated_executor_t<Handler>;

    Handler handler_;

    template<typename DeducedHandler>
    explicit small_functor(DeducedHandler&& dh)
      : handler_{std::forward<DeducedHandler>(dh)}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return detail::allocators::rebind_associated<small_functor>(handler_);
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

template<typename T>
using can_use_sbo =
  std::integral_constant<bool,
                         std::is_nothrow_move_constructible<T>::value &&
                           sizeof(T) <= sizeof(raw_handler_storage) &&
                           alignof(T) <= alignof(raw_handler_storage)>;

template<typename Handler, typename R, typename... Ts>
struct vtable_generator<Handler, R(Ts...)>
{
    static R invoke(raw_handler_storage& s, Ts... args)
    {
        auto h = static_cast<Handler*>(s.void_ptr);
        BOOST_ASSERT(h != nullptr);
        auto handler = std::move(*h);
        // Deallocation-before-invocation guarantee
        destroy(s);
        return (handler)(std::forward<Ts>(args)...);
    }

    static void move_construct(raw_handler_storage& dst,
                               raw_handler_storage& src) noexcept
    {
        dst.void_ptr = src.void_ptr;
    }

    static void destroy(raw_handler_storage& s) noexcept
    {
        auto const h = static_cast<Handler*>(s.void_ptr);
        auto alloc = boost::asio::get_associated_allocator(*h);
        std::allocator_traits<decltype(alloc)>::destroy(alloc, h);
        std::allocator_traits<decltype(alloc)>::deallocate(alloc, h, 1);
    }

    static constexpr vtable<R(Ts...)> value{invoke, move_construct, destroy};
};

template<typename Handler, typename R, typename... Ts>
constexpr vtable<R(Ts...)> vtable_generator<Handler, R(Ts...)>::value;

template<typename Handler, typename R, typename... Ts>
struct vtable_generator<small_functor<Handler>, R(Ts...)>
{
    static R invoke(raw_handler_storage& p, Ts... args)
    {
        auto const h = reinterpret_cast<small_functor<Handler>*>(&p.buffer);
        auto handler = std::move(*h);
        // Deallocation-before-invocation guarantee
        destroy(p);
        return (handler)(std::forward<Ts>(args)...);
    }

    static void move_construct(raw_handler_storage& dst,
                               raw_handler_storage& src) noexcept
    {
        static_assert(sizeof(small_functor<Handler>) <= sizeof(dst.buffer),
                      "dst buffer too small");
        static_assert(alignof(small_functor<Handler>) <=
                        alignof(decltype(dst.buffer)),
                      "dst buffer not aligned properly");
        auto const h = reinterpret_cast<small_functor<Handler>*>(&src.buffer);
        new (&dst.buffer) small_functor<Handler>{std::move(*h)};
        destroy(src);
    }

    static void destroy(raw_handler_storage& s) noexcept
    {
        auto const h = reinterpret_cast<small_functor<Handler>*>(&s.buffer);
        h->~small_functor<Handler>();
    }

    static constexpr vtable<R(Ts...)> value{invoke, move_construct, destroy};
};

template<typename Handler, typename R, typename... Ts>
constexpr vtable<R(Ts...)>
  vtable_generator<small_functor<Handler>, R(Ts...)>::value;

template<typename U, typename... Vs, typename R, typename... Ts>
struct vtable_generator<U (*)(Vs...), R(Ts...)>
{
    static R invoke(raw_handler_storage& s, Ts... args)
    {
        auto const h = reinterpret_cast<U (*)(Vs...)>(s.func_ptr);
        BOOST_ASSERT(h != nullptr);
        return (h)(std::forward<Ts>(args)...);
    }

    static void move_construct(raw_handler_storage& dst,
                               raw_handler_storage& src) noexcept
    {
        dst.func_ptr = src.func_ptr;
    }

    static constexpr vtable<R(Ts...)> value{
      invoke,
      move_construct,
      default_vtable_generator<R(Ts...)>::destroy};
};

template<typename U, typename... Vs, typename R, typename... Ts>
constexpr vtable<R(Ts...)> vtable_generator<U (*)(Vs...), R(Ts...)>::value;

template<typename Handler, typename R, typename... Ts>
struct vtable_generator<std::reference_wrapper<Handler>, R(Ts...)>
{
    static R invoke(raw_handler_storage& s, Ts... args)
    {
        auto const h = static_cast<Handler*>(s.void_ptr);
        BOOST_ASSERT(h != nullptr);
        return (*h)(std::forward<Ts>(args)...);
    }

    static void move_construct(raw_handler_storage& dst,
                               raw_handler_storage& src) noexcept
    {
        dst.void_ptr = src.void_ptr;
    }

    static constexpr vtable<R(Ts...)> value{
      invoke,
      move_construct,
      default_vtable_generator<R(Ts...)>::destroy};
};

template<typename Handler, typename R, typename... Ts>
constexpr vtable<R(Ts...)>
  vtable_generator<std::reference_wrapper<Handler>, R(Ts...)>::value;

template<typename Signature, typename U, typename... Ts>
void
allocate_handler(raw_handler_storage& s,
                 vtable<Signature> const*& v,
                 U (*f)(Ts...)) noexcept
{
    s.func_ptr = reinterpret_cast<decltype(s.func_ptr)>(f);
    v = &vtable_generator<U (*)(Ts...), Signature>::value;
}

template<typename Signature, typename Handler>
void
allocate_handler_sbo(raw_handler_storage& s,
                     vtable<Signature> const*& v,
                     Handler&& h,
                     std::false_type)
{
    using handler_type =
      wrapper_selector<typename std::remove_reference<Handler>::type>;
    auto alloc = detail::allocators::rebind_associated<handler_type>(h);
    auto tmp = detail::allocators::allocate(alloc);
    std::allocator_traits<decltype(alloc)>::construct(
      alloc, boost::to_address(tmp), std::forward<Handler>(h));
    s.void_ptr = tmp.release();
    v = &vtable_generator<handler_type, Signature>::value;
}

template<typename Signature, typename Handler>
void
allocate_handler_sbo(raw_handler_storage& s,
                     vtable<Signature> const*& v,
                     Handler&& handler,
                     std::true_type)
{
    using handler_type =
      small_functor<typename std::remove_reference<Handler>::type>;
    new (&s.buffer) handler_type{std::forward<Handler>(handler)};
    v = &vtable_generator<handler_type, Signature>::value;
}

template<typename Signature, typename Handler>
void
allocate_handler(raw_handler_storage& s,
                 vtable<Signature> const*& v,
                 Handler&& handler)
{
    using handler_type = typename std::remove_reference<Handler>::type;
    allocate_handler_sbo<Signature>(
      s, v, std::forward<Handler>(handler), can_use_sbo<handler_type>{});
}

template<typename Signature, typename Handler>
void
allocate_handler(raw_handler_storage& s,
                 vtable<Signature> const*& v,
                 std::reference_wrapper<Handler> handler)
{
    s.void_ptr = &handler.get();
    v = &vtable_generator<std::reference_wrapper<Handler>, Signature>::value;
}

} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_HANDLER_ERASURE_HPP
