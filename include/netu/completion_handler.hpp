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

#include <boost/core/ignore_unused.hpp>
#include <boost/core/pointer_traits.hpp>
#include <functional>
#include <cassert>

namespace netu
{

namespace detail
{

template <typename T>
T& move_if_not_ref(T& t, std::true_type)
{
    return t;
}

template <typename T>
T&& move_if_not_ref(T&& t, std::false_type)
{
    return std::move(t);
}

template<class T, class U = T>
T exchange(T& obj, U&& val)
{
    T old = std::move(obj);
    obj = std::forward<U>(val);
    return old;
}

template <typename From, typename To>
using disable_same_conversion_t = typename std::enable_if<!std::is_same<To, typename std::decay<From>::type>::value>::type;

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
struct completion_handler_base;

template <typename R, typename... Ts>
struct completion_handler_base<R(Ts...)>
{
    union storage_type
    {
        void* void_ptr;
        R(*func_ptr)(Ts...);
    };

    using management_func_t = void(*)(completion_handler_base&);
    using invocation_func_t = R(*)(handler_op, completion_handler_base&, Ts...);

    completion_handler_base() = default;

    template <typename Handler>
    completion_handler_base(void* ptr, handler_manager<Handler, R(Ts...)> hm) noexcept:
        invoke_{hm.invoke},
        manage_{hm.manage}
    {
        storage_.void_ptr = ptr;
    }

    template <typename Handler>
    completion_handler_base(R(*ptr)(Ts...), handler_manager<Handler, R(Ts...)> hm) noexcept:
        invoke_{hm.invoke},
        manage_{hm.manage}
    {
        storage_.func_ptr = ptr;
    }

    completion_handler_base(completion_handler_base const& other) = delete;

    completion_handler_base(completion_handler_base&& other) noexcept:
        invoke_{exchange(other.invoke_, nullptr)},
        manage_{exchange(other.manage_, nullptr)},
        storage_{other.storage_}
    {
    }

    ~completion_handler_base()
    {
        reset();
    }

    completion_handler_base& operator=(completion_handler_base const& other) = delete;

    completion_handler_base& operator=(completion_handler_base&& other) noexcept
    {
        completion_handler_base tmp{std::move(other)};
        tmp.swap(*this);
        return *this;
    }

    void swap(completion_handler_base& other) noexcept
    {
        using std::swap;
        swap(invoke_, other.invoke_);
        swap(manage_, other.manage_);
        swap(storage_, other.storage_);
    }

    void reset()
    {
        if (manage_)
        {
            manage_(*this);
            invoke_ = nullptr;
            manage_ = nullptr;
        }
    }

    template <typename... Us>
    R invoke(Us&&... us)
    {
        return invoke_(handler_op::invoke, *this, std::forward<Us>(us)...);
    }

    invocation_func_t invoke_ = nullptr;
    management_func_t manage_ = nullptr;
    storage_type storage_;
};

template <typename Handler, typename R, typename... Ts>
struct handler_manager<Handler, R(Ts...)>
{
    using handler_base = completion_handler_base<R(Ts...)>;
    static R invoke(handler_op op, handler_base& hb, Ts... args)
    {
        auto h = static_cast<Handler*>(hb.storage_.void_ptr);
        switch (op)
        {
            case handler_op::invoke:
            {
                auto handler = std::move(*h);
                // Deallocation-before-invocation guarantee
                hb = {};
                return (handler)(detail::move_if_not_ref(std::forward<Ts>(args),
                                                         std::is_reference<Ts>{}) ...);
            }
            default:
                assert(false);
        }
    }

    static void manage(handler_base& hb)
    {
        auto h = static_cast<Handler*>(hb.storage_.void_ptr);
        auto alloc = allocators::rebind_associated<Handler>(*h);
        using pointer_t = typename std::allocator_traits<decltype(alloc)>::pointer;
        auto fancy_ptr = boost::pointer_traits<pointer_t>::pointer_to(*h);
        std::allocator_traits<decltype(alloc)>::destroy(alloc, h);
        std::allocator_traits<decltype(alloc)>::deallocate(alloc, fancy_ptr, 1);
    }
};

template <typename U, typename... Vs, typename R, typename... Ts>
struct handler_manager<U(*)(Vs...), R(Ts...)>
{
    using handler_base = completion_handler_base<R(Ts...)>;

    static R invoke(handler_op op, handler_base& hb, Ts... args)
    {
        auto h = hb.storage_.func_ptr;
        switch (op)
        {
            case handler_op::invoke:
            {
                hb = {};
                return (h)(detail::move_if_not_ref(std::forward<Ts>(args), std::is_reference<Ts>{}) ...);
            }
            default:
                assert(false);
        }
    }

    static void manage(handler_base& hb)
    {
        boost::ignore_unused(hb);
    }
};

template <typename Signature, typename U, typename... Ts>
completion_handler_base<Signature> allocate_handler(U(*p)(Ts...))
{
    return completion_handler_base<Signature>{p, handler_manager<decltype(p), Signature>{}};
}

template <typename Signature, typename Handler>
completion_handler_base<Signature> allocate_handler(Handler&& handler)
{

    using handler_type = typename std::remove_reference<Handler>::type;
    auto alloc = allocators::rebind_associated<handler_type>(handler);
    auto tmp = detail::allocators::allocate(alloc);
    std::allocator_traits<decltype(alloc)>::construct(alloc, boost::to_address(tmp.get()), std::forward<Handler>(handler));
    completion_handler_base<Signature> chb {tmp.release(), handler_manager<handler_type, Signature>{}};
    return chb;
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
    completion_handler(completion_handler && ) = default;

    template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler>>
    completion_handler(Handler&& handler);

    completion_handler& operator=(completion_handler && ) = default;

    completion_handler& operator=(completion_handler const& ) = delete;

    template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler>>
    auto operator=(Handler&& handler) -> decltype(*this);

    auto operator=(std::nullptr_t) noexcept -> decltype(*this);

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
    detail::completion_handler_base<R(Ts...)> base_;
};

template <typename R, typename... Ts>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<R(Ts...)>>>
completion_handler<R(Ts...)>::completion_handler(Handler&& handler):
    base_{detail::allocate_handler<R(Ts...)>(std::forward<Handler>(handler))}
{
}

template <typename R, typename... Ts>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<R(Ts...)>>>
auto completion_handler<R(Ts...)>::operator=(Handler&& handler) -> decltype(*this)
{
    base_ = detail::allocate_handler<R(Ts...)>(std::forward<Handler>(handler));
    return *this;
}

template <typename R, typename... Ts>
auto completion_handler<R(Ts...)>::operator=(std::nullptr_t) noexcept -> decltype(*this)
{
    base_.reset();
    return *this;
}

template <typename R, typename... Ts>
void completion_handler<R(Ts...)>::swap(completion_handler& other) noexcept
{
    using std::swap;
    base_.swap(other.base_);
}

template <typename R, typename... Ts>
template <typename... Args>
R completion_handler<R(Ts...)>::invoke(Args&&... args)
{
    if (!base_.manage_)
    {
        throw std::bad_function_call{};
    }

    return base_.invoke(std::forward<Args>(args)...);
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
    return !(rhs == lhs);
}

template <typename R, typename... Ts>
void swap(completion_handler<R(Ts...)>& lhs, completion_handler<R(Ts...)>& rhs) noexcept
{
    return lhs.swap(rhs);
}

} // namespace netu

#endif // NETU_COMPLETION_HANDLER_HPP
