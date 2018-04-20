#include <netu/test/stream.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(constructors)
{
    boost::asio::io_context ctx;
    netu::test::stream stream{ctx};
}
