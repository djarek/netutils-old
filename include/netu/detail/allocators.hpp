//
// Copyright (c) 2017 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_ALLOCATORS_HPP
#define NETU_DETAIL_ALLOCATORS_HPP

#include <boost/asio/associated_allocator.hpp>

namespace netu
{
namespace detail
{
namespace allocators
{

template <typename T, typename U>
using rebound_alloc_t = typename std::allocator_traits<boost::asio::associated_allocator_t<T>>::template rebind_alloc<U>;

template <typename ReboundType, typename T>
auto rebind_associated(T const& t) noexcept -> rebound_alloc_t<T, ReboundType>
{
    return rebound_alloc_t<T, ReboundType>{boost::asio::get_associated_allocator(t)};
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

    void operator()(pointer p) noexcept
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
} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_ALLOCATORS_HPP
