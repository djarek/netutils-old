//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/completion_handler_list.hpp>

#include <netu/test/allocator.hpp>

#include <boost/test/unit_test.hpp>

namespace netu
{

std::ostream&
operator<<(std::ostream& ostr,
           completion_handler_list<void(int&)>::released_node const& rn)
{
    ostr << (rn ? "not nullptr" : "nullptr");
    return ostr;
}

template class completion_handler_list<void(int&)>;

BOOST_AUTO_TEST_CASE(push_pop)
{
    int counter = 0;
    completion_handler_list<void(int&)> list;
    list.push_back([](int& i) { i = 42; });
    BOOST_TEST(!list.empty());

    BOOST_TEST(list.size() == 1);

    auto& second = list.back();

    list.push_front([&counter](int const& i) { counter += i; });
    BOOST_TEST(!list.empty());
    BOOST_TEST(list.size() == 2);

    auto& first = list.front();

    BOOST_TEST(&list.front() != &list.back());
    BOOST_TEST(&static_cast<decltype(list) const &>(list).front() == &first);
    BOOST_TEST(&static_cast<decltype(list) const &>(list).back() == &second);

    BOOST_TEST(&first != &second);
    BOOST_TEST(&*list.begin() == &first);
    BOOST_TEST(&*std::prev(list.cend()) == &second);

    auto handler = list.pop_back();
    BOOST_TEST(!!handler);
    BOOST_TEST(handler != nullptr);
    BOOST_TEST(nullptr != handler);

    handler.invoke(counter);

    BOOST_TEST(!handler);
    BOOST_TEST(handler == nullptr);
    BOOST_TEST(nullptr == handler);
    BOOST_TEST(counter == 42);

    handler = list.pop_front();
    BOOST_TEST(!!handler);
    BOOST_TEST(handler != nullptr);

    int i = 1;
    handler.invoke(i);

    BOOST_TEST(!handler);
    BOOST_TEST(handler == nullptr);
    BOOST_TEST(counter == 43);
}

class functor_with_allocator
{
public:
    using allocator_type = test::allocator<void>;

    functor_with_allocator(test::allocator_control& ctrl)
      : ctrl_{&ctrl}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return allocator_type{*ctrl_};
    }

    void operator()(int& i)
    {
        ++i;
    }

private:
    test::allocator_control* ctrl_;
};

BOOST_AUTO_TEST_CASE(handler_with_allocator)
{
    completion_handler_list<void(int&)> list;

    test::allocator_control ctrl{};
    BOOST_CHECK_THROW(list.push_back(functor_with_allocator{ctrl}),
                      test::allocation_failure);
    BOOST_TEST(list.empty());
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);
    BOOST_TEST(ctrl.destructions == 0);
    BOOST_TEST(ctrl.deallocations == 0);

    ctrl.allocatons_left = 1;
    BOOST_CHECK_THROW(list.push_back(functor_with_allocator{ctrl}),
                      test::construction_failure);
    BOOST_TEST(list.empty());
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);
    BOOST_TEST(ctrl.destructions == 0);
    BOOST_TEST(ctrl.deallocations == 1);
    ctrl = {};

    ctrl.allocatons_left = 1;
    ctrl.constructions_left = 1;
    list.push_back(functor_with_allocator{ctrl});
    BOOST_TEST(!list.empty());
    BOOST_TEST(list.size() == 1);
    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);
    BOOST_TEST(ctrl.destructions == 0);
    BOOST_TEST(ctrl.deallocations == 0);

    auto node = list.pop_back();
    node.reset();

    BOOST_TEST(ctrl.allocatons_left == 0);
    BOOST_TEST(ctrl.constructions_left == 0);
    BOOST_TEST(ctrl.destructions == 1);
    BOOST_TEST(ctrl.deallocations == 1);
}

BOOST_AUTO_TEST_CASE(splice)
{
}

} // namespace netu
