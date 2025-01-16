#include <numeric>
#include <iostream>

#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

#define OPIO_NET_CONNECTION_TEST_NAME( name ) name

#include "connection_tests.ipp"
