// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "config.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "base58.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validation.h"
#include "validationinterface.h"
#include "ethash/internal.h"

#include <algorithm>
#include <queue>
#include <utility>

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

#include "wallet/wallet.h"

using namespace std;

static const int MAX_COINBASE_SCRIPTSIG_SIZE = 100;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

class ScoreCompare {
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b) {
        // Convert to less than.
        return CompareTxMemPoolEntryByScore()(*b, *a);
    }
};

int64_t UpdateTime(CBlockHeader *pblock, const Config &config,
                   const CBlockIndex *pindexPrev) {
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime =
        std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    const Consensus::Params &consensusParams =
        config.GetChainParams().GetConsensus();

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, config);
    }

    return nNewTime - nOldTime;
}

static uint64_t ComputeMaxGeneratedBlockSize(const Config &config,
                                             const CBlockIndex *pindexPrev) {
    // Block resource limits
    // If -blockmaxsize is not given, limit to DEFAULT_MAX_GENERATED_BLOCK_SIZE
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    uint64_t nMaxGeneratedBlockSize = DEFAULT_MAX_GENERATED_BLOCK_SIZE;
    if (IsArgSet("-blockmaxsize")) {
        nMaxGeneratedBlockSize =
            GetArg("-blockmaxsize", DEFAULT_MAX_GENERATED_BLOCK_SIZE);
    }

    // Limit size to between 1K and MaxBlockSize-1K for sanity:
    nMaxGeneratedBlockSize =
        std::max(uint64_t(1000), std::min(config.GetMaxBlockSize() - 1000,
                                          nMaxGeneratedBlockSize));

    return nMaxGeneratedBlockSize;
}

BlockAssembler::BlockAssembler(const Config &_config,
                               const CChainParams &_chainparams)
    : chainparams(_chainparams), config(&_config) {
    if (IsArgSet("-blockmintxfee")) {
        CAmount n(0);
        ParseMoney(GetArg("-blockmintxfee", ""), n);
        blockMinFeeRate = CFeeRate(n);
    } else {
        blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }

    LOCK(cs_main);
    nMaxGeneratedBlockSize =
        ComputeMaxGeneratedBlockSize(*config, chainActive.Tip());
}

void BlockAssembler::resetBlock() {
    inBlock.clear();

    // Reserve space for coinbase tx.
    nBlockSize = 1000;
    nBlockSigOps = 100;

    // These counters do not include coinbase tx.
    nBlockTx = 0;
    nFees = CAmount(0);
    nInterest = CAmount(0);

    lastFewTxs = 0;
    blockFinished = false;
}

static const std::vector<uint8_t>
getExcessiveBlockSizeSig(const Config &config) {
    std::string cbmsg = "/EB" + getSubVersionEB(config.GetMaxBlockSize()) + "/";
    const char *cbcstr = cbmsg.c_str();
    std::vector<uint8_t> vec(cbcstr, cbcstr + cbmsg.size());
    return vec;
}

