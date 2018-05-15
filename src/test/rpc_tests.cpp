// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/client.h"
#include "rpc/server.h"

#include "base58.h"
#include "config.h"
#include "netbase.h"

#include "test/test_bitcoin.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

UniValue CallRPC(std::string args) {
    std::vector<std::string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    std::string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    GlobalConfig config;
    JSONRPCRequest request;
    request.strMethod = strMethod;
    request.params = RPCConvertValues(strMethod, vArgs);
    request.fHelp = false;
    BOOST_CHECK(tableRPC[strMethod]);
    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(config, request);
        return result;
    } catch (const UniValue &objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}

BOOST_FIXTURE_TEST_SUITE(rpc_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(rpc_rawparams) {
    // Test raw transaction API argument handling
    UniValue r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction not_hex"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction "
                              "a3b807410df0b60fcb9736768df5823938b2f838694939ba"
                              "45f3c0a1bff150ed not_int"),
                      std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction null null"),
                      std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"),
                      std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {}"),
                      std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction {} {}"),
                      std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] []"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"),
                      std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction DEADBEEF"),
                      std::runtime_error);
    std::string rawtx ="0100012afa837b7c2dbbf6dc4dc42aeae2577326c7ee19b7b452f36"
            "64c6c9e023da5790191cf96e300d9004730440220648ab2b7e436e49008378bb57"
            "bf0d7f2bf09590f9e59481a207238e66f2eb1b1022030e0d35410bdf7823d3f76a"
            "214821e3090fc30799cb24ce47cfeea10f8355d3a0147304402203fed4635b7d16"
            "fe7a0c861f484f86b92d46af8e1815def16788227caea29eb0802200e25b11ca1a"
            "a18f7c15b94623c08df46e3b4e3306344323812547053de45816c014752210256b"
            "f4196053598e1ec0a5e7b7ea9d8cd795afa5a156cfd8b800bed136fad520f2102b"
            "01f83148175be452662058d06ec0b39bd67608400b573e3584d23b8a86ec15f52a"
            "e0191ced9de40001976a9146065083ff437ea8690f0ee9c92ac7cab8e7a7b1a88a"
            "c0000";
    BOOST_CHECK_NO_THROW(
        r = CallRPC(std::string("decoderawtransaction ") + rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "size").get_int(), 294);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_THROW(
        r = CallRPC(std::string("decoderawtransaction ") + rawtx + " extra"),
        std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("signrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction ff00"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ") + rawtx));
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ") + rawtx +
                                 " null null NONE|ANYONECANPAY"));
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ") + rawtx +
                                 " [] [] NONE|ANYONECANPAY"));
    BOOST_CHECK_THROW(CallRPC(std::string("signrawtransaction ") + rawtx +
                              " null null badenum"),
                      std::runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to
    // send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction DEADBEEF"),
                      std::runtime_error);
    BOOST_CHECK_THROW(
        CallRPC(std::string("sendrawtransaction ") + rawtx + " extra"),
        std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_togglenetwork) {
    UniValue r;

    r = CallRPC("getnetworkinfo");
    bool netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive false"));
    r = CallRPC("getnetworkinfo");
    int numConnection = find_value(r.get_obj(), "connections").get_int();
    BOOST_CHECK_EQUAL(numConnection, 0);

    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, false);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive true"));
    r = CallRPC("getnetworkinfo");
    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);
}

