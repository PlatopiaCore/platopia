/*
 * interest.cpp
 */
#include "base58.h"
#include "chain.h"
#include "coins.h"
#include "config.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "dstencode.h"
#include "init.h"
#include "keystore.h"
#include "merkleblock.h"
#include "net.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "rpc/tojson.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"
#include "txmempool.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif
#include "validation.h"
#include <cstdint>
#include <univalue.h>

static UniValue getinterestinfo(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getinterestinfo\n"
            "\nReturns the current interest taken and remain in the blockchain.\n"
            "\nResult:\n"
            "{\n"
            "  \"total\": 2400000000.00000000,      (numeric) total interest\n"
            "  \"left\": xxx,                       (numeric) left interest\n"
            "  \"leftPercentage\": \"xx%\",         (percentage) left / total\n"
            "  \"currentPeriod\":                   (object) interest info of current period\n"
            "  {\n"
            "    \"total\": xxx,                    (numeric) total interest in current period\n"
            "    \"taken\": xxx,                    (numeric) interest taken in current period\n"
            "    \"takenPercentage\": \"xx%\",      (percentage) interest taken percentage in current period\n"
            "    \"left\": xxx,                     (numeric) interest left in current period\n"
            "    \"leftPercentage\": \"xx%\"        (percentage) interest left percentage in current period\n"
            "  }\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getinterestinfo", "") +
            HelpExampleRpc("getinterestinfo", ""));
    }

    double periodMinInterestRate;
    CAmount periodTotal;
    CAmount periodTaken;
    CAmount totalLeft;
    if(!GetCurrentInterestInfo(periodMinInterestRate, periodTotal, periodTaken, totalLeft))
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
               std::string("Can't get interest info, please retry."));
    }
    int periodTakenPercentage = static_cast<int>(periodTaken * 100 / periodTotal);
    int leftPercentage = static_cast<int>(static_cast<double>(totalLeft) / Params().TotalInterest() * 100);

    UniValue results(UniValue::VOBJ);
    results.push_back(Pair("total", ValueFromAmount(Params().TotalInterest())));
    results.push_back(Pair("left", ValueFromAmount(totalLeft)));
    char percent[20] = {0};
    sprintf(percent, "%d%%", leftPercentage);
    results.push_back(Pair("leftPercentage", UniValue(UniValue::VSTR, string(percent))));

    UniValue period(UniValue::VOBJ);
    period.push_back(Pair("total", ValueFromAmount(periodTotal)));
    period.push_back(Pair("taken", ValueFromAmount(periodTaken)));
    sprintf(percent, "%d%%", periodTakenPercentage);
    period.push_back(Pair("takenPercentage", UniValue(UniValue::VSTR, string(percent))));
    period.push_back(Pair("left", ValueFromAmount(totalLeft)));
    sprintf(percent, "%d%%", 100 - periodTakenPercentage);
    period.push_back(Pair("leftPercentage", UniValue(UniValue::VSTR, string(percent))));
    results.push_back(Pair("currentPeriod", period));

    return results;
}

static UniValue getmyinterest(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getmyinterest\n"
            "\nReturns my locked principal and interest.\n"
            "\nResult:\n"
            "{\n"
            "  \"total\": 2400000000.00000000,      (numeric) total interest\n"
            "  \"left\": xxx,                       (numeric) left interest\n"
            "  \"leftPercentage\": \"xx%\",         (percentage) left / total\n"
            "  \"currentPeriod\":                   (object) interest info of current period\n"
            "  {\n"
            "    \"total\": xxx,                    (numeric) total interest in current period\n"
            "    \"taken\": xxx,                    (numeric) interest taken in current period\n"
            "    \"takenPercentage\": \"xx%\",      (percentage) interest taken percentage in current period\n"
            "    \"left\": xxx,                     (numeric) interest left in current period\n"
            "    \"leftPercentage\": \"xx%\"        (percentage) interest left percentage in current period\n"
            "  }\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmyinterest", "") +
            HelpExampleRpc("getmyinterest", ""));
    }


    CAmount principal(0);
    CAmount interest(0);
    int currentHeight = chainActive.Height();

    std::vector<CTxOutVerbose> vDepositItem;
    pwalletMain->GetAllDeposit(vDepositItem);
    for(std::vector<CTxOutVerbose>::iterator i = vDepositItem.begin(); i != vDepositItem.end(); ++i)
    {
        if(currentHeight - i->height + 1 <= i->nLockTime)
        {
            principal += i->nPrincipal;
            interest += i->nValue - i->nPrincipal;
        }
    }
    UniValue results(UniValue::VOBJ);
    results.push_back(Pair("LockedPrincipal", ValueFromAmount(principal)));
    results.push_back(Pair("LockedInterest", ValueFromAmount(interest)));
    return results;
}

