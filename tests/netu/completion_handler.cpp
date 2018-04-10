//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/completion_handler.hpp>

#include <boost/make_unique.hpp>
#include <boost/test/unit_test.hpp>
#include <netu/test/allocator.hpp>

namespace netu
{

template<typename R, typename... Ts>
std::ostream&
operator<<(std::ostream& stream, completion_handler<R(Ts...)> const& ch)
{
    stream << std::boolalpha << static_cast<bool>(ch);
    return stream;
}

namespace
{
void (*const func_ptr)() = []() {};
} // namespace

struct fat_functor
{
    using allocator_type = test::allocator<fat_functor>;

    std::array<char, 1000> data_{};
    allocator_type alloc_;

    explicit fat_functor(test::allocator<fat_functor> alloc)
      : alloc_{alloc}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return alloc_;
    }

    void operator()()
    {
    }
};

static_assert(std::is_same<boost::asio::associated_allocator_t<fat_functor>,
                           fat_functor::allocator_type>::value,
              "Wrong associated allocator");

BOOST_AUTO_TEST_CASE(constructors)
{
    completion_handler<void(void)> ch{};
    BOOST_TEST(!ch);

    completion_handler<void(void)> ch_nullptr{nullptr};
    BOOST_TEST(!ch_nullptr);

    completion_handler<void(void)> ch_lambda{[]() {}};
    BOOST_TEST(!!ch_lambda);

    completion_handler<void(void)> ch_move{std::move(ch_lambda)};
    BOOST_TEST(!!ch_move);

    completion_handler<void(void)> ch_func_ptr{func_ptr};
    BOOST_TEST(!!ch_func_ptr);

    static_assert(
      !std::is_constructible<completion_handler<void(void)>,
                             completion_handler<void(void)>&>::value,
      "Must not be constructible from a ref");
    static_assert(
      !std::is_constructible<completion_handler<void(void)>,
                             completion_handler<void(void)> const&>::value,
      "Must not be constructible from a const ref");
}

BOOST_AUTO_TEST_CASE(allocation)
{
    test::allocator_control ctrl{};
    fat_functor ff{test::allocator<fat_functor>{ctrl}};
    {
        completion_handler<void(void)> ch;

        BOOST_CHECK_THROW(ch = ff, test::allocation_failure);
        BOOST_TEST(!ch);
        BOOST_TEST(ch == nullptr);
        BOOST_TEST(ctrl.allocatons_left == 0);
        BOOST_TEST(ctrl.constructions_left == 0);
        BOOST_TEST(ctrl.destructions == 0);
        BOOST_TEST(ctrl.deallocations == 0);

        ctrl.allocatons_left = 1;
        BOOST_CHECK_THROW(ch = ff, test::construction_failure);
        BOOST_TEST(!ch);
        BOOST_TEST(ch == nullptr);
        BOOST_TEST(ctrl.allocatons_left == 0);
        BOOST_TEST(ctrl.constructions_left == 0);
        BOOST_TEST(ctrl.destructions == 0);
        BOOST_TEST(ctrl.deallocations == 1);
        ctrl = {};

        ctrl.allocatons_left = 1;
        ctrl.constructions_left = 1;
        ch = ff;
        BOOST_TEST(!!ch);
        BOOST_TEST(ch != nullptr);
        BOOST_TEST(ctrl.allocatons_left == 0);
        BOOST_TEST(ctrl.constructions_left == 0);
        BOOST_TEST(ctrl.destructions == 0);
        BOOST_TEST(ctrl.deallocations == 0);

        ch = nullptr;
        BOOST_TEST(!ch);
        BOOST_TEST(nullptr == ch);
    }

    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);
    BOOST_TEST(ctrl.destructions == 1);
    BOOST_TEST(ctrl.deallocations == 1);
}