std::unique_ptr<CBlockTemplate>
BlockAssembler::CreateNewBlock(const CScript &scriptPubKeyIn) {
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());
    if (!pblocktemplate.get()) {
        return nullptr;
    }

    // Pointer for convenience.
    pblock = &pblocktemplate->block;

    // Add dummy coinbase tx as first transaction.
    pblock->vtx.emplace_back();
    // updated at end
    pblocktemplate->vTxFees.push_back(CAmount(-1));
    // updated at end
    pblocktemplate->vTxSigOpsCount.push_back(-1);

    LOCK2(cs_main, mempool.cs);
    CBlockIndex *pindexPrev = chainActive.Tip();
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion =
        ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand()) {
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);
    }

    pblock->nTime = GetAdjustedTime();
    pblock->nBlockHeight = chainActive.Height() + 1;
    nMaxGeneratedBlockSize = ComputeMaxGeneratedBlockSize(*config, pindexPrev);

    nLockTimeCutoff = pblock->GetBlockTime();

    addPriorityTxs();
    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.nFlags = TX_FLAGS_COINBASE;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].SetNull();
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vout[0].nLockTime = COINBASE_MATURITY;
    coinbaseTx.vin[0].prevout.n = nHeight;
    coinbaseTx.vin[0].prevout.nValue = coinbaseTx.vout[0].nValue;
    coinbaseTx.vin[0].scriptSig = CScript() << OP_0;
    pblock->vtx[0] = MakeTransactionRef(coinbaseTx);
    //inherit parameters from pindexPrev
    pblock->nChainInterest = pindexPrev->nChainInterest + nInterest;
    pblocktemplate->vTxFees[0] = -1 * nFees;


    uint64_t nSerializeSize =
        GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);

    LogPrintf("CreateNewBlock(): total size: %u txs: %u fees: %ld sigops %d\n",
              nSerializeSize, nBlockTx, nFees, nBlockSigOps);

    // Fill in header.
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    UpdateTime(pblock, *config, pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, *config);
    pblock->nNonce = 0;


    pblocktemplate->vTxSigOpsCount[0] =
        GetSigOpCountWithoutP2SH(*pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(*config, state, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s",
                                           __func__,
                                           FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrintf("CreateNewBlock: %s\n", pblock->ToString());
    LogPrint("bench", "CreateNewBlock() packages: %.2fms (%d packages, %d "
                      "updated descendants), validity: %.2fms (total %.2fms)\n",
             0.001 * (nTime1 - nTimeStart), nPackagesSelected,
             nDescendantsUpdated, 0.001 * (nTime2 - nTime1),
             0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter) {
    for (CTxMemPool::txiter parent : mempool.GetMemPoolParents(iter)) {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries &testSet) {
    for (CTxMemPool::setEntries::iterator iit = testSet.begin();
         iit != testSet.end();) {
        // Only test txs not already in the block.
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOps) {
    auto blockSizeWithPackage = nBlockSize + packageSize;
    if (blockSizeWithPackage >= nMaxGeneratedBlockSize) {
        return false;
    }
    if (nBlockSigOps + packageSigOps >=
        GetMaxBlockSigOpsCount(blockSizeWithPackage)) {
        return false;
    }
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(
    const CTxMemPool::setEntries &package) {
    uint64_t nPotentialBlockSize = nBlockSize;
    for (const CTxMemPool::txiter it : package) {
        CValidationState state;
        if (!ContextualCheckTransaction(*config, it->GetTx(), state, nHeight,
                                        nLockTimeCutoff)) {
            return false;
        }

        uint64_t nTxSize =
            ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
        if (nPotentialBlockSize + nTxSize >= nMaxGeneratedBlockSize) {
            return false;
        }

        nPotentialBlockSize += nTxSize;
    }

    return true;
}

bool BlockAssembler::TestForBlock(CTxMemPool::txiter it) {
    auto blockSizeWithTx =
        nBlockSize +
        ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    if (blockSizeWithTx >= nMaxGeneratedBlockSize) {
        if (nBlockSize > nMaxGeneratedBlockSize - 100 || lastFewTxs > 50) {
            blockFinished = true;
            return false;
        }
        if (nBlockSize > nMaxGeneratedBlockSize - 1000) {
            lastFewTxs++;
        }
        return false;
    }

    auto maxBlockSigOps = GetMaxBlockSigOpsCount(blockSizeWithTx);
    if (nBlockSigOps + it->GetSigOpCount() >= maxBlockSigOps) {
        // If the block has room for no more sig ops then flag that the block is
        // finished.
        // TODO: We should consider adding another transaction that isn't very
        // dense in sigops instead of bailing out so easily.
        if (nBlockSigOps > maxBlockSigOps - 2) {
            blockFinished = true;
            return false;
        }
        // Otherwise attempt to find another tx with fewer sigops to put in the
        // block.
        return false;
    }

    // Must check that lock times are still valid. This can be removed once MTP
    // is always enforced as long as reorgs keep the mempool consistent.
    CValidationState state;
    if (!ContextualCheckTransaction(*config, it->GetTx(), state, nHeight,
                                    nLockTimeCutoff)) {
        return false;
    }

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter) {
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCount.push_back(iter->GetSigOpCount());
    nBlockSize += iter->GetTxSize();
    ++nBlockTx;
    nBlockSigOps += iter->GetSigOpCount();
    nFees += iter->GetFee();
    nInterest += iter->GetInterest();
    inBlock.insert(iter);

    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(iter->GetTx().GetId(), dPriority, dummy);
        LogPrintf(
            "priority %.1f fee %s txid %s\n", dPriority,
            CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
            iter->GetTx().GetId().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(
    const CTxMemPool::setEntries &alreadyAdded,
    indexed_modified_transaction_set &mapModifiedTx) {
    int nDescendantsUpdated = 0;
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set.
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc)) {
                continue;
            }
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCountWithAncestors -= it->GetSigOpCount();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present in
// mapModifiedTx (which implies that the mapTx ancestor state is stale due to
// ancestor inclusion in the block). Also skip transactions that we've already
// failed to add. This can happen if we consider a transaction in mapModifiedTx
// and it fails: we can then potentially consider it again while walking mapTx.
// It's currently guaranteed to fail again, but as a belt-and-suspenders check
// we put it in failedTx and avoid re-evaluation, since the re-evaluation would
// be using cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(
    CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx,
    CTxMemPool::setEntries &failedTx) {
    assert(it != mempool.mapTx.end());
    if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it)) {
        return true;
    }
    return false;
}

void BlockAssembler::SortForBlock(
    const CTxMemPool::setEntries &package, CTxMemPool::txiter entry,
    std::vector<CTxMemPool::txiter> &sortedEntries) {
    // Sort package by ancestor count. If a transaction A depends on transaction
    // B, then A's ancestor count must be greater than B's. So this is
    // sufficient to validly order the transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(),
              CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based on feerate of a
// transaction including all unconfirmed ancestors. Since we don't remove
// transactions from the mempool as we select them for block inclusion, we need
// an alternate method of updating the feerate of a transaction with its
// not-yet-selected ancestors as we go. This is accomplished by walking the
// in-mempool descendants of selected transactions and storing a temporary
// modified state in mapModifiedTxs. Each time through the loop, we compare the
// best transaction in mapModifiedTxs with the next transaction in the mempool
// to decide what transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected,
                                   int &nDescendantsUpdated) {
    // mapModifiedTx will store sorted packages after they are modified because
    // some of their txs are already in the block.
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work.
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors.
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator
        mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() ||
           !mapModifiedTx.empty()) {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
            SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx,
                           failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry.
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score than the one
                // from mapTx. Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOps = iter->GetSigOpCountWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOps = modit->nSigOpCountWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOps)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx, we
                // must erase failed entries so that we can consider the next
                // best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES &&
                nBlockSize > nMaxGeneratedBlockSize - 1000) {
                // Give up if we're close to full and haven't succeeded in a
                // while.
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit,
                                          nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final.
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void BlockAssembler::addPriorityTxs() {
    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay.
    if (config->GetBlockPriorityPercentage() == 0) {
        return;
    }

    uint64_t nBlockPrioritySize =
        nMaxGeneratedBlockSize * config->GetBlockPriorityPercentage() / 100;

    // This vector will be sorted into a priority queue:
    std::vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>
        waitPriMap;
    typedef std::map<CTxMemPool::txiter, double,
                     CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi =
             mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi) {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(mi->GetTx().GetId(), dPriority, dummy);
        vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;

    // Add a tx from priority queue to fill the part of block reserved to
    // priority transactions.
    while (!vecPriority.empty() && !blockFinished) {
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        // If tx already in block, skip.
        if (inBlock.count(iter)) {
            // Shouldn't happen for priority txs.
            assert(false);
            continue;
        }

        // If tx is dependent on other mempool txs which haven't yet been
        // included then put it in the waitSet.
        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        // If this tx fits in the block add it, otherwise keep looping.
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // If now that this txs is added we've surpassed our desired
            // priority size or have dropped below the AllowFreeThreshold, then
            // we're done adding priority txs.
            if (nBlockSize >= nBlockPrioritySize ||
                !AllowFree(actualPriority)) {
                break;
            }

            // This tx was successfully added, so add transactions that depend
            // on this one to the priority queue to try again.
            for (CTxMemPool::txiter child : mempool.GetMemPoolChildren(iter)) {
                waitPriIter wpiter = waitPriMap.find(child);
                if (wpiter != waitPriMap.end()) {
                    vecPriority.push_back(
                        TxCoinAgePriority(wpiter->second, child));
                    std::push_heap(vecPriority.begin(), vecPriority.end(),
                                   pricomparer);
                    waitPriMap.erase(wpiter);
                }
            }
        }
    }
}

void IncrementExtraNonce(const Config &config, CBlock *pblock,
                         const CBlockIndex *pindexPrev,
                         unsigned int &nExtraNonce) {
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    // Height first in coinbase required for block.version=2
//    unsigned int nHeight = pindexPrev->nHeight + 1;
//    CMutableTransaction txCoinbase(*pblock->vtx[0]);
//    txCoinbase.vin[0].scriptSig =
//        (CScript() << nHeight << CScriptNum(nExtraNonce)
//                   << getExcessiveBlockSizeSig(config)) +
//        COINBASE_FLAGS;
//    assert(txCoinbase.vin[0].scriptSig.size() <= MAX_COINBASE_SCRIPTSIG_SIZE);

//    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

CCriticalSection cs_miner;

MineWorker::MineWorker(const Config &_config):config(&_config)
{
    //GetMainSignals().ScriptForMining(coinbaseScript);
    scriptPubKey = CScript();

    fGenerate           = false;
    fPoolMiningFinished = true;

    nThreads      = -1;
    dHashesPerSec = 0.0;

    currentTemplate = NULL;
    minerThreads    = NULL;
    workDispatcher  = NULL;
    dagGenerator    = NULL;

    mapEpochLight.clear();
    mapEpochLight.clear();
    listWork.clear();
}

MineWorker::~MineWorker()
{
    if (currentTemplate){
        delete currentTemplate;
        currentTemplate = NULL;
    }

    DestroyEthashFull();
    DestroyEthashLight();
}

/*
void MineWorker::SetMinerAddress(CBitcoinAddress address)
{
    scriptPubKey = GetScriptForDestination(address.Get());
}

CBitcoinAddress MineWorker::GetMinerAddress()
{
    CTxDestination dest;
    ExtractDestination(scriptPubKey, dest);

    return CBitcoinAddress(dest);
}
*/

int MineWorker::GetThreads()
{
    return nThreads;
}

void MineWorker::SetThreads(int threadCount)
{
    nThreads = threadCount;
}

void MineWorker::RunWorker()
{
    CleanWork();
    PlatopiaMinerPoolStart();

    if (dagGenerator != NULL) {
        dagGenerator->interrupt();

        delete dagGenerator; dagGenerator = NULL;
    }

    if (workDispatcher != NULL) {
        workDispatcher->interrupt();

        delete workDispatcher; workDispatcher = NULL;
    }

    if (fGenerate) {
        dagGenerator = new boost::thread(boost::bind(&(MineWorker::dagGeneratorWork), this));
    }

    if (fGenerate) {
        workDispatcher = new boost::thread(boost::bind(&(MineWorker::dispatchWork), this));
    }

}

void MineWorker::StopWorker()
{
    PlatoPiaMinerPoolStop();
    CleanWork();
}

void MineWorker::PlatopiaMinerPoolStart(uint64_t nMaxTries)
{
    LogPrintf("PlatoPiaMinerPoolStart \n");

    fGenerate = true;
    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        fPoolMiningFinished=true;

        delete minerThreads; minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate) {
        fGenerate = false;
        LogPrintf("nThreads: %d\tfGenerate: %s\n", nThreads, fGenerate ? "true":"false");
        return;
    }
    dHashesPerSec = 0.0;

    LogPrintf("PlatopiaMinerPoolStart threads %d\n", nThreads);

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++) {
        minerThreads->create_thread(boost::bind(&(MineWorker::doWork), this, nMaxTries));
    }

    return;
}

void MineWorker::dagGeneratorWork(MineWorker *worker)
{
    RenameThread("dagGeneratorWork");

    while (worker->fGenerate) {
        if (worker->GetEthashFull(chainActive.Height()) == NULL) worker->AppendEthashFull(chainActive.Height());

        if (chainActive.Height() % ETHASH_EPOCH_LENGTH > 20000) {
            worker->AppendEthashFull(chainActive.Height() + ETHASH_EPOCH_LENGTH);
        }

        MilliSleep(10 * 1000);
    }
}

void MineWorker::PlatoPiaMinerPoolStop()
{
    LogPrintf("PlatopiaMinerPoolStop\n");
    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        fPoolMiningFinished = true;
        delete minerThreads; minerThreads = NULL;
        dHashesPerSec = 0;
    }

    fGenerate = false;
}

bool MineWorker::AppendEthashFull(uint32_t nBlockHeight)
{
    if (mapEpochFull.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) != mapEpochFull.end()) {
        return true;
    }

    {
        LOCK(cs_ethash);
        ethash_light_t plight = GetEthashLight(nBlockHeight);
        ethash_full_t  pfull  = ethash_full_new(plight, dagCallbackShim);

        int64_t nEpochs = nBlockHeight / ETHASH_EPOCH_LENGTH;
        mapEpochFull.insert(make_pair(nEpochs, pfull));
    }

    return true;
}