BOOST_AUTO_TEST_CASE(rpc_createraw_op_return) {
    BOOST_CHECK_NO_THROW(
        CallRPC("createrawtransaction "
                "[{\"txid\":"
                "\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff1"
                "50ed\",\"vout\":0}] [{\"address\":\"Pa9ErTTTSdEHK4o8LNpbuwudvV28Wcby35\",\"amount\":1.23}]"));

    // Allow more than one data transaction output
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction "
                                 "[{\"txid\":"
                                 "\"a3b807410df0b60fcb9736768df5823938b2f838694"
                                 "939ba45f3c0a1bff150ed\",\"vout\":0}] "
                                 "[{\"address\":\"Pa9ErTTTSdEHK4o8LNpbuwudvV28Wcby35\",\"amount\":1.23}]"));

    // Key not "data" (bad address)
    BOOST_CHECK_THROW(
        CallRPC("createrawtransaction "
                "[{\"txid\":"
                "\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff1"
                "50ed\",\"vout\":0}] {\"somedata\":\"68656c6c6f776f726c64\"}"),
        std::runtime_error);

    // Bad hex encoding of data output
    BOOST_CHECK_THROW(
        CallRPC("createrawtransaction "
                "[{\"txid\":"
                "\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff1"
                 "50ed\",\"vout\":0}] {\"data\":\"12345\"}"),
       std::runtime_error);
    BOOST_CHECK_THROW(
        CallRPC("createrawtransaction "
                "[{\"txid\":"
                "\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff1"
                "50ed\",\"vout\":0}] {\"data\":\"12345g\"}"),
        std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values) {
    BOOST_CHECK(ValueFromAmount(CAmount(0LL)).write() == "0.00000000");
    BOOST_CHECK(ValueFromAmount(CAmount(1LL)).write() == "0.00000001");
    BOOST_CHECK(ValueFromAmount(CAmount(17622195LL)).write() == "0.17622195");
    BOOST_CHECK(ValueFromAmount(CAmount(50000000LL)).write() == "0.50000000");
    BOOST_CHECK(ValueFromAmount(CAmount(89898989LL)).write() == "0.89898989");
    BOOST_CHECK(ValueFromAmount(CAmount(100000000LL)).write() == "1.00000000");
    BOOST_CHECK(ValueFromAmount(CAmount(2099999999999990LL)).write() ==
                "20999999.99999990");
    BOOST_CHECK(ValueFromAmount(CAmount(2099999999999999LL)).write() ==
                "20999999.99999999");

    BOOST_CHECK_EQUAL(ValueFromAmount(CAmount(0)).write(), "0.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(123456789 * (COIN / 10000)).write(),
                      "12345.67890000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-1 * COIN).write(), "-1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-1 * COIN / 10).write(), "-0.10000000");

    BOOST_CHECK_EQUAL(ValueFromAmount(100000000 * COIN).write(),
                      "100000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(10000000 * COIN).write(),
                      "10000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(1000000 * COIN).write(),
                      "1000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(100000 * COIN).write(),
                      "100000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(10000 * COIN).write(), "10000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(1000 * COIN).write(), "1000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(100 * COIN).write(), "100.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(10 * COIN).write(), "10.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN).write(), "1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 10).write(), "0.10000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 100).write(), "0.01000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 1000).write(), "0.00100000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 10000).write(), "0.00010000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 100000).write(), "0.00001000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 1000000).write(), "0.00000100");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 10000000).write(), "0.00000010");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN / 100000000).write(), "0.00000001");
}

static UniValue ValueFromString(const std::string &str) {
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values) {
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("-0.00000001")),
                      UniValue);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0")), CAmount(0LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000000")),
                      CAmount(0LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001")),
                      CAmount(1LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.17622195")),
                      CAmount(17622195LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.5")),
                      CAmount(50000000LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.50000000")),
                      CAmount(50000000LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.89898989")),
                      CAmount(89898989LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1.00000000")),
                      CAmount(100000000LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.9999999")),
                      CAmount(2099999999999990LL));
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.99999999")),
                      CAmount(2099999999999999LL));

    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1e-8")),
                      COIN / 100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.1e-7")),
                      COIN / 100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.01e-6")),
                      COIN / 100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString(
                          "0."
                          "0000000000000000000000000000000000000000000000000000"
                          "000000000000000000000001e+68")),
                      COIN / 100000000);
    BOOST_CHECK_EQUAL(
        AmountFromValue(ValueFromString("10000000000000000000000000000000000000"
                                        "000000000000000000000000000e-64")),
        COIN);
    BOOST_CHECK_EQUAL(
        AmountFromValue(ValueFromString(
            "0."
            "000000000000000000000000000000000000000000000000000000000000000100"
            "000000000000000000000000000000000000000000000000000e64")),
        COIN);

    // should fail
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e-9")), UniValue);
    // should fail
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0.000000019")),
                      UniValue);
    // should pass, cut trailing 0
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001000000")),
                      CAmount(1LL));
    // should fail
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("19e-9")), UniValue);
    // should pass, leading 0 is present
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.19e-6")), CAmount(19));

    // overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("92233720368.54775808")),
                      UniValue);
    // overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e+11")), UniValue);
    // overflow error signless
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e11")), UniValue);
    // overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("93e+9")), UniValue);
}

