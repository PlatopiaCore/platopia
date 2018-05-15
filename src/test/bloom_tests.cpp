// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bloom.h"

#include "base58.h"
#include "clientversion.h"
#include "key.h"
#include "merkleblock.h"
#include "random.h"
#include "serialize.h"
#include "streams.h"
#include "test/test_bitcoin.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "streams.h"

#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/tuple/tuple.hpp>

BOOST_FIXTURE_TEST_SUITE(bloom_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize) {
    CBloomFilter filter(3, 0.01, 0, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")),
        "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")),
        "Bloom filter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("03614e9b050000000000000001");
    std::vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());

    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    filter.clear();
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter should be empty!");
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize_with_tweak) {
    // Same test as bloom_create_insert_serialize, but we add a nTweak of 100
    CBloomFilter filter(3, 0.01, 2147483649UL, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")),
        "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")),
        "Bloom filter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("03ce4299050000000100008001");
    std::vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_key) {
    std::string strSecret =
        std::string("5Kg1gnAjaLfKiwhhPpGS3QfRg2m6awQvaj98JCZBZQ5SuS2F15C");
    CBitcoinSecret vchSecret;
    BOOST_CHECK(vchSecret.SetString(strSecret));

    CKey key = vchSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    std::vector<uint8_t> vchPubKey(pubkey.begin(), pubkey.end());

    CBloomFilter filter(2, 0.001, 0, BLOOM_UPDATE_ALL);
    filter.insert(vchPubKey);
    uint160 hash = pubkey.GetID();
    filter.insert(std::vector<uint8_t>(hash.begin(), hash.end()));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("038fc16b080000000000000001");
    std::vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_match) {
    // Random real transaction
    // (2bba3ba7beb43aeb1eb6a9f07f46f5ec8539fefd1e42704831c4a9cc02c951e6)
    CDataStream stream(
        ParseHex("010001b3700e7883713e6d331469f96cf031a8a56765b9c4ac50e3c87fba6"
                "2f700a5a60091ced9de406a473044022016bdfa9074c4fd8186a85f442048e"
                "1e06a662f245a1595d6da3242504e7b4389022035ffb0facacb2aad1beeccf"
                "ae475d3a22b111ad182e822aa1f371d08b6e809b9012102b01f83148175be4"
                "52662058d06ec0b39bd67608400b573e3584d23b8a86ec15f0191ced3d1200"
                "01976a914f11786e49acf394dac3638236f3ea823653c4f3d88ac0000"),
        SER_DISK, CLIENT_VERSION);
    CTransaction tx(deserialize, stream);

    // and one which spends it
    // (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    uint8_t ch[] = {
        0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65,
        0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8,
        0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74,
        0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00,
        0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77,
        0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f,
        0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2,
        0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e,
        0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4,
        0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2,
        0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a,
        0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34,
        0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97,
        0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b,
        0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f,
        0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00,
        0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef,
        0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39,
        0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93,
        0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43,
        0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> vch(ch, ch + sizeof(ch) - 1);
    CDataStream spendStream(vch, SER_DISK, CLIENT_VERSION);
    CTransaction spendingTx(deserialize, spendStream);

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S(
        "0x2bba3ba7beb43aeb1eb6a9f07f46f5ec8539fefd1e42704831c4a9cc02c951e6"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // byte-reversed tx hash
    filter.insert(ParseHex(
        "e651c902cca9c4314870421efdfe3985ecf5467ff0a9b61eeb3ab4bea73bba2b"));
    BOOST_CHECK_MESSAGE(
        filter.IsRelevantAndUpdate(tx),
        "Simple Bloom filter didn't match manually serialized tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("3044022016bdfa9074c4fd8186a85f442048e1e06a662f245a15"
            "95d6da3242504e7b4389022035ffb0facacb2aad1beeccfae475d3a22b111ad182e"
            "822aa1f371d08b6e809b901"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match input signature");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("02b01f83148175be452662058d06ec0b39bd67608400b573e35"
            "84d23b8a86ec15f"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match input pub key");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("f11786e49acf394dac3638236f3ea823653c4f3d"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match output address");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0xa6a500f762ba7fc8e350acc"
            "4b96567a5a831f06cf96914336d3e7183780e70b3"),
                            0));

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    COutPoint prevOutPoint(uint256S("0xa6a500f762ba7fc8e350acc"
            "4b96567a5a831f06cf96914336d3e7183780e70b3"), 0, 4999000000);
    {
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << prevOutPoint;
        std::vector<uint8_t> data(stream.begin(), stream.end());
        filter.insert(data);
    }
    BOOST_CHECK_MESSAGE(
        filter.IsRelevantAndUpdate(tx),
        "Simple Bloom filter didn't match manually serialized COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S(
        "00000009e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched random tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("0000006d2965547608b9e15d9032a7b9d64fa431"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched random address");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0x90c122d70786e899529d71dbeba91ba216982fb"
                                     "6ba58f3bdaab65e73b7e9260b"),
                            1));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched COutPoint for an output "
                        "we didn't care about");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0x000000d70786e899529d71dbeba91ba216982fb"
                                     "6ba58f3bdaab65e73b7e9260b"),
                            0));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched COutPoint for an output "
                        "we didn't care about");
}

