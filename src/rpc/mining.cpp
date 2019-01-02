// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "config.h"
#include "consensus/consensus.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "dstencode.h"
#include "init.h"
#include "miner.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "rpc/blockchain.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util.h"
#include "ethash/ethash.h"
#include "ethash/internal.h"
#include "utilstrencodings.h"
#include "validation.h"
#include "validationinterface.h"

#include <univalue.h>

#include <cstdint>
#include <memory>

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive. If 'height' is
 * nonnegative, compute the estimate at the time when a given block was found.
 */
static UniValue GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height()) {
        pb = chainActive[height];
    }

    if (pb == nullptr || !pb->nHeight) {
        return 0;
    }

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0) {
        lookup = pb->nHeight %
                     Params().GetConsensus().DifficultyAdjustmentInterval() +
                 1;
    }

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight) {
        lookup = pb->nHeight;
    }

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a
    // divide by zero exception.
    if (minTime == maxTime) {
        return 0;
    }

    arith_uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return workDiff.getdouble() / timeDiff;
}

static UniValue getnetworkhashps(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "getnetworkhashps ( nblocks height )\n"
            "\nReturns the estimated network hashes per second based on the "
            "last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies since last "
            "difficulty change.\n"
            "Pass in [height] to estimate the network speed at the time when a "
            "certain block was found.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, optional, default=120) The number of "
            "blocks, or -1 for blocks since last difficulty change.\n"
            "2. height      (numeric, optional, default=-1) To estimate at the "
            "time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n" +
            HelpExampleCli("getnetworkhashps", "") +
            HelpExampleRpc("getnetworkhashps", ""));
    }

    LOCK(cs_main);
    return GetNetworkHashPS(
        request.params.size() > 0 ? request.params[0].get_int() : 120,
        request.params.size() > 1 ? request.params[1].get_int() : -1);
}

static UniValue generateBlocks(const Config &config,
                               std::shared_ptr<CReserveScript> coinbaseScript,
                               int nGenerate, uint64_t nMaxTries,
                               bool keepScript) {
    static const int nInnerLoopCount = 0x10000000;
    int nHeightStart = 0;
    int nHeightEnd = 0;
    int nHeight = 0;

    ethash_light_t light_ethash = NULL;
    ethash_full_t full_ethash = NULL;
    int epochs = 0;

    {
        // Don't keep cs_main locked.
        LOCK(cs_main);
        nHeightStart = chainActive.Height();
        nHeight      = nHeightStart;
        nHeightEnd   = nHeightStart + nGenerate;
    }

    if (light_ethash == NULL) {
        light_ethash = ethash_light_new(nHeight);
    }

    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    while (nHeight < nHeightEnd) {
        std::unique_ptr<CBlockTemplate> pblocktemplate(
            BlockAssembler(config, Params())
                .CreateNewBlock(coinbaseScript->reserveScript));

        if (!pblocktemplate.get()) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        }

        if (epochs != chainActive.Height() / ETHASH_EPOCH_LENGTH) {
            light_ethash = ethash_light_new(chainActive.Height());
            full_ethash = ethash_full_new(light_ethash, NULL);
            epochs = chainActive.Height() / ETHASH_EPOCH_LENGTH;
        }
        if (full_ethash == NULL) {
            full_ethash = ethash_full_new(light_ethash, NULL);
            if (full_ethash == NULL) {
                LogPrintf("full_ethash init failed\n");
                continue;
            }
        }

        CBlock *pblock = &pblocktemplate->block;

        {
            LOCK(cs_main);
            IncrementExtraNonce(config, pblock, chainActive.Tip(), nExtraNonce);
        }

        ethash_h256_t thash;

        CBlockHeaderBase bBlock(*pblock);

        bool fNegative;
        bool fOverflow;

        arith_uint256 bnTarget;

        bnTarget.SetCompact(bBlock.nBits, &fNegative, &fOverflow);

        // Check range
        if (fNegative || bnTarget == 0 || fOverflow ||
            bnTarget >
                UintToArith256(config.GetChainParams().GetConsensus().powLimit)) {
            return false;
        }

        ethash_h256_t boundary = bnTarget.ToEthashH256();
        thash = bBlock.GetEthash();

        while (true) {
            // Yes, there is a chance every nonce could fail to satisfy the -regtest
            // target -- 1 in 2^(2^32). That ain't gonna happen.
            if (full_ethash == NULL) {
                LogPrintf("full_ethash invalid\n");
                break;
            }
            ethash_return_value_t ret = ethash_full_compute(full_ethash, thash, pblock->nNonce);

            if (ethash_quick_check_difficulty( &thash, pblock->nNonce, &(ret.mix_hash), &boundary )) {
                // Found a solution
                pblock->hashMix = ret.mix_hash;
                break;
            }
            ++pblock->nNonce;
            --nMaxTries;
        }

        if (nMaxTries == 0) {
            break;
        }

        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }

        std::shared_ptr<const CBlock> shared_pblock =
            std::make_shared<const CBlock>(*pblock);
        if (!ProcessNewBlock(config, shared_pblock, true, nullptr)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR,
                               "ProcessNewBlock, block not accepted");
        }
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

        // Mark script as important because it was used at least for one
        // coinbase output if the script came from the wallet.
        if (keepScript) {
            coinbaseScript->KeepScript();
        }
    }

    return blockHashes;
}