ethash_full_t MineWorker::GetEthashFull(uint32_t nBlockHeight) const
{
    if (mapEpochFull.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) != mapEpochFull.end()) {
        return mapEpochFull.at(nBlockHeight / ETHASH_EPOCH_LENGTH);
    }

    return NULL;
}

bool MineWorker::EraseEthashFull(uint32_t nBlockHeight)
{
    if (mapEpochFull.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) == mapEpochFull.end()) return true;

    ethash_full_t pfull = mapEpochFull.at(nBlockHeight / ETHASH_EPOCH_LENGTH);
    ethash_full_delete(pfull);
    mapEpochFull.erase(nBlockHeight / ETHASH_EPOCH_LENGTH);

    return true;
}

void MineWorker::DestroyEthashFull()
{
    for(std::map<int64_t, ethash_full_t>::iterator it = mapEpochFull.begin(); it != mapEpochFull.end(); it++) {
        ethash_full_t pfull = it->second;
        ethash_full_delete(pfull);
    }

    mapEpochFull.clear();
}

bool MineWorker::AppendEthashLight(uint32_t nBlockHeight)
{
    if (mapEpochLight.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) != mapEpochLight.end()) {
        return true;
    }

    {
        LOCK(cs_ethash);
        ethash_light_t plight  = ethash_light_new(nBlockHeight);
        int64_t        nEpochs = nBlockHeight / ETHASH_EPOCH_LENGTH;
        mapEpochLight.insert(make_pair(nEpochs, plight));
    }

    return true;
}

