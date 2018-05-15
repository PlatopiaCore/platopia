// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"

#include <vector>

struct CDNSSeedData {
    std::string name, host;
    bool supportsServiceBitsFiltering;
    CDNSSeedData(const std::string &strName, const std::string &strHost,
                 bool supportsServiceBitsFilteringIn = false)
        : name(strName), host(strHost),
          supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
};

struct ChainTxData {
    int64_t nTime;
    int64_t nTxCount;
    double dTxRate;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams {
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_BASE58_TYPES
    };

    const Consensus::Params &GetConsensus() const { return consensus; }
    const CMessageHeader::MessageMagic &DiskMagic() const { return diskMagic; }
    const CMessageHeader::MessageMagic &NetMagic() const { return netMagic; }
    int GetDefaultPort() const { return nDefaultPort; }
    const uint256& ProofOfWorkLimit() const { return bnProofOfWorkLimit; }
    /** Used to check majorities for block version upgrade */
    int EnforceBlockUpgradeMajority() const { return nEnforceBlockUpgradeMajority; }
    int RejectBlockOutdatedMajority() const { return nRejectBlockOutdatedMajority; }
    int ToCheckBlockUpgradeMajority() const { return nToCheckBlockUpgradeMajority; }

    const CBlock &GenesisBlock() const { return genesis; }

    /** Used if GeneratePlatopias is called with a negative number of threads */
    int DefaultMinerThreads() const { return nMinerThreads; }
    bool RequireRPCPassword() const { return fRequireRPCPassword; }
    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return fMiningRequiresPeers; }

    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Allow mining of a min-difficulty block */
    bool AllowMinDifficultyBlocks() const { return fAllowMinDifficultyBlocks; }
    /** Skip proof-of-work check: allow mining of any difficulty block */
    bool SkipProofOfWorkCheck() const { return fSkipProofOfWorkCheck; }
    /** Policy: Filter transactions that do not match well-defined patterns */
    bool RequireStandard() const { return fRequireStandard; }
    int BlocksPerDay() const { return consensus.nBlocksPerDay; }
    int BlocksInterestInterval() const { return consensus.nBlocksPerDay * 100;}
    int BlocksPerCentury() const { return consensus.nBlocksPerCentury; }
    int64_t MaxTipAge() const { return nMaxTipAge; }
    uint64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    /**
     * Make miner stop after a block is found. In RPC, don't return until
     * nGenProcLimit blocks are generated.
     */
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData> &DNSSeeds() const { return vSeeds; }
    const std::vector<uint8_t> &Base58Prefix(Base58Type type) const {
        return base58Prefixes[type];
    }
    const std::string &CashAddrPrefix() const { return cashaddrPrefix; }
    const std::vector<SeedSpec6> &FixedSeeds() const { return vFixedSeeds; }
    const CCheckpointData &Checkpoints() const { return checkpointData; }
    const ChainTxData &TxData() const { return chainTxData; }

    uint64_t TotalInterest() const { return consensus.nTotalInterest; }
    int LockInterestBlocksThreshould ( int nLevel ) const {
        if ( nLevel<0||nLevel>7 ) {
            return 0;
        }
        return consensus.nLockInterestBlocksThreshould[nLevel];
    }
    int AdjustToLockInterestThreshold(int lockBlocks) const{
        for(int i = 7; i >= 0; --i)
        {
            if(lockBlocks >= consensus.nLockInterestBlocksThreshould[i])
            {
                return consensus.nLockInterestBlocksThreshould[i];
            }
        }
        return 0;
    }
    double LockInterestRate ( int nLevel ) const {
        if ( nLevel<0||nLevel>6 ) {
            return 0;
        }
        return consensus.nLockInterestRate[nLevel];
    }


    double DecayRatio() const {
        return consensus.nDecayRatio;
    }

    int centuryForBlock(uint32_t blockHeight) const {
        return (blockHeight - 1) / consensus.nBlocksPerCentury + 1;
    }

    int FirstBlockHeightInCentury(int century) const {
        return (century - 1) * consensus.nBlocksPerCentury + 1;
    }

    int LastBlockHeightInCentury(int century) const {
        return century * consensus.nBlocksPerCentury;
    }


protected:
    CChainParams() {}

    CAmount OldChainSubsidyForBlock(uint32_t blockHeight) const;
    //total subsidy from genesis block to height for old chain
    CAmount OldChainSubsidyTillBlock(uint32_t blockHeight) const;

    //lottery for each winner for century in old chain
    CAmount OldChainLotteryEachWinner(int oldChainCentury) const;
    CAmount OldChainLotteryTillCentury(int oldChainCentury) const;

    uint32_t oldChainHeight;//genesis block height continuing from old chain

    Consensus::Params consensus;
    CMessageHeader::MessageMagic diskMagic;
    CMessageHeader::MessageMagic netMagic;
    int nDefaultPort;
    uint64_t nPruneAfterHeight;
    uint256 bnProofOfWorkLimit;

    int nEnforceBlockUpgradeMajority;
    int nRejectBlockOutdatedMajority;
    int nToCheckBlockUpgradeMajority;

    int nMinerThreads;
    long nMaxTipAge;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<uint8_t> base58Prefixes[MAX_BASE58_TYPES];
    std::string cashaddrPrefix;

    std::string strNetworkID;
    CBlock genesis;
    std::vector<SeedSpec6> vFixedSeeds;
    bool fRequireRPCPassword;
    bool fMiningRequiresPeers;
    bool fAllowMinDifficultyBlocks;
    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool fMineBlocksOnDemand;
    bool fSkipProofOfWorkCheck;
    bool fTestnetToBeDeprecatedFieldRPC;
    CCheckpointData checkpointData;
    ChainTxData chainTxData;
};

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams &Params();

/**
 * @returns CChainParams for the given BIP70 chain name.
 */
CChainParams &Params(const std::string &chain);

/**
 * Sets the params returned by Params() to those for the given BIP70 chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string &chain);

/**
 * Allows modifying the BIP9 regtest parameters.
 */
void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime,
                                 int64_t nTimeout);

#endif // BITCOIN_CHAINPARAMS_H