static UniValue generate(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            "generate nblocks ( maxtries )\n"
            "\nMine up to nblocks blocks immediately (before the RPC call "
            "returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated "
            "immediately.\n"
            "2. maxtries     (numeric, optional) How many iterations to try "
            "(default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n" +
            HelpExampleCli("generate", "11"));
    }

    int nGenerate = request.params[0].get_int();
    uint64_t nMaxTries = 1000000;
    if (request.params.size() > 1) {
        nMaxTries = request.params[1].get_int();
    }

    std::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    // If the keypool is exhausted, no script is returned at all. Catch this.
    if (!coinbaseScript) {
        throw JSONRPCError(
            RPC_WALLET_KEYPOOL_RAN_OUT,
            "Error: Keypool ran out, please call keypoolrefill first");
    }

    // Throw an error if no script was provided.
    if (coinbaseScript->reserveScript.empty()) {
        throw JSONRPCError(
            RPC_INTERNAL_ERROR,
            "No coinbase script available (mining requires a wallet)");
    }


    UniValue blockHashes(UniValue::VARR);
    //std::vector<uint256> hashes = mineworker->MineBlocks(nGenerate);
    std::vector<uint256> hashes = mineworker->MineBlocks(coinbaseScript, nGenerate, nMaxTries, true);
    for (auto it = hashes.cbegin(); it != hashes.cend(); it++){
        blockHashes.push_back((*it).GetHex());
    }

    return blockHashes;
    //return generateBlocks(config, coinbaseScript, nGenerate, nMaxTries, true);
}

static UniValue generatetoaddress(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            "generatetoaddress nblocks address (maxtries)\n"
            "\nMine blocks immediately to a specified address (before the RPC "
            "call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated "
            "immediately.\n"
            "2. address      (string, required) The address to send the newly "
            "generated bitcoin to.\n"
            "3. maxtries     (numeric, optional) How many iterations to try "
            "(default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks to myaddress\n" +
            HelpExampleCli("generatetoaddress", "11 \"myaddress\""));
    }

    int nGenerate = request.params[0].get_int();
    uint64_t nMaxTries = 1000000;
    if (request.params.size() > 2) {
        nMaxTries = request.params[2].get_int();
    }

    CTxDestination destination = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Error: Invalid address");
    }

    std::shared_ptr<CReserveScript> coinbaseScript(new CReserveScript());
    coinbaseScript->reserveScript = GetScriptForDestination(destination);

    UniValue blockHashes(UniValue::VARR);
    //std::vector<uint256> hashes = mineworker->MineBlocks(nGenerate);
    std::vector<uint256> hashes = mineworker->MineBlocks(coinbaseScript, nGenerate, nMaxTries, false);
    for (auto it = hashes.cbegin(); it != hashes.cend(); it++){
        blockHashes.push_back((*it).GetHex());
    }

    return blockHashes;
    //return generateBlocks(config, coinbaseScript, nGenerate, nMaxTries, false);
}