ethash_light_t MineWorker::GetEthashLight(uint32_t nBlockHeight)
{
    if (mapEpochLight.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) != mapEpochLight.end()) {
        return mapEpochLight.at(nBlockHeight / ETHASH_EPOCH_LENGTH);
    }

    AppendEthashLight(nBlockHeight);
    return mapEpochLight.at(nBlockHeight / ETHASH_EPOCH_LENGTH);
}

bool MineWorker::EraseEthashLight(uint32_t nBlockHeight)
{
    if (mapEpochLight.find(nBlockHeight/ ETHASH_EPOCH_LENGTH) == mapEpochLight.end()) return true;

    ethash_light_t plight = mapEpochLight.at(nBlockHeight / ETHASH_EPOCH_LENGTH);
    ethash_light_delete(plight);
    mapEpochLight.erase(nBlockHeight / ETHASH_EPOCH_LENGTH);

    return true;
}

void MineWorker::DestroyEthashLight()
{
    for(std::map<int64_t, ethash_light_t>::iterator it = mapEpochLight.begin(); it != mapEpochLight.end(); it++) {
        ethash_light_t plight = it->second;
        ethash_light_delete(plight);
    }

    mapEpochLight.clear();
}

void MineWorker::dispatchSingleWork(MineWorker *worker, std::shared_ptr<CReserveScript> coinbaseScript,
                                       int nBlocks, bool keepScript, vector<uint256> *vHashes)
{
    RenameThread("dispatchSingleWork");

    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    LogPrintf("dispatchSingleWork: blocks: %d\n", nBlocks);
    while (worker->fGenerate && nBlocks) {
        worker->CleanWork();
        Work work = worker->GenNewWork(coinbaseScript->reserveScript);
        //Work work = worker->GenNewWork(worker->scriptPubKey);
        auto pwork = worker->AddWork(work.block, work.boundary);

        while (worker->fGenerate && nBlocks) {
            if (pwork->done) {
                SetThreadPriority(THREAD_PRIORITY_NORMAL);

                if (worker->ProcessBlockFound(worker->config, &(pwork->block), *pwalletMain)) {
                    vHashes->push_back(pwork->block.GetHash());
                    worker->RemoveWork(pwork->blockEthash);
                    nBlocks--;

                    if (keepScript) {
                        coinbaseScript->KeepScript();
                    }
                }
                SetThreadPriority(THREAD_PRIORITY_LOWEST);
                break;
            }
            static int64_t nLogTime;
            if (GetTime() - nLogTime > 1 * 30) {
                nLogTime = GetTime();
                LogPrintf("hashmeter %6.3f khash/s\n", worker->GetHashRate()/1000.0);
            }

            MilliSleep(1000);
        }
    }
}

