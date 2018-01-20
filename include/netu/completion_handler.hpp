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

#include <boost/asio/associated_allocator.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/pointer_traits.hpp>
#include <functional>

namespace netu
{

namespace detail
{
namespace allocators
{

template <typename T, typename U>
using rebind_t = typename std::allocator_traits<boost::asio::associated_allocator_t<T>>::template rebind_alloc<U>;

template <typename ReboundType, typename T>
auto rebind_associated(T& t) noexcept -> rebind_t<T, ReboundType>
{
    auto alloc = boost::asio::get_associated_allocator(t);
    using rebound_alloc_t = typename std::allocator_traits<decltype(alloc)>::template rebind_alloc<ReboundType>;
    return rebound_alloc_t{alloc};
}

template <typename Allocator>
struct deallocator
{
    using pointer = typename std::allocator_traits<Allocator>::pointer;

    deallocator(Allocator& alloc, std::size_t n) noexcept:
        alloc_{alloc},
        n_{n}
    {
    }

    void operator ()(pointer p) noexcept
    {
        std::allocator_traits<Allocator>::deallocate(alloc_, p, n_);
    }

    Allocator& alloc_;
    std::size_t n_;
};

template <typename Allocator>
using alloc_ptr = std::unique_ptr<typename std::allocator_traits<Allocator>::value_type, deallocator<Allocator>>;

template <typename Allocator>
auto allocate(Allocator& alloc, std::size_t n = 1) -> alloc_ptr<Allocator>
{
    return alloc_ptr<Allocator>{
        std::allocator_traits<Allocator>::allocate(alloc, n),
        deallocator<Allocator>{alloc, n}
    };
}

} // namespace allocators

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

template <typename From, typename To>
using disable_same_conversion_t = typename std::enable_if<!std::is_same<To, typename std::decay<From>::type>::value>::type;

} // namespace detail

template <typename Signature>
class completion_handler;

template <typename ReturnType, typename... Args>
class completion_handler<ReturnType(Args...)>
{
private:
    struct deleter;

    struct eraser_base;

    template <typename CompletionHandler>
    struct eraser;

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
    ReturnType invoke(DeducedArgs&&... args);

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(ptr_);
    }

    template <typename R, typename... Ts>
    friend bool operator==(completion_handler<R(Ts...)> const& handler, std::nullptr_t) noexcept;

    template <typename R, typename... Ts>
    friend bool operator==(std::nullptr_t, completion_handler<R(Ts...)> const& handler) noexcept;

    template <typename R, typename... Ts>
    friend bool operator!=(completion_handler<R(Ts...)> const& handler, std::nullptr_t) noexcept;

    template <typename R, typename... Ts>
    friend bool operator!=(std::nullptr_t, completion_handler<R(Ts...)> const& handler) noexcept;

private:
    template <typename Handler>
    static std::unique_ptr<eraser_base, deleter> make_eraser(Handler&& handler);

    std::unique_ptr<eraser_base, deleter> ptr_;
};

template <typename ReturnType, typename... Args>
struct completion_handler<ReturnType(Args...)>::eraser_base
{
    virtual ReturnType invoke(std::unique_ptr<eraser_base, deleter>& ptr, Args...) = 0;
    virtual void self_destruct() noexcept = 0;
protected:
    eraser_base() = default;
    eraser_base(eraser_base const&) = delete;
    eraser_base& operator=(eraser_base const&) = delete;
    eraser_base(eraser_base&&) = delete;
    eraser_base& operator=(eraser_base&&) = delete;
    ~eraser_base() = default;
};

template <typename ReturnType, typename... Args>
struct completion_handler<ReturnType(Args...)>::deleter
{
    void operator()(eraser_base* ptr)
    {
        ptr->self_destruct();
    }
};

template <typename ReturnType, typename... Args>
template <typename CompletionHandler>
struct completion_handler<ReturnType(Args...)>::eraser : eraser_base
{
public:
    template <typename Handler >
    eraser(Handler&& handler):
        handler_{std::forward<Handler>(handler)}
    {
    }