static UniValue getmininginfo(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block "
            "transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"errors\": \"...\"            (string) Current errors\n"
            "  \"networkhashps\": nnn,      (numeric) The network hashes per "
            "second\n"
            "  \"pooledtx\": n              (numeric) The size of the mempool\n"
            "  \"chain\": \"xxxx\",           (string) current network name as "
            "defined in BIP70 (main, test, regtest)\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmininginfo", "") +
            HelpExampleRpc("getmininginfo", ""));
    }

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("blocks", int(chainActive.Height())));
    obj.push_back(Pair("currentblocksize", uint64_t(nLastBlockSize)));
    obj.push_back(Pair("currentblocktx", uint64_t(nLastBlockTx)));
    obj.push_back(Pair("difficulty", double(GetDifficulty(chainActive.Tip()))));
    obj.push_back(Pair("blockprioritypercentage",
                       uint8_t(GetArg("-blockprioritypercentage",
                                      DEFAULT_BLOCK_PRIORITY_PERCENTAGE))));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    obj.push_back(Pair("networkhashps", getnetworkhashps(config, request)));
    obj.push_back(Pair("pooledtx", uint64_t(mempool.size())));
    obj.push_back(Pair("chain", Params().NetworkIDString()));
    return obj;
}

// NOTE: Unlike wallet RPC (which use BCH values), mining RPCs follow GBT (BIP
// 22) in using satoshi amounts
static UniValue prioritisetransaction(const Config &config,
                                      const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "Accepts the transaction into mined blocks at a higher (or lower) "
            "priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. priority_delta (numeric, required) The priority to add or "
            "subtract.\n"
            "                  The transaction selection algorithm considers "
            "the tx as it would have a higher priority.\n"
            "                  (priority of a transaction is calculated: "
            "coinage * value_in_satoshis / txsize) \n"
            "3. fee_delta      (numeric, required) The fee value (in satoshis) "
            "to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the "
            "algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid "
            "a higher (or lower) fee.\n"
            "\nResult:\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n" +
            HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000") +
            HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000"));
    }

    LOCK(cs_main);

    uint256 hash = ParseHashStr(request.params[0].get_str(), "txid");
    CAmount nAmount(request.params[2].get_int64());

    mempool.PrioritiseTransaction(hash, request.params[0].get_str(),
                                  request.params[1].get_real(), nAmount);
    return true;
}

// NOTE: Assumes a conclusive result; if result is inconclusive, it must be
// handled by caller
static UniValue BIP22ValidationResult(const Config &config,
                                      const CValidationState &state) {
    if (state.IsValid()) {
        return NullUniValue;
    }

    std::string strRejectReason = state.GetRejectReason();
    if (state.IsError()) {
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    }

    if (state.IsInvalid()) {
        if (strRejectReason.empty()) {
            return "rejected";
        }
        return strRejectReason;
    }

    // Should be impossible.
    return "valid?";
}

std::string gbt_vb_name(const Consensus::DeploymentPos pos) {
    const struct BIP9DeploymentInfo &vbinfo = VersionBitsDeploymentInfo[pos];
    std::string s = vbinfo.name;
    if (!vbinfo.gbt_force) {
        s.insert(s.begin(), '!');
    }
    return s;
}

static UniValue estimatefee(const Config &config,
                            const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "estimatefee nblocks\n"
            "\nEstimates the approximate fee per kilobyte needed for a "
            "transaction to begin\n"
            "confirmation within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required)\n"
            "\nResult:\n"
            "n              (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "A negative value is returned if not enough transactions and "
            "blocks\n"
            "have been observed to make an estimate.\n"
            "-1 is always returned for nblocks == 1 as it is impossible to "
            "calculate\n"
            "a fee that is high enough to get reliably included in the next "
            "block.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatefee", "6"));
    }

    RPCTypeCheck(request.params, {UniValue::VNUM});

    int nBlocks = request.params[0].get_int();
    if (nBlocks < 1) {
        nBlocks = 1;
    }

    CFeeRate feeRate = mempool.estimateFee(nBlocks);
    if (feeRate == CFeeRate(CAmount(0))) {
        return -1.0;
    }

    return ValueFromAmount(feeRate.GetFeePerK());
}