void MineWorker::dispatchWork(MineWorker *worker)
{
    RenameThread("dispatchWork");

    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    while (worker->fGenerate) {
        Work work = worker->GenNewWork(worker->scriptPubKey);
        auto pwork = worker->AddWork(work.block, work.boundary);

        while (worker->fGenerate) {
            if(chainActive.Height() >= pwork->block.nBlockHeight)
            {
                pwork->deprecated = true;
                while(pwork->miningThreads != 0)
                {
                    MilliSleep(1000);
                }

                worker->RemoveWork(pwork->blockEthash);
                break;
            }
            else
            {
                if (pwork->done)
                {
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    worker->ProcessBlockFound(worker->config, &(pwork->block), *pwalletMain);
                    while(pwork->miningThreads != 0)
                    {
                        MilliSleep(1000);
                    }

                    worker->RemoveWork(pwork->blockEthash);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                    break;
                }

                static int64_t nLogTime;
                if (GetTime() - nLogTime > 1 * 30) {
                    nLogTime = GetTime();
                    LogPrintf("hashmeter %6.3f khash/s\n", worker->GetHashRate()/1000.0);
                }
            }

            MilliSleep(1000);
        }
    }

}

void MineWorker::doWork(MineWorker *worker, uint64_t nMaxTries)
{
    RenameThread("doWork");

    while (worker->fGenerate) {
        auto work = worker->GetWork();
        if (work == NULL || work->done || work->deprecated) {
            MilliSleep(1000);
            continue;
        }

        {
            LOCK(cs_miner);
            work->miningThreads++;
        }

        uint32_t nBlockHeight = work->block.nBlockHeight;
        ethash_h256_t blockEthash = work->blockEthash;
        ethash_h256_t boundary    = work->boundary;
        LogPrintf("Work on: %s\n", ethash_h256_encode(blockEthash));

        ethash_h256_t mixHash = {0};
        uint64_t nNonce = 0;
        SetThreadPriority(THREAD_PRIORITY_LOWEST);
        if(worker->MinePlatopia(&(work->done), &(work->deprecated), blockEthash, nBlockHeight, boundary, &mixHash, &nNonce, nMaxTries))
        {
            work->block.nNonce  = nNonce;
            work->block.hashMix = mixHash;
            work->done = true;
        }
        {
            LOCK(cs_miner);
            work->miningThreads--;
        }
        SetThreadPriority(THREAD_PRIORITY_LOWEST);
        if (!worker->fGenerate)
        {
            break;
        }
    }

}