    ReturnType invoke(std::unique_ptr<eraser_base, deleter>& ptr, Args... args) override
    {
        auto handler = std::move(handler_);
        boost::ignore_unused(ptr.release());
        self_destruct();
        return handler(detail::move_if_not_ref(std::forward<Args>(args), std::is_reference<Args>{}) ...);
    }

    void self_destruct() noexcept override
    {
        auto alloc = detail::allocators::rebind_associated<eraser>(handler_);
        using pointer_t = typename std::allocator_traits<decltype(alloc)>::pointer;
        auto fancy_this = boost::pointer_traits<pointer_t>::pointer_to(*this);
        std::allocator_traits<decltype(alloc)>::destroy(alloc, this);
        std::allocator_traits<decltype(alloc)>::deallocate(alloc, fancy_this, 1);
    }

    CompletionHandler handler_;
};

template <typename ReturnType, typename... Args>
template <typename Handler>
auto
completion_handler<ReturnType(Args...)>::make_eraser(Handler&& handler) -> std::unique_ptr<eraser_base, deleter>
{
    auto alloc = detail::allocators::rebind_associated<eraser<Handler>>(handler);
    auto tmp = detail::allocators::allocate(alloc);
    std::allocator_traits<decltype(alloc)>::construct(alloc, boost::to_address(tmp.get()), std::forward<Handler>(handler));
    return std::unique_ptr<eraser_base, deleter>{boost::to_address(tmp.release())};
}

template <typename ReturnType, typename... Args>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<ReturnType(Args...)>>>
completion_handler<ReturnType(Args...)>::completion_handler(Handler&& handler):
    ptr_{make_eraser(std::forward<Handler>(handler))}
{
}

template <typename ReturnType, typename... Args>
template <typename Handler, class = detail::disable_same_conversion_t<Handler, completion_handler<ReturnType(Args...)>>>
auto completion_handler<ReturnType(Args...)>::operator=(Handler&& handler) -> decltype(*this)
{
    ptr_ = make_eraser(std::forward<Handler>(handler));
    return *this;
}

template <typename ReturnType, typename... Args>
auto completion_handler<ReturnType(Args...)>::operator=(std::nullptr_t) noexcept -> decltype(*this)
{
    ptr_.reset();
    return *this;
}

template <typename ReturnType, typename... Args>
void completion_handler<ReturnType(Args...)>::swap(completion_handler& other) noexcept
{
    using std::swap;
    swap(ptr_, other.ptr_);
}

template <typename ReturnType, typename... Args>
template <typename... DeducedArgs>
ReturnType completion_handler<ReturnType(Args...)>::invoke(DeducedArgs&&... args)
{
    if (!ptr_)
    {
        throw std::bad_function_call{};
    }

    return ptr_->invoke(ptr_, std::forward<DeducedArgs>(args)...);
}

template <typename ReturnType, typename... Args>
bool operator==(completion_handler<ReturnType(Args...)> const& lhs, std::nullptr_t rhs) noexcept
{
    return lhs.ptr_ == rhs;
}

template <typename ReturnType, typename... Args>
bool operator==(std::nullptr_t lhs, completion_handler<ReturnType(Args...)> const& rhs) noexcept
{
    return rhs == lhs;
}

template <typename ReturnType, typename... Args>
bool operator!=(completion_handler<ReturnType(Args...)> const& lhs, std::nullptr_t rhs) noexcept
{
    return !(lhs == rhs);
}

template <typename ReturnType, typename... Args>
bool operator!=(std::nullptr_t lhs, completion_handler<ReturnType(Args...)> const& rhs) noexcept
{
    return !(rhs == lhs);
}

template <typename ReturnType, typename... Args>
void swap(completion_handler<ReturnType(Args...)>& lhs, completion_handler<ReturnType(Args...)>& rhs) noexcept
{
    return lhs.swap(rhs);
}

} // namespace netu

#endif // NETU_COMPLETION_HANDLER_HPP
