// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "net.h"
#include "validation.h"

#include "test/test_bitcoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params &consensusParams) {
    int maxHalvings = 241;
    CAmount nInitialSubsidy = 921.1644 * COIN;

    double previousSubsidy = static_cast<double>(nInitialSubsidy) / 0.9; // for height == 0
    for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
        int nHeight = nHalvings * consensusParams.nSubsidyHalvingInterval + 1;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        previousSubsidy *= 0.9;
        BOOST_CHECK_EQUAL(nSubsidy, static_cast<CAmount>(previousSubsidy));
    }
    BOOST_CHECK_EQUAL(
        GetBlockSubsidy(maxHalvings * consensusParams.nSubsidyHalvingInterval,
                        consensusParams),
        CAmount(0));
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval) {
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test) {
    TestBlockSubsidyHalvings(Params(CBaseChainParams::MAIN).GetConsensus()); // As in main
    TestBlockSubsidyHalvings(Params(CBaseChainParams::REGTEST).GetConsensus());// As in regtest
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test) {
    const Consensus::Params &consensusParams =
        Params(CBaseChainParams::MAIN).GetConsensus();
    CAmount nSum(0);
    for (int nHeight = 0; nHeight < 14000000; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= 921.1644 * COIN);
        nSum += 1000 * nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount(263802041678005000));
}

bool ReturnFalse() {
    return false;
}
bool ReturnTrue() {
    return true;
}

BOOST_AUTO_TEST_CASE(test_combiner_all) {
    boost::signals2::signal<bool(), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