inline bool MineWorker::MinePlatopia(bool *fDone, bool *deprecated, ethash_h256_t blockEthash, uint64_t nBlockHeight, ethash_h256_t boundary, ethash_h256_t *mixHashOut, uint64_t *nonceOut, uint64_t nMaxTries)
{

    ethash_full_t pfull = GetEthashFull(nBlockHeight);
    while (pfull == NULL) {
        if (!fGenerate) return false;
        pfull = GetEthashFull(nBlockHeight);
        MilliSleep(1000);
    }
    uint64_t nNonce = GetRand(0xffffffffffffffff);

    uint64_t nTryCount      = 0;
    uint64_t nHashCount     = 0;
    int64_t  nHPSTimerStart = 0;

    while(fGenerate && !*fDone && !*deprecated) {
        ethash_return_value_t ret = ethash_full_compute(pfull, blockEthash, nNonce);
        if (ethash_quick_check_difficulty( &blockEthash, nNonce, &(ret.mix_hash), &boundary )) {
            // Found a solution
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            LogPrintf("PlatopiaMiner:\n");
            LogPrintf("proof-of-work found  \n");
            LogPrintf("   Ethash: %s\n", ethash_h256_encode(blockEthash));
            LogPrintf("   Target: %s\n", ethash_h256_encode(boundary));
            LogPrintf("   Nonce: %llu\n", nNonce);
            LogPrintf("   MixHash: %s\n", ethash_h256_encode(ret.mix_hash));
            *mixHashOut = ret.mix_hash;
            *nonceOut = nNonce;
            // In regression test mode, stop mining after a block is found.
            return true;
        }

        nHashCount += 1;
        nNonce     += 1;

        if(nMaxTries != 0) {
            nTryCount  += 1;

            if(nTryCount > nMaxTries) {
                break;
            }
        }

        if (GetTimeMillis() - nHPSTimerStart > 4000) {
            static CCriticalSection cs;
            {
                LOCK(cs);
                dHashesPerSec = (double)(1000.0 * nHashCount / (GetTimeMillis() - nHPSTimerStart));
            }
            nHPSTimerStart = GetTimeMillis();
            nHashCount = 0;
        }
    }

    return false;
}

