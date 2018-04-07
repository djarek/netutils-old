//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_ALLOCATORS_HPP
#define NETU_DETAIL_ALLOCATORS_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/core/pointer_traits.hpp>

namespace netu
{
namespace detail
{
namespace allocators
{

template<typename T, typename U>
using rebound_alloc_t = typename std::allocator_traits<
  boost::asio::associated_allocator_t<T>>::template rebind_alloc<U>;

template<typename ReboundType, typename T>
auto
rebind_associated(T const& t) noexcept -> rebound_alloc_t<T, ReboundType>
{
    return rebound_alloc_t<T, ReboundType>{
      boost::asio::get_associated_allocator(t)};
}

template<typename Allocator>
struct deallocator
{
    using pointer = typename std::allocator_traits<Allocator>::pointer;

    template<typename U>
    explicit deallocator(U&& alloc) noexcept
      : alloc_{std::forward<U>(alloc)}
    {
    }

    void operator()(pointer p) noexcept
    {
        std::allocator_traits<Allocator>::deallocate(alloc_, p, 1);
    }

    Allocator alloc_;
};

template<typename Allocator>
struct deleter
{
    using pointer = typename std::allocator_traits<Allocator>::pointer;

    template<typename U>
    explicit deleter(U&& alloc) noexcept
      : alloc_{std::forward<U>(alloc)}
    {
    }

    void operator()(pointer p) noexcept
    {
        std::allocator_traits<Allocator>::destroy(alloc_, p);
        std::allocator_traits<Allocator>::deallocate(alloc_, p, 1);
    }

    Allocator alloc_;
};

template<typename Allocator>
using alloc_ptr =
  std::unique_ptr<typename std::allocator_traits<Allocator>::value_type,
                  deallocator<Allocator>>;

template<typename Allocator>
using allocator_unique_ptr =
  std::unique_ptr<typename std::allocator_traits<Allocator>::value_type,
                  deleter<Allocator>>;

template<typename Allocator>
auto
allocate(Allocator&& alloc) -> alloc_ptr<typename std::decay<Allocator>::type>
{
    using alloc_t = typename std::decay<Allocator>::type;
    return alloc_ptr<alloc_t>{
      std::allocator_traits<alloc_t>::allocate(alloc, 1),
      deallocator<alloc_t>{std::forward<Allocator>(alloc)}};
}

template<typename Allocator, typename... Args>
auto
construct(alloc_ptr<Allocator>&& p, Args&&... args)
  -> allocator_unique_ptr<Allocator>
{
    auto ptr = boost::to_address(p);
    std::allocator_traits<Allocator>::construct(
      p.get_deleter().alloc_, ptr, std::forward<Args>(args)...);
    return allocator_unique_ptr<Allocator>{
      p.release(), deleter<Allocator>{std::move(p.get_deleter().alloc_)}};
}

template<typename Allocator, typename... Args>
auto
allocate_unique(Allocator&& a, Args&&... args)
  -> allocator_unique_ptr<typename std::decay<Allocator>::type>
{
    return netu::detail::allocators::construct(
      netu::detail::allocators::allocate(std::forward<Allocator>(a)),
      std::forward<Args>(args)...);
}

} // namespace allocators
} // namespace detail
} // namespace netu

#endif // NETU_DETAIL_ALLOCATORS_HPP
