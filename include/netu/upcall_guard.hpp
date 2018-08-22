//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#ifndef NETU_UPCALL_GUARD_HPP
#define NETU_UPCALL_GUARD_HPP

#include <netu/detail/composed_operation.hpp>

namespace netu
{

// TODO: allow users to disable the use of [[nodiscard]] (causes warnings
// with -pedantic in C++14 or lower)
struct [[nodiscard]] upcall_guard
{
};

} // namespace netu

#endif // NETU_UPCALL_GUARD_HPP
