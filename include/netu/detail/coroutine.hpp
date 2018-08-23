//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_DETAIL_COROUTINE_HPP
#define NETU_DETAIL_COROUTINE_HPP

#include <netu/coroutine.hpp>
#include <netu/detail/assume.hpp>

namespace netu
{
namespace detail
{

class coroutine_ref
{
public:
    explicit coroutine_ref(coroutine& c) noexcept
      : coroutine_ref(&c)
    {
    }

    explicit coroutine_ref(coroutine* c) noexcept
      : ref_{c}
    {
    }

    coroutine_ref& operator=(int i)
    {
        ref_->state_ = i;
        release();
        return *this;
    }

    operator int() const
    {
        NETU_ASSUME(ref_ != nullptr);
        return ref_->state_;
    }

    void release()
    {
        (void)ref_.release();
    }

private:
    struct completion_guard
    {
        void operator()(coroutine* c)
        {
            c->state_ = -1;
        }
    };

    std::unique_ptr<coroutine, completion_guard> ref_;
};
} // namespace detail

} // namespace netu

#endif // NETU_DETAIL_COROUTINE_HPP
