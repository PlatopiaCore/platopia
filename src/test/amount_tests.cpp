// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(amount_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(AmountTests) {
    BOOST_CHECK(CAmount(2) <= CAmount(2));
    BOOST_CHECK(CAmount(2) <= CAmount(3));

    BOOST_CHECK(CAmount(2) >= CAmount(2));
    BOOST_CHECK(CAmount(3) >= CAmount(2));

    BOOST_CHECK(CAmount(1) < CAmount(2));
    BOOST_CHECK(CAmount(-1) < CAmount(0));

    BOOST_CHECK(CAmount(2) > CAmount(1));
    BOOST_CHECK(CAmount(0) > CAmount(-1));

    BOOST_CHECK(CAmount(1) < CAmount(2));
    BOOST_CHECK(CAmount(-1) < CAmount(0));

    BOOST_CHECK(CAmount(2) > CAmount(1));
    BOOST_CHECK(CAmount(0) > CAmount(-1));

    BOOST_CHECK(CAmount(0) == CAmount(0));
    BOOST_CHECK(CAmount(0) != CAmount(1));

    CAmount amount(0);
    BOOST_CHECK_EQUAL(amount += CAmount(1), CAmount(1));
    BOOST_CHECK_EQUAL(amount += CAmount(-1), CAmount(0));
    BOOST_CHECK_EQUAL(amount -= CAmount(1), CAmount(-1));
    BOOST_CHECK_EQUAL(amount -= CAmount(-1), CAmount(0));

    BOOST_CHECK_EQUAL(COIN + COIN, CAmount(2 * COIN));
    BOOST_CHECK_EQUAL(2 * COIN + COIN, CAmount(3 * COIN));
    BOOST_CHECK_EQUAL(-1 * COIN + COIN, CAmount(0));

    BOOST_CHECK_EQUAL(COIN - COIN, CAmount(0));
    BOOST_CHECK_EQUAL(COIN - 2 * COIN, -1 * COIN);

    BOOST_CHECK_EQUAL(10 * CAmount(10), CAmount(100));
    BOOST_CHECK_EQUAL(-1 * CAmount(1), CAmount(-1));

    BOOST_CHECK_EQUAL(CAmount(10) / 3, CAmount(3));
    BOOST_CHECK_EQUAL(10 * COIN / COIN, 10.0);
    BOOST_CHECK_EQUAL(CAmount(10) / -3, CAmount(-3));
    BOOST_CHECK_EQUAL(-10 * COIN / (-1 * COIN), 10.0);

    BOOST_CHECK_EQUAL(CAmount(100) / 10, CAmount(10));
    BOOST_CHECK_EQUAL(CAmount(100) / 3, CAmount(33));
    BOOST_CHECK_EQUAL(CAmount(101) / 3, CAmount(33));

    BOOST_CHECK_EQUAL(CAmount(100) % 10, CAmount(0));
    BOOST_CHECK_EQUAL(CAmount(100) % 3, CAmount(1));
    BOOST_CHECK_EQUAL(CAmount(101) % 3, CAmount(2));
}

BOOST_AUTO_TEST_CASE(GetFeeTest) {
    CFeeRate feeRate;

    feeRate = CFeeRate(CAmount(0));
    // Must always return 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e5), CAmount(0));

    feeRate = CFeeRate(CAmount(1000));
    // Must always just return the arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1000), CAmount(1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9000), CAmount(9000));

    feeRate = CFeeRate(CAmount(-1000));
    // Must always just return -1 * arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(-1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(-121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(-999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1000), CAmount(-1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9000), CAmount(-9000));

    feeRate = CFeeRate(CAmount(123));
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    // Special case: returns 1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(14));
    BOOST_CHECK_EQUAL(feeRate.GetFee(122), CAmount(15));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(122));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1000), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9000), CAmount(1107));

    feeRate = CFeeRate(CAmount(-123));
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    // Special case: returns -1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(-1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(-1));

    // Check full constructor
    // default value
    BOOST_CHECK(CFeeRate(CAmount(-1), 1000) == CFeeRate(CAmount(-1)));
    BOOST_CHECK(CFeeRate(CAmount(0), 1000) == CFeeRate(CAmount(0)));
    BOOST_CHECK(CFeeRate(CAmount(1), 1000) == CFeeRate(CAmount(1)));
    // lost precision (can only resolve satoshis per kB)
    BOOST_CHECK(CFeeRate(CAmount(1), 1001) == CFeeRate(CAmount(0)));
    BOOST_CHECK(CFeeRate(CAmount(2), 1001) == CFeeRate(CAmount(1)));
    // some more integer checks
    BOOST_CHECK(CFeeRate(CAmount(26), 789) == CFeeRate(CAmount(32)));
    BOOST_CHECK(CFeeRate(CAmount(27), 789) == CFeeRate(CAmount(34)));
    // Maximum size in bytes, should not crash
    CFeeRate(MAX_MONEY, std::numeric_limits<size_t>::max() >> 1).GetFeePerK();
}

BOOST_AUTO_TEST_SUITE_END()