BOOST_AUTO_TEST_CASE(assignment)
{
    completion_handler<void(void)> ch;

    ch = []() {};
    BOOST_TEST(!!ch);

    completion_handler<void(void)> ch_move = func_ptr;
    ch_move = std::move(ch);
    BOOST_TEST(!!ch_move);
    BOOST_TEST(!ch);
    ch_move = nullptr;
    BOOST_TEST(!ch_move);
}

BOOST_AUTO_TEST_CASE(invocation)
{
    completion_handler<int()> ch;
    BOOST_CHECK_THROW(ch.invoke(), std::bad_function_call);

    ch = []() { return 0xDEADBEEF; };

    BOOST_TEST(ch.invoke() == 0xDEADBEEF);
    BOOST_TEST(!ch);

    // Invocation with a move-only type
    auto p = boost::make_unique<int>(0xDEADBEEF);
    completion_handler<int(std::unique_ptr<int>)> ch2 =
      [](std::unique_ptr<int> p) {
          BOOST_REQUIRE(p != nullptr);
          BOOST_TEST(*p == 0xDEADBEEF);
          return 0xDEADBEEF;
      };
    ;
    BOOST_TEST(ch2.invoke(std::move(p)) == 0xDEADBEEF);

    auto func = []() -> std::unique_ptr<int> {
        return boost::make_unique<int>(0xC0FFEE);
    };

    completion_handler<std::unique_ptr<int>(void)> ch3 = func;

    p = ch3.invoke();
    BOOST_REQUIRE(p != nullptr);
    BOOST_TEST(*p == 0xC0FFEE);
}

BOOST_AUTO_TEST_CASE(comparison)
{
    completion_handler<void()> ch = func_ptr;
    BOOST_TEST(nullptr != ch);
    BOOST_TEST(ch != nullptr);
    ch = nullptr;
    BOOST_TEST(nullptr == ch);
    BOOST_TEST(ch == nullptr);
}

BOOST_AUTO_TEST_CASE(swap_func)
{
    bool l1_called = false;
    bool l2_called = false;

    completion_handler<void()> ch1 = [&]() { l1_called = true; };
    completion_handler<void()> ch2 = [&]() { l2_called = true; };
    using std::swap;
    swap(ch1, ch2);
    ch1.invoke();
    BOOST_TEST(l2_called == true);
    ch2.invoke();
    BOOST_TEST(l1_called == true);
}

namespace
{
int
incompatible_func(const std::string&)
{
    return 0xDEADBEEF;
}
} // namespace

BOOST_AUTO_TEST_CASE(incompatible_func_ptr)
{
    completion_handler<int(std::string)> ch = incompatible_func;
    auto str = ch.invoke("str");
    BOOST_TEST(str == 0xDEADBEEF);
    BOOST_TEST(ch == nullptr);
}

namespace
{

struct ref_wrapper_functor
{
    int operator()();
};

ref_wrapper_functor rwf;

int
ref_wrapper_functor::operator()()
{
    BOOST_TEST(this == &rwf);
    return 0xDEADBEEF;
}

} // namespace

BOOST_AUTO_TEST_CASE(reference_wrapper)
{
    completion_handler<int()> ch = std::ref(rwf);
    BOOST_TEST(ch != nullptr);
    auto result = ch.invoke();
    BOOST_TEST(result == 0xDEADBEEF);
    BOOST_TEST(ch == nullptr);
}

BOOST_AUTO_TEST_CASE(rvalue_ref)
{
    completion_handler<int(std::unique_ptr<int> &&)> ch;
    ch = [](std::unique_ptr<int> ptr) {
        BOOST_REQUIRE(ptr != nullptr);
        BOOST_TEST(*ptr == 0xC0FFEE);
        return 0xDEADBEEF;
    };

    BOOST_TEST(ch != nullptr);
    auto result = ch.invoke(boost::make_unique<int>(0xC0FFEE));
    BOOST_TEST(result == 0xDEADBEEF);
    BOOST_TEST(ch == nullptr);
}

} // namespace netu
