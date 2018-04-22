//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/synchronized_value.hpp>

#include <boost/noncopyable.hpp>
#include <boost/test/unit_test.hpp>

#include <unordered_set>

namespace netu
{

namespace
{

std::unordered_set<void*> lock_set;

struct fake_basic_lockable : boost::noncopyable
{
    void lock()
    {
        auto pair = lock_set.insert(this);
        BOOST_ASSERT(pair.second);
    }

    void unlock() noexcept
    {
        auto erased = lock_set.erase(this);
        BOOST_ASSERT(erased == 1);
    }

    ~fake_basic_lockable()
    {
        auto count = lock_set.count(this);
        BOOST_ASSERT(count == 0);
    }
};

struct fake_lockable : boost::noncopyable
{
    void lock()
    {
        auto pair = lock_set.insert(this);
        BOOST_ASSERT(pair.second);
    }

    bool try_lock()
    {
        lock();
        return true;
    }

    void unlock() noexcept
    {
        auto erased = lock_set.erase(this);
        BOOST_ASSERT(erased == 1);
    }

    ~fake_lockable()
    {
        auto count = lock_set.count(this);
        BOOST_ASSERT(count == 0);
    }
};

} // namespace

BOOST_AUTO_TEST_CASE(single_value_apply)
{
    synchronized_value<int, fake_basic_lockable> sv1{42};
    synchronized_value<int, fake_basic_lockable> sv2{43};

    auto v = apply(
      [](int& v) {
          BOOST_TEST(lock_set.size() == 1);
          return v;
      },
      sv1);
    BOOST_TEST(v == 42);
    BOOST_TEST(lock_set.empty());

    v = apply(
      [](int const& v) {
          BOOST_TEST(lock_set.size() == 1);
          return v;
      },
      static_cast<decltype(sv2) const &>(sv2));
    BOOST_TEST(v == 43);
    BOOST_TEST(lock_set.empty());
}

BOOST_AUTO_TEST_CASE(multi_value_apply)
{
    synchronized_value<int, fake_lockable> sv1{42};
    synchronized_value<int, fake_lockable> sv2{43};

    auto v = apply(
      [](int& v1, int& v2) {
          BOOST_TEST(lock_set.size() == 2);
          auto v = v1;
          v1 = v2;
          return v;
      },
      sv1,
      sv2);
    BOOST_TEST(v == 42);

    v = apply(
      [](int const& v1, int const& v2) {
          BOOST_TEST(lock_set.size() == 2);
          return v1 + v2;
      },
      static_cast<decltype(sv1) const &>(sv1),
      static_cast<decltype(sv2) const &>(sv2));

    BOOST_TEST(v == 43 * 2);
}

} // namespace netu
