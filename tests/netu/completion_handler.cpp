//
// Copyright (c) 2017 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/completion_handler.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/make_unique.hpp>

namespace netu
{

template <typename R, typename... Ts>
std::ostream& operator<<(std::ostream& stream, completion_handler<R(Ts...)> const& ch)
{
    stream << std::boolalpha << static_cast<bool>(ch);
    return stream;
}

namespace
{
void(* const func_ptr)(void) = [](){};

} // namespace

struct allocation_failure : std::bad_alloc
{
};

struct construction_failure : std::runtime_error
{
    construction_failure(const char* str):
        std::runtime_error{str}
    {
    }
};

struct allocator_control
{
    std::size_t allocatons_left = 0;
    std::size_t constructions_left = 0;
};

template <typename T>
struct test_allocator
{
    using value_type = T;
    using pointer = value_type*;

    template <typename U>
    explicit test_allocator(test_allocator<U> const& other):
        ctrl_{other.ctrl_}
    {
    }

    explicit test_allocator(allocator_control& ctrl):
        ctrl_{&ctrl}
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

    template <typename... Args>
    void construct(pointer p, Args&&... args)
    {
        if (ctrl_->constructions_left == 0)
        {
            throw construction_failure{"ctrl_->constructions_left == 0"};
        }
        new (p) T{std::forward<Args>(args)...};
        ctrl_->constructions_left--;
    }

    void deallocate(pointer p, std::size_t)
    {
        ::operator delete (p);
    }

    allocator_control* ctrl_;
};

struct fat_functor
{
    using allocator_type = test_allocator<fat_functor>;

    std::array<char, 1000> data_;
    allocator_type alloc_;

    explicit fat_functor(test_allocator<fat_functor> alloc):
        alloc_{alloc}
    {
    }

    allocator_type get_allocator() const
    {
        return alloc_;
    }

    void operator()()
    {
    }
};

static_assert(std::is_same<boost::asio::associated_allocator_t<fat_functor>, fat_functor::allocator_type>::value,
              "Wrong associated allocator");

BOOST_AUTO_TEST_CASE(constructors)
{
    completion_handler<void(void)> ch {};
    BOOST_TEST(!ch);

    completion_handler<void(void)> ch_lambda {[](){}};
    BOOST_TEST(!!ch_lambda);

    completion_handler<void(void)> ch_move {std::move(ch_lambda)};
    BOOST_TEST(!!ch_move);

    completion_handler<void(void)> ch_func_ptr {func_ptr};
    BOOST_TEST(!!func_ptr);

    static_assert(!std::is_constructible<completion_handler<void(void)>, completion_handler<void(void)>&>::value,
                  "Must not be constructible from a ref");
    static_assert(!std::is_constructible<completion_handler<void(void)>, completion_handler<void(void)> const&>::value,
                  "Must not be constructible from a const ref");
}

BOOST_AUTO_TEST_CASE(allocation)
{
    allocator_control ctrl{};
    fat_functor ff{test_allocator<fat_functor>{ctrl}};
    completion_handler<void(void)> ch;

    BOOST_CHECK_THROW(ch = ff, allocation_failure);
    BOOST_TEST(!ch);
    BOOST_TEST(ch == nullptr);
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);

    ctrl.allocatons_left = 1;
    BOOST_CHECK_THROW(ch = ff, construction_failure);
    BOOST_TEST(!ch);
    BOOST_TEST(ch == nullptr);
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);

    ctrl.allocatons_left = 1;
    ctrl.constructions_left = 1;
    ch = ff;
    BOOST_TEST(!!ch);
    BOOST_TEST(ch != nullptr);
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);

    ch = nullptr;
    BOOST_TEST(!ch);
    BOOST_TEST(nullptr == ch);
}

BOOST_AUTO_TEST_CASE(assignment)
{
    completion_handler<void(void)> ch;

    ch = [](){};
    BOOST_TEST(!!ch);

    completion_handler<void(void)> ch_move;
    ch_move = std::move(ch);
    BOOST_TEST(!!ch_move);
    ch_move = nullptr;
    BOOST_TEST(!ch_move);

    completion_handler<void(void)> ch_func_ptr {func_ptr};
    BOOST_TEST(!!func_ptr);
}

BOOST_AUTO_TEST_CASE(invocation)
{
    completion_handler<int()> ch;
    BOOST_CHECK_THROW(ch.invoke(), std::bad_function_call);

    ch = []()
    {
        return 0xDEADBEEF;
    };

    BOOST_TEST(ch.invoke() == 0xDEADBEEF);
    BOOST_TEST(!ch);

    // Invocation with a move-only type
    auto p = boost::make_unique<int>(0xDEADBEEF);
    completion_handler<int(std::unique_ptr<int>)> ch2 = [](std::unique_ptr<int> p){
        BOOST_REQUIRE(p != nullptr);
        BOOST_TEST(*p == 0xDEADBEEF);
        return 0xDEADBEEF;
    };
    ;
    BOOST_TEST(ch2.invoke(std::move(p)) == 0xDEADBEEF);

    auto func = []() -> std::unique_ptr<int>
    {
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

    completion_handler<void()> ch1 = [&]()
        {
            l1_called = true;
        };
    completion_handler<void()> ch2 = [&]()
        {
            l2_called = true;
        };
    using std::swap;
    swap(ch1, ch2);
    ch1.invoke();
    BOOST_TEST(l2_called == true);
    ch2.invoke();
    BOOST_TEST(l1_called == true);
}

namespace
{
int incompatible_func(const std::string&) { return 0xDEADBEEF; }
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

int ref_wrapper_functor::operator()()
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

} // namespace netu