double MineWorker::GetHashRate() const
{
    if (nThreads <= 1) return dHashesPerSec;
    return dHashesPerSec * nThreads;
}

void MineWorker::SetHashRate(double dRate)
{
    dHashesPerSec = dRate;

    static int64_t nLogTime;
    if (GetTime() - nLogTime > 1 * 30) {
        nLogTime = GetTime();
        LogPrintf("hashmeter %6.3f khash/s\n", GetHashRate()/1000.0);
    }
}

Work MineWorker::GenNewWork(const CScript &scriptPubKeyIn)
{
    unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler((*config), Params()).CreateNewBlock(scriptPubKeyIn));

    if (!pblocktemplate.get()) throw error("CreateBlock Failed\n");

    CBlock *pblock = &pblocktemplate->block;

    unsigned int nExtraNonce = 0;
    {
        LOCK(cs_main);
        IncrementExtraNonce(*config, pblock, chainActive.Tip(), nExtraNonce);
    }

    arith_uint256 hashTarget  = arith_uint256().SetCompact(pblock->nBits);
    ethash_h256_t boundary    = hashTarget.ToEthashH256();
    ethash_h256_t blockEthash = CBlockHeaderBase(*pblock).GetEthash();

    return {*pblock, blockEthash, boundary, false, 0, false};
}

std::shared_ptr<Work>
MineWorker::AddWork(CBlock &block, ethash_h256_t boundary)
{
    ethash_h256_t blockEthash = CBlockHeaderBase(block).GetEthash();
    for (auto it = listWork.begin(); it != listWork.end(); it++) {
        if (EthashEquals((*it)->blockEthash, blockEthash) && EthashEquals(boundary, (*it)->boundary)) {
            return *it;
        }
    }

    LogPrintf("Add a new work %s\n", ethash_h256_encode(blockEthash));
    std::shared_ptr<Work> newWork(new Work{block, blockEthash, boundary, false, 0, false});
    listWork.push_back(newWork);
    return listWork.back();
}

std::shared_ptr<Work> MineWorker::GetWork() const
{
    for (auto it : listWork) {
        if (it->done) continue;

        return it;
    }

    LogPrintf("GetWork no work\n");
    return NULL;
}

std::shared_ptr<Work> MineWorker::GetWork(ethash_h256_t &blockEthash) const
{
    for (auto it : listWork) {
        if (EthashEquals(it->blockEthash, blockEthash)) return it;
    }

    return NULL;
}

void MineWorker::UpdateWork(ethash_h256_t &blockEthash, uint64_t &nonce, ethash_h256_t &hashMix)
{
    for (auto it : listWork) {
        if (EthashEquals(it->blockEthash, blockEthash)) {
            it->block.nNonce  = nonce;
            it->block.hashMix = hashMix;
        }
    }
}

void MineWorker::RemoveWork(const ethash_h256_t &blockEthash)
{
    LogPrintf("RemoveWork: %s\n", ethash_h256_encode_big(blockEthash));
    listWork.remove_if(([&, blockEthash](std::shared_ptr<Work> work){ return EthashEquals(work->blockEthash, blockEthash ); }));
}

void MineWorker::RemoveWork(uint32_t nBlockHeight)
{
    LogPrintf("RemoveWork: %u\n", nBlockHeight);
    listWork.remove_if(([&, nBlockHeight](std::shared_ptr<Work> work){ return work->block.nBlockHeight == nBlockHeight ; }));
}