static UniValue estimatepriority(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "estimatepriority nblocks\n"
            "\nDEPRECATED. Estimates the approximate priority "
            "a zero-fee transaction needs to begin\n"
            "confirmation within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required)\n"
            "\nResult:\n"
            "n              (numeric) estimated priority\n"
            "\n"
            "A negative value is returned if not enough "
            "transactions and blocks\n"
            "have been observed to make an estimate.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatepriority", "6"));
    }

    RPCTypeCheck(request.params, {UniValue::VNUM});

    int nBlocks = request.params[0].get_int();
    if (nBlocks < 1) {
        nBlocks = 1;
    }

    return mempool.estimatePriority(nBlocks);
}

static UniValue estimatesmartfee(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "estimatesmartfee nblocks\n"
            "\nWARNING: This interface is unstable and may disappear or "
            "change!\n"
            "\nEstimates the approximate fee per kilobyte needed for a "
            "transaction to begin\n"
            "confirmation within nblocks blocks if possible and return the "
            "number of blocks\n"
            "for which the estimate is valid.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "{\n"
            "  \"feerate\" : x.x,     (numeric) estimate fee-per-kilobyte (in "
            "BCH)\n"
            "  \"blocks\" : n         (numeric) block number where estimate "
            "was found\n"
            "}\n"
            "\n"
            "A negative value is returned if not enough transactions and "
            "blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "However it will not return a value below the mempool reject fee.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatesmartfee", "6"));
    }

    RPCTypeCheck(request.params, {UniValue::VNUM});

    int nBlocks = request.params[0].get_int();

    UniValue result(UniValue::VOBJ);
    int answerFound;
    CFeeRate feeRate = mempool.estimateSmartFee(nBlocks, &answerFound);
    result.push_back(Pair("feerate",
                          feeRate == CFeeRate(CAmount(0))
                              ? -1.0
                              : ValueFromAmount(feeRate.GetFeePerK())));
    result.push_back(Pair("blocks", answerFound));
    return result;
}

static UniValue estimatesmartpriority(const Config &config,
                                      const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "estimatesmartpriority nblocks\n"
            "\nDEPRECATED. WARNING: This interface is unstable and may "
            "disappear or change!\n"
            "\nEstimates the approximate priority a zero-fee transaction needs "
            "to begin\n"
            "confirmation within nblocks blocks if possible and return the "
            "number of blocks\n"
            "for which the estimate is valid.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required)\n"
            "\nResult:\n"
            "{\n"
            "  \"priority\" : x.x,    (numeric) estimated priority\n"
            "  \"blocks\" : n         (numeric) block number where estimate "
            "was found\n"
            "}\n"
            "\n"
            "A negative value is returned if not enough transactions and "
            "blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "However if the mempool reject fee is set it will return 1e9 * "
            "MAX_MONEY.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatesmartpriority", "6"));
    }

    RPCTypeCheck(request.params, {UniValue::VNUM});

    int nBlocks = request.params[0].get_int();

    UniValue result(UniValue::VOBJ);
    int answerFound;
    double priority = mempool.estimateSmartPriority(nBlocks, &answerFound);
    result.push_back(Pair("priority", priority));
    result.push_back(Pair("blocks", answerFound));
    return result;
}

