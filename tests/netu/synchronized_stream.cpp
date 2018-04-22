//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/djarek/netutils
//

#include <netu/synchronized_stream.hpp>

#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>

namespace netu
{

using test_stream_t = boost::asio::local::stream_protocol::socket;

struct synchronized_stream_ctx_fixture
{
    synchronized_stream_ctx_fixture()
    {
        boost::asio::local::connect_pair(stream1_.lowest_layer(),
                                         stream2_.lowest_layer());
    }

    boost::asio::io_context ctx_;
    synchronized_stream<test_stream_t> stream1_{ctx_};
    synchronized_stream<test_stream_t> stream2_{ctx_};
};

struct synchronized_stream_piecewise_fixture
{
    synchronized_stream_piecewise_fixture()
    {
        boost::asio::local::connect_pair(stream1_.lowest_layer(),
                                         stream2_.lowest_layer());
    }

    boost::asio::io_context ctx_;
    synchronized_stream<test_stream_t> stream1_{
      std::piecewise_construct,
      std::forward_as_tuple(ctx_),
      std::forward_as_tuple(ctx_.get_executor())};
    synchronized_stream<test_stream_t> stream2_{
      std::piecewise_construct,
      std::forward_as_tuple(ctx_),
      std::forward_as_tuple(ctx_.get_executor())};
};

struct synchronized_stream_two_arg_fixture
{
    synchronized_stream_two_arg_fixture()
    {
        boost::asio::local::connect_pair(stream1_.lowest_layer(),
                                         stream2_.lowest_layer());
    }

    boost::asio::io_context ctx_;
    synchronized_stream<test_stream_t> stream1_{ctx_, ctx_.get_executor()};
    synchronized_stream<test_stream_t> stream2_{ctx_, ctx_.get_executor()};
};

using fixture_list_t = boost::mpl::list<synchronized_stream_ctx_fixture,
                                        synchronized_stream_piecewise_fixture,
                                        synchronized_stream_two_arg_fixture>;

BOOST_AUTO_TEST_CASE_TEMPLATE(read, Fixture, fixture_list_t)
{
    Fixture f;
    const std::string str{"test"};
    bool ran_in_write_strand = false;
    f.stream1_.async_write_some(
      boost::asio::const_buffer(str.data(), str.size()),
      [&f, &str, &ran_in_write_strand](boost::system::error_code ec,
                                       std::size_t n) {
          ran_in_write_strand =
            f.stream1_.get_executor().running_in_this_thread();
          BOOST_TEST(!ec);
          BOOST_TEST(n == 4);
      });

    std::string rb = "1234";
    bool ran_in_read_strand = false;
    f.stream2_.async_read_some(
      boost::asio::buffer(rb),
      [&f, &ran_in_read_strand](boost::system::error_code ec, std::size_t n) {
          ran_in_read_strand =
            f.stream2_.get_executor().running_in_this_thread();
          BOOST_TEST(!ec);
          BOOST_TEST(n == 4);
      });
    f.ctx_.run();

    BOOST_TEST(ran_in_write_strand);
    BOOST_TEST(ran_in_read_strand);
    BOOST_TEST(rb == "test");
}

} // namespace netu