BOOST_AUTO_TEST_CASE(json_parse_errors) {
    // Valid
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0").get_real(), 1.0);
    // Valid, with leading or trailing whitespace
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue(" 1.0").get_real(), 1.0);
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0 ").get_real(), 1.0);

    // should fail, missing leading 0, therefore invalid JSON
    BOOST_CHECK_THROW(AmountFromValue(ParseNonRFCJSONValue(".19e-6")),
                      std::runtime_error);
    BOOST_CHECK_EQUAL(AmountFromValue(ParseNonRFCJSONValue(
                          "0.00000000000000000000000000000000000001e+30 ")),
                      CAmount(1));
    // Invalid, initial garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("[1.0"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("a1.0"), std::runtime_error);
    // Invalid, trailing garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0sds"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0]"), std::runtime_error);
    // BCH addresses should fail parsing
    BOOST_CHECK_THROW(
        ParseNonRFCJSONValue("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"),
        std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNL"),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_ban) {
    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0 add")));
    // portnumber for setban not allowed
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.0.0:8334")),
                      std::runtime_error);
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    UniValue ar = r.get_array();
    UniValue o1 = ar[0].get_obj();
    UniValue adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/32");
    BOOST_CHECK_NO_THROW(CallRPC(std::string("setban 127.0.0.0 remove")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0UL);

    BOOST_CHECK_NO_THROW(
        r = CallRPC(std::string("setban 127.0.0.0/24 add 1607731200 true")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    UniValue banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    // absolute time check
    BOOST_CHECK_EQUAL(banned_until.get_int64(), 1607731200);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));

    BOOST_CHECK_NO_THROW(
        r = CallRPC(std::string("setban 127.0.0.0/24 add 200")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    int64_t now = GetTime();
    BOOST_CHECK(banned_until.get_int64() > now);
    BOOST_CHECK(banned_until.get_int64() - now <= 200);

    // must throw an exception because 127.0.0.1 is in already banned subnet
    // range
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.0.1 add")),
                      std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("setban 127.0.0.0/24 remove")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0UL);

    BOOST_CHECK_NO_THROW(
        r = CallRPC(std::string("setban 127.0.0.0/255.255.0.0 add")));
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.1.1 add")),
                      std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0UL);

    // invalid IP
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban test add")),
                      std::runtime_error);

    // IPv6 tests
    BOOST_CHECK_NO_THROW(
        r = CallRPC(
            std::string("setban FE80:0000:0000:0000:0202:B3FF:FE1E:8329 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "fe80::202:b3ff:fe1e:8329/128");

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string(
                             "setban 2001:db8::/ffff:fffc:0:0:0:0:0:0 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:db8::/30");

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(
        r = CallRPC(std::string(
            "setban 2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(),
                      "2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128");
}

BOOST_AUTO_TEST_CASE(rpc_convert_values_generatetoaddress) {
    UniValue result;

    BOOST_CHECK_NO_THROW(result = RPCConvertValues(
                             "generatetoaddress",
                             {"101", "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(),
                      "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues(
                             "generatetoaddress",
                             {"101", "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(),
                      "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues(
                             "generatetoaddress",
                             {"1", "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a", "9"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(),
                      "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);

    BOOST_CHECK_NO_THROW(result = RPCConvertValues(
                             "generatetoaddress",
                             {"1", "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU", "9"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(),
                      "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);
}

BOOST_AUTO_TEST_SUITE_END()
