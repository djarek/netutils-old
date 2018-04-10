//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <cstdint>
#include <new>
#include <stdexcept>

namespace netu
{

namespace test
{

struct allocation_failure : std::bad_alloc
{
};

struct construction_failure : std::runtime_error
{
    explicit construction_failure(const char* str)
      : std::runtime_error{str}
    {
    }
};

struct allocator_control
{
    std::size_t allocatons_left = 0;
    std::size_t constructions_left = 0;
    std::size_t destructions = 0;
    std::size_t deallocations = 0;
};

template<typename T>
struct allocator
{
    using value_type = T;
    using pointer = value_type*;

    template<typename U>
    explicit allocator(allocator<U> const& other)
      : ctrl_{other.ctrl_}
    {
    }

    explicit allocator(allocator_control& ctrl)
      : ctrl_{&ctrl}
    {
    }

    pointer allocate(std::size_t n)
    {
        if (ctrl_->allocatons_left == 0)
        {
            throw allocation_failure{};
        }
        ctrl_->allocatons_left--;
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    template<typename... Args>
    void construct(pointer p, Args&&... args)
    {
        if (ctrl_->constructions_left == 0)
        {
            throw construction_failure{"ctrl_->constructions_left == 0"};
        }
        new (p) T{std::forward<Args>(args)...};
        ctrl_->constructions_left--;
    }

    void destroy(pointer p) noexcept
    {
        p->~T();
        ctrl_->destructions++;
    }

    void deallocate(pointer p, std::size_t /*n*/) noexcept
    {
        ::operator delete(p);
        ctrl_->deallocations++;
    }

    allocator_control* ctrl_;
};

} // namespace test
} // namespace netu