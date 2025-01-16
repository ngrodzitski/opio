#include <numeric>
#include <iostream>

#define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 0  // NOLINT

#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

#define OPIO_NET_CONNECTION_TEST_NAME( name ) SyncWriteHeuristicZero##name

#include "connection_tests.ipp"