static std::vector<uint8_t> RandomData() {
    uint256 r = GetRandHash();
    return std::vector<uint8_t>(r.begin(), r.end());
}

BOOST_AUTO_TEST_CASE(rolling_bloom) {
    // last-100-entry, 1% false positive:
    CRollingBloomFilter rb1(100, 0.01);

    // Overfill:
    static const int DATASIZE = 399;
    std::vector<uint8_t> data[DATASIZE];
    for (int i = 0; i < DATASIZE; i++) {
        data[i] = RandomData();
        rb1.insert(data[i]);
    }
    // Last 100 guaranteed to be remembered:
    for (int i = 299; i < DATASIZE; i++) {
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // false positive rate is 1%, so we should get about 100 hits if
    // testing 10,000 random keys. We get worst-case false positive
    // behavior when the filter is as full as possible, which is
    // when we've inserted one minus an integer multiple of nElement*2.
    unsigned int nHits = 0;
    for (int i = 0; i < 10000; i++) {
        if (rb1.contains(RandomData())) ++nHits;
    }
    // Run test_bitcoin with --log_level=message to see BOOST_TEST_MESSAGEs:
    BOOST_TEST_MESSAGE("RollingBloomFilter got "
                       << nHits << " false positives (~100 expected)");

    // Insanely unlikely to get a fp count outside this range:
    BOOST_CHECK(nHits > 25);
    BOOST_CHECK(nHits < 175);

    BOOST_CHECK(rb1.contains(data[DATASIZE - 1]));
    rb1.reset();
    BOOST_CHECK(!rb1.contains(data[DATASIZE - 1]));

    // Now roll through data, make sure last 100 entries
    // are always remembered:
    for (int i = 0; i < DATASIZE; i++) {
        if (i >= 100) BOOST_CHECK(rb1.contains(data[i - 100]));
        rb1.insert(data[i]);
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // Insert 999 more random entries:
    for (int i = 0; i < 999; i++) {
        std::vector<uint8_t> d = RandomData();
        rb1.insert(d);
        BOOST_CHECK(rb1.contains(d));
    }
    // Sanity check to make sure the filter isn't just filling up:
    nHits = 0;
    for (int i = 0; i < DATASIZE; i++) {
        if (rb1.contains(data[i])) ++nHits;
    }
    // Expect about 5 false positives, more than 100 means
    // something is definitely broken.
    BOOST_TEST_MESSAGE("RollingBloomFilter got "
                       << nHits << " false positives (~5 expected)");
    BOOST_CHECK(nHits < 100);

    // last-1000-entry, 0.01% false positive:
    CRollingBloomFilter rb2(1000, 0.001);
    for (int i = 0; i < DATASIZE; i++) {
        rb2.insert(data[i]);
    }
    // ... room for all of them:
    for (int i = 0; i < DATASIZE; i++) {
        BOOST_CHECK(rb2.contains(data[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()