static UniValue eth_getWork(const Config &config, const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            "eth_getWork\n"

            "\nResult:\n"
            "[\n"
            "\"0xaaaaaaaaaaaaaaaa\" (string) blockHash\n"
            "\"0x5eed\"   (string) seedHash\n"
            "\"0xb0d2a27\"  boundary\n"
            "]\n"
        );
    }


    std::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    // If the keypool is exhausted, no script is returned at all. Catch this.
    if (!coinbaseScript) {
        throw JSONRPCError(
            RPC_WALLET_KEYPOOL_RAN_OUT,
            "Error: Keypool ran out, please call keypoolrefill first");
    }

    // Throw an error if no script was provided.
    if (coinbaseScript->reserveScript.empty()) {
        throw JSONRPCError(
            RPC_INTERNAL_ERROR,
            "No coinbase script available (mining requires a wallet)");
    }

    auto pwork = mineworker->GetLastNewWork(coinbaseScript, true, true);
    LogPrintf("GetLastWork \n");
    CBlock* pblock = &pwork->block; // pointer for convenience

    ethash_h256_t blockEthash = pwork->blockEthash;
    ethash_h256_t seedHash    = ethash_get_seedhash(pblock->nBlockHeight);
    ethash_h256_t boundary    = pwork->boundary;

    UniValue result(UniValue::VARR);
    result.push_back(ethash_h256_encode(blockEthash));
    result.push_back(ethash_h256_encode(seedHash));
    result.push_back(ethash_h256_encode(boundary));

    //result.push_back(HexStr(BEGIN(blockEthash.b), END(blockEthash.b)));
    //result.push_back(HexStr(BEGIN(seedHash.b),    END(seedHash.b)));
    //result.push_back(HexStr(BEGIN(boundary.b),    END(boundary.b)));
    return result;
}

static UniValue eth_submitWork(const Config &config, const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
            "eth_submitWork\n"
        );
    }

    string hexnonce        = request.params[0].get_str();
    string blockheaderHash = request.params[1].get_str();
    string hexhashmix      = request.params[2].get_str();

    std::stringstream sNonce;
    uint64_t          nonce;
    sNonce << std::hex << hexnonce.substr(2, hexnonce.size()-2);
    sNonce >> nonce;

    ethash_h256_t headerHash = ethash_h256_decode_big(blockheaderHash);
    ethash_h256_t mixhash    = ethash_h256_decode_big(hexhashmix);

    return mineworker->SubmitWork(headerHash, nonce, mixhash);
}

static UniValue eth_submitHashrate(const Config &config, const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() < 2) {
        throw std::runtime_error(
            "eth_submitHashrate hashrate\n"
        );
    }

    string hexHashRate = request.params[0].get_str();

    std::stringstream sRate;
    uint64_t     hashRate;
    sRate << std::hex << hexHashRate.substr(2, hexHashRate.size()-2);
    sRate >> hashRate;

    mineworker->SetHashRate(hashRate);
    return true;
}

// clang-format off
static const CRPCCommand commands[] = {
    //  category   name                     actor (function)       okSafeMode
    //  ---------- ------------------------ ---------------------- ----------
    {"mining",     "eth_getWork",           eth_getWork,           true, {}},
    {"mining",     "eth_submitWork",        eth_submitWork,        true, {"hexnonce", "blockheaderHash", "hexhashmix"}},
    {"mining",     "eth_submitHashrate",    eth_submitHashrate,    true, {"hashrate"}},
    {"mining",     "getnetworkhashps",      getnetworkhashps,      true, {"nblocks", "height"}},
    {"mining",     "getmininginfo",         getmininginfo,         true, {}},
    {"mining",     "prioritisetransaction", prioritisetransaction, true, {"txid", "priority_delta", "fee_delta"}},

    {"generating", "generate",              generate,              true, {"nblocks", "maxtries"}},
    {"generating", "generatetoaddress",     generatetoaddress,     true, {"nblocks", "address", "maxtries"}},

    {"util",       "estimatefee",           estimatefee,           true, {"nblocks"}},
    {"util",       "estimatepriority",      estimatepriority,      true, {"nblocks"}},
    {"util",       "estimatesmartfee",      estimatesmartfee,      true, {"nblocks"}},
    {"util",       "estimatesmartpriority", estimatesmartpriority, true, {"nblocks"}},
};
// clang-format on

void RegisterMiningRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