static UniValue getinterestlist(const Config &config,
        const JSONRPCRequest &request) {

    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getinterestlist\n"
            "\nReturns all interest list.\n"
            "\nResult:\n"
            "{\n"
            "  \"lockedDeposit\":       locked deposit transactions\n"
            "  [\n"
            "    {\n"
            "      \"txid\": \"txid\",\n"
            "      \"vout\": n,\n"
            "      \"remianBlocks\": 15360,\n"
            "      \"remainDays\": 16,\n"
            "      \"interestRatePer100Days\": \"1.28571%\",  interest rate for 100 block days\n"
            "      \"principal\": xx,\n"
            "      \"interest\": xx\n"
            "    },\n"
            "    ...\n"
            "  ],\n"
            "  \"finishedDeposit\":     unlocked deposit transactions\n"
            "  [\n"
            "    {\n"
            "      \"txid\": \"txid\",\n"
            "      \"vout\": n,\n"
            "      \"interestRatePer100Days\": \"1.28571%\",  interest rate for 100 block days\n"
            "      \"principal\": xx,\n"
            "      \"interest\": xx\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getinterestlist", "") +
            HelpExampleRpc("getinterestlist", ""));
    }

    const int currentHeight = chainActive.Height();

    std::vector<CTxOutVerbose> vDepositItem;
    pwalletMain->GetAllDeposit(vDepositItem);
    UniValue lockedArray(UniValue::VARR);
    UniValue releasedArray(UniValue::VARR);
    for(std::vector<CTxOutVerbose>::iterator i = vDepositItem.begin(); i != vDepositItem.end(); ++i)
    {
        int remianBlocks = i->nLockTime - (currentHeight - i->height + 1) + 1;
        int remainDays = (remianBlocks + (Params().BlocksPerDay() - 1)) / Params().BlocksPerDay();
        UniValue item(UniValue::VOBJ);
        if(remianBlocks <= 0)
        {
            item.push_back(Pair("txid", i->txid.GetHex()));
            item.push_back(Pair("vout", i->n));
            char interestRateStr[20] = {0};
            sprintf(interestRateStr, "%.5f%%", GetInterestRate(i->nLockTime, i->height) * 100);
            item.push_back(Pair("interestRatePer100Days", interestRateStr));
            item.push_back(Pair("principal", ValueFromAmount(i->nPrincipal)));
            item.push_back(Pair("interest", ValueFromAmount(i->nValue - i->nPrincipal)));

            releasedArray.push_back(item);
        }
        else
        {
            item.push_back(Pair("txid", i->txid.GetHex()));
            item.push_back(Pair("vout", i->n));
            item.push_back(Pair("remianBlocks", remianBlocks));
            item.push_back(Pair("remainDays", remainDays));
            char interestRateStr[20] = {0};
            sprintf(interestRateStr, "%.5f%%", GetInterestRate(i->nLockTime, i->height) * 100);
            item.push_back(Pair("interestRatePer100Days", interestRateStr));
            item.push_back(Pair("principal", ValueFromAmount(i->nPrincipal)));
            item.push_back(Pair("interest", ValueFromAmount(i->nValue - i->nPrincipal)));

            lockedArray.push_back(item);
        }
    }

    UniValue results(UniValue::VOBJ);
    results.push_back(Pair("lockedDeposit", lockedArray));
    results.push_back(Pair("finishedDeposit", releasedArray));
    return results;
}

static UniValue getlockinterest(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "getlockinterest lockdays principal\n"
            "\nGet interest of principal for lockdays.\n"
            "\nArguments:\n"
            "1. \"lockdays\"     (numeric, required) lockdays, value among [16, 32, 64, 128, 256, 512, 1024]\n"
            "２. \"principal\"    (numeric, required) amount to deposit\n"
            "\nResult:\n"
            "{\n"
            "  \"locktime\":locktime,   (numeric) adjusted locktime for interest, may small than given lockdays * 960\n"
            "  \"interest\":interest,   (numeric) interest got\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getlockinterest", "16 123.456") +
            HelpExampleRpc("getlockinterest", "16 123.456"));
    }

    int locktime = request.params[0].get_int() * Params().BlocksPerDay();
    if(locktime <= 0)
    {
        throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Invalid locktime. Locktime must > 0.");
    }
    CAmount principal = AmountFromValue(request.params[1]);
    if(principal <= 0)
    {
        throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Invalid locktime. satoshi must > 0.");
    }

    CAmount interest = GetInterest(principal, locktime, chainActive.Height() + 1);
    int adjustedLockTime = Params().AdjustToLockInterestThreshold(locktime);

    UniValue results(UniValue::VOBJ);
    results.push_back(Pair("locktime", adjustedLockTime));
    results.push_back(Pair("interest", ValueFromAmount(interest)));
    return results;
}


static const CRPCCommand commands[] = {
    //  category            name                      actor (function)        okSafeMode
    //  ------------------- ------------------------  ----------------------  ----------
    { "interest",   "getinterestinfo",      getinterestinfo,    true,   {} },
    { "interest",   "getmyinterest",        getmyinterest,      false,  {} },
    { "interest",   "getinterestlist",      getinterestlist,    false,  {} },
    { "interest",   "getlockinterest",      getlockinterest,    true,   {"lockdays", "principal"} },
};

void RegisterInterestRPCCommands(CRPCTable &t) {
    if (GetBoolArg("-disablewallet", false)) {
        return;
    }

    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