void MineWorker::SetWorkDone(ethash_h256_t &blockEthash)
{
    for (auto it : listWork) {
        if (EthashEquals(it->blockEthash, blockEthash)) {
            it->done = true;
        }
    }
}

void MineWorker::ShowWorkList() const
{
    int i = 0;
    for (auto it : listWork) {
        LogPrintf("Work Index:%d, BlockEthash: %s, Height: %ld, Done: %s\n", i++, ethash_h256_encode_big(it->blockEthash), it->block.nBlockHeight, it->done ? "true":"false");
    }
}

void MineWorker::CleanWork()
{
    listWork.clear();
}

bool MineWorker::ProcessBlockFound(const Config *config, const CBlock* pblock, CWallet& wallet)
{

    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0]->GetValueOut()));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("PlatopiaMiner : generated block is stale");
    }

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    bool fNewBlock = false;
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(*config, shared_pblock, true, &fNewBlock))
        return error("Platopia Miner : ProcessNewBlock, block not accepted");

    uint256 blockhash = pblock->GetHash();
    LogPrintf("NotifyBlockMined block hash %s", blockhash.ToString());
    //uiInterface.NotifyBlockMined(blockhash);

    return true;
}

vector<uint256> MineWorker::MineBlocks(std::shared_ptr<CReserveScript> coinbaseScript,
                                       int nGenerate, uint64_t nMaxTries, bool keepScript)
{
    CleanWork();
    PlatopiaMinerPoolStart(nMaxTries);

    if (dagGenerator != NULL) {
        dagGenerator->interrupt();

        delete dagGenerator; dagGenerator = NULL;
    }

    if (fGenerate) {
        dagGenerator = new boost::thread(boost::bind(&(MineWorker::dagGeneratorWork), this));
    }

    vector<uint256> blockHashes;
    if (workDispatcher != NULL) {
        workDispatcher->interrupt();

        delete workDispatcher; workDispatcher = NULL;
    }

    if (fGenerate) {
        workDispatcher = new boost::thread(boost::bind(&(MineWorker::dispatchSingleWork), this, coinbaseScript, nGenerate, keepScript, &blockHashes));
        //workDispatcher = new boost::thread(boost::bind(&(MineWorker::dispatchSingleWork), this, nBlocks, &blockHashes));
    }

    if (workDispatcher) {
        workDispatcher->join();
    }

    PlatoPiaMinerPoolStop();
    return blockHashes;
}

std::shared_ptr<Work> MineWorker::GetLastNewWork(std::shared_ptr<CReserveScript> coinbaseScript, bool keepScript, bool prune)
{
/*
    txnouttype outType = TX_NONSTANDARD;
    if(!IsStandard(scriptPubKey, outType))
    {
        string sAddress = pwalletMain->GetAddress();
        CBitcoinAddress address(sAddress);
        SetMinerAddress(address);
    }
*/

    auto pwork = GetWork();
    if (!pwork) {
        Work work = GenNewWork(scriptPubKey);
        LogPrintf("Gen NewWork NULL\n");
        pwork = AddWork(work.block, work.boundary);
    }

    if(prune)
    {
        while(pwork->block.nBlockHeight <= chainActive.Height())
        {
            RemoveWork(pwork->block.nBlockHeight);
            pwork = GetWork();
            if (!pwork) {
                Work work = GenNewWork(scriptPubKey);
                LogPrintf("Gen NewWork Height\n");
                pwork = AddWork(work.block, work.boundary);
            }
        }
    }

    ShowWorkList();
    return pwork;
}

bool MineWorker::SubmitWork(ethash_h256_t blockEthash, uint64_t nNonce, ethash_h256_t mixHash)
{
    SetWorkDone(blockEthash);
    UpdateWork(blockEthash, nNonce, mixHash);
    auto pwork = GetWork(blockEthash);
    if (pwork == NULL) {
        LogPrintf("no such Work %s\n", ethash_h256_encode(blockEthash));
        return false;
    }

    if (ProcessBlockFound(config, &(pwork->block), *pwalletMain)) {
        return true;
    }

    RemoveWork(pwork->blockEthash);
    return false;
}

