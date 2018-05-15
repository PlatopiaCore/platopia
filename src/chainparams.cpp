// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <cassert>
#include <string>
#include <cmath>

#include "chainparamsseeds.h"
#include "core_io.h"

#include "ethash/ethash.h"
#include "ethash/internal.h"
#include "ethash/sha3.h"

static CBlock CreateGenesisBlock(const char *intro,
                                 const CScript &genesisOutputScript,
                                 uint32_t nTime, uint64_t nNonce,
                                 ethash_h256_t mixHash,
                                 uint32_t nBits, int32_t nVersion,
                                 const CAmount genesisReward, CAmount nChainInterest) {
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.nFlags = TX_FLAGS_COINBASE;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0;
    txNew.vin[0].prevout.nValue = genesisReward;
    txNew.vin[0].prevout.n = 0;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;
    txNew.vout[0].nPrincipal = 0;
    txNew.vout[0].nLockTime = 100;
    txNew.vout[0].strContent = std::string(intro);

//    std::cout << EncodeHexTx(txNew) << std::endl;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.hashMix = mixHash;
    genesis.nVersion = nVersion;
    genesis.nBlockHeight = 0;
    genesis.nChainInterest = nChainInterest;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation transaction
 * cannot be spent since it did not originally exist in the database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000,
 * hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893,
 * vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase
 * 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint64_t nNonce, ethash_h256_t mixHash,
                                 uint32_t nBits, int32_t nVersion, std::string scriptOut,
                                 const CAmount genesisReward, const CAmount nChainInterest) {
    const char *intro = "By resolving the trust problem of data transmission through technical means, blockchain technology"
                                   " has become an invisible huge force that drives the development of science and technology and a strong"
                                   " force that pushes humanity forward in the right direction. Through its continuous efforts to establish"
                                   " a completely decentralized and borderless public trust implementation system that everyone can"
                                   " participate in, Platopia is a meaningful social practice that combines science and technology with"
                                   " humanity and awakens the seeds of kindness in our hearts so as to inspire and serve every future generation.";
    std::vector<unsigned char>scriptGenesis = ParseHex(scriptOut);
    const CScript genesisOutputScript = CScript(scriptGenesis.begin(), scriptGenesis.end());
    return CreateGenesisBlock(intro, genesisOutputScript, nTime, nNonce, mixHash,
                              nBits, nVersion, genesisReward, nChainInterest);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nBlocksPerDay = 960;
        consensus.nDaysPerCentury = 300;
        consensus.nDecayRatio = 0.9;
        consensus.nBlocksPerCentury = consensus.nBlocksPerDay * consensus.nDaysPerCentury;
        consensus.nSubsidyHalvingInterval = consensus.nBlocksPerCentury;

        consensus.nTotalInterest = 240000000000000000;
        consensus.nLockInterestBlocksThreshould[0]=16*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[1]=32*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[2]=64*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[3]=128*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[4]=256*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[5]=512*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[6]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[7]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestRate[0]=(double)0.0142857;//per 100 days
        consensus.nLockInterestRate[1]=(double)0.0285714;
        consensus.nLockInterestRate[2]=(double)0.0428571;
        consensus.nLockInterestRate[3]=(double)0.0571428;
        consensus.nLockInterestRate[4]=(double)0.0714285;
        consensus.nLockInterestRate[5]=(double)0.0857142;
        consensus.nLockInterestRate[6]=(double)0.0999999;

        consensus.nBlockReward = OldChainSubsidyForBlock(1440001);
        consensus.nGenesisReward = OldChainSubsidyTillBlock(1440000) + 39168290492526951 + OldChainLotteryTillCentury(centuryForBlock(1440000));

        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256S("0x000000000000024b89b42a942fe0d9fea3bb4"
                                       "4ab7bd1b19115dd6a759c0808b8");
        // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP65Height = 388381;
        // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.BIP66Height = 363725;
        consensus.powLimit = uint256S(
            "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // two weeks
        consensus.nPowTargetTimespan = 24 * 60 * 60;
        consensus.nPowTargetSpacing = 90;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        // 95% of 2016
        consensus.nRuleChangeActivationThreshold = 1916;
        // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMinerConfirmationWindow = 2016;

        // The best chain should have at least this much work.
//        consensus.nMinimumChainWork =
//            uint256S("0x0000000000000000000000000000000000000000007e5dbf54c7f6b"
//                     "58a6853cd");

        // By default assume that the signatures in ancestors of this block are
        // valid.
//        consensus.defaultAssumeValid =
//            uint256S("0x000000000000000005e14d3f9fdfb70745308706615cfa9edca4f45"
//                     "58332b201");

        /**
         * The message start string is designed to be unlikely to occur in
         * normal data. The characters are rarely used upper ASCII, not valid as
         * UTF-8, and produce a large 32-bit integer with any alignment.
         */
        diskMagic[0] = 0xfc;
        diskMagic[1] = 0xb0;
        diskMagic[2] = 0xed;
        diskMagic[3] = 0xee;
        netMagic[0] = 0xfc;
        netMagic[1] = 0xf0;
        netMagic[2] = 0xed;
        netMagic[3] = 0xee;
        nDefaultPort = 41319;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1512403200, 6029914714024845399,
                                     ethash_h256_decode_big("0x0e0c6282441b4b1156fa86331b20c412803d62867ae4c4359973919576e7252b"),
                                     472776703, 3, std::string("76a914d21f0e6dce303eb06350458d400d8b582c65562988ac"),
                                     consensus.nGenesisReward, 39168290492526951);
        consensus.hashGenesisBlock = genesis.GetHash();
//        std::cout << genesis.ToString() << std::endl;
//        CMutableTransaction tx = genesis.vtx[0];
//        std::cout << tx.ToString() << std::endl;
//        std::cout << genesis.GetHash().ToString() << std::endl;
        assert(consensus.hashGenesisBlock ==
               uint256S("0x56e0b8ce91d07105264979fb4d93ebc641d2eb044c39a011a36881f2c88873b7"));
//        std::cout<<genesis.hashMerkleRoot.GetHex()<<std::endl;
        assert(genesis.hashMerkleRoot ==
               uint256S("0x7ea48162117efa96921aa8f94c78a579f3f1d35c00499a9713813460e08cb4c1"));

        // Note that of those with the service bits flag, most only support a
        // subset of possible options.
        // Platopia Core seeder
        vSeeds.push_back(
            CDNSSeedData("platopia.org", "seed0.platopia.org", true));
        vSeeds.push_back(
            CDNSSeedData("platopia.org", "seed1.platopia.org", true));
        vSeeds.push_back(
            CDNSSeedData("platopia.org", "seed2.platopia.org", true));


        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 0x38);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 5);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        cashaddrPrefix = "bitcoincash";

        vFixedSeeds = std::vector<SeedSpec6>(
            pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            .mapCheckpoints = {
                {},
            }};

        // Data as of block
        // 00000000000000000166d612d5595e2b1cd88d71d695fc580af64d8da8658c23
        // (height 446482).
        chainTxData = ChainTxData{
            // UNIX timestamp of last known number of transactions.
            1483472411,
            // Total number of transactions between genesis and that timestamp
            // (the tx=... number in the SetBestChain debug.log lines)
            184495391,
            // Estimated number of transactions per second after that timestamp.
            3.2};
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nBlocksPerDay = 960;
        consensus.nDaysPerCentury = 300;
        consensus.nDecayRatio = 0.9;
        consensus.nBlocksPerCentury = consensus.nBlocksPerDay * consensus.nDaysPerCentury;
        consensus.nSubsidyHalvingInterval = consensus.nBlocksPerCentury;

        consensus.nTotalInterest = 240000000000000000;
        consensus.nLockInterestBlocksThreshould[0]=16*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[1]=32*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[2]=64*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[3]=128*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[4]=256*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[5]=512*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[6]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[7]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestRate[0]=(double)0.0142857;//per 100 days
        consensus.nLockInterestRate[1]=(double)0.0285714;
        consensus.nLockInterestRate[2]=(double)0.0428571;
        consensus.nLockInterestRate[3]=(double)0.0571428;
        consensus.nLockInterestRate[4]=(double)0.0714285;
        consensus.nLockInterestRate[5]=(double)0.0857142;
        consensus.nLockInterestRate[6]=(double)0.0999999;

        consensus.BIP34Height = 10000;
        consensus.BIP34Hash = uint256();

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008
        consensus.nMinerConfirmationWindow = 600;
        consensus.nBlockReward = OldChainSubsidyForBlock(1440001);
        consensus.nGenesisReward = OldChainSubsidyTillBlock(1440000) + 39168290492526951 + OldChainLotteryTillCentury(centuryForBlock(1440000));

        consensus.powLimit = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // two weeks
        consensus.nPowTargetTimespan = 10 * 60;
        consensus.nPowTargetSpacing = 10;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork =
            uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid =
            uint256S("0x000000000000b41f1f2ddf130df8824e2b61c0af809ff86dd5cadb361d984ca7");

        diskMagic[0] = 0x0b;
        diskMagic[1] = 0x11;
        diskMagic[2] = 0x09;
        diskMagic[3] = 0x07;
        netMagic[0] = 0x0b;
        netMagic[1] = 0x11;
        netMagic[2] = 0x09;
        netMagic[3] = 0x07;
        nDefaultPort = 21319;
        nMaxTipAge = 300;
        nPruneAfterHeight = 1000;

        genesis =
                CreateGenesisBlock(1512403200, 9,
                                   ethash_h256_decode_big("0x31046c8c6e4330cbe95c8023140fe8da6edca0d093cb054655baa3ece1c49bf6"),
                                   0x2007FFFF, 3, std::string("76a914ab9eb67a1bc20e8f138523dffc88586f2f31e94188ac"),
                                   consensus.nGenesisReward, 39168290492526951);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0x7611df4e77e6aa14125a5379f14ef902e23eca1abc4878c8463fb72ef1a5aee3"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x736939dfdf8c64ea08be450de50294ad397c66a582059a39c9a3e2a28daa876d"));

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 0x38);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        vFixedSeeds = std::vector<SeedSpec6>(
            pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = {
            .mapCheckpoints = {
                {},
            }};

        // Data as of block
        // 00000000c2872f8f8a8935c8e3c5862be9038c97d4de2cf37ed496991166928a
        // (height 1063660)
        chainTxData = ChainTxData{1483546230, 12834668, 0.15};
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nBlocksPerDay = 10;
        consensus.nDaysPerCentury = 30;
        consensus.nDecayRatio = 0.9;
        consensus.nBlocksPerCentury = consensus.nBlocksPerDay * consensus.nDaysPerCentury;
        consensus.nSubsidyHalvingInterval = consensus.nBlocksPerCentury;

        consensus.nLockInterestBlocksThreshould[0]=16*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[1]=32*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[2]=64*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[3]=128*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[4]=256*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[5]=512*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[6]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestBlocksThreshould[7]=1024*consensus.nBlocksPerDay;
        consensus.nLockInterestRate[0]=(double)1.42857;//per 100 days
        consensus.nLockInterestRate[1]=(double)2.85714;
        consensus.nLockInterestRate[2]=(double)4.28571;
        consensus.nLockInterestRate[3]=(double)5.71428;
        consensus.nLockInterestRate[4]=(double)7.14285;
        consensus.nLockInterestRate[5]=(double)8.57142;
        consensus.nLockInterestRate[6]=(double)9.99999;

        // BIP34 has not activated on regtest (far in the future so block v1 are
        // not rejected in tests)
        consensus.BIP34Height = 100000000;
        consensus.BIP34Hash = uint256();
        // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP65Height = 1351;
        // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251;
        consensus.powLimit = uint256S(
            "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // two weeks
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 10;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        // 75% for testchains
        consensus.nRuleChangeActivationThreshold = 108;
        // Faster than normal for regtest (144 instead of 2016)
        consensus.nMinerConfirmationWindow = 144;


        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid = uint256S("0x00");


        diskMagic[0] = 0xfa;
        diskMagic[1] = 0xbf;
        diskMagic[2] = 0xb5;
        diskMagic[3] = 0xda;
        netMagic[0] = 0xda;
        netMagic[1] = 0xb5;
        netMagic[2] = 0xbf;
        netMagic[3] = 0xfa;
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;

        consensus.nBlockReward = OldChainSubsidyForBlock(1501);
        consensus.nGenesisReward = OldChainSubsidyTillBlock(1500) + 39168290492526951 + OldChainLotteryTillCentury(centuryForBlock(1500));
        genesis = CreateGenesisBlock(1512403200, 1,
                                     ethash_h256_decode_big("0x836c063fc357fc6a3e09df0f6781a183e6f0aa49259a43f568ee1c6f8c7ce448"),
                                     0x207fffff, 3, std::string("76a914ab9eb67a1bc20e8f138523dffc88586f2f31e94188ac"),
                                     consensus.nGenesisReward, 39168290492526951);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0x98df12433b40e2ac03774aa911de4683099e707ccaff03d7ace0ba57f49f3be8"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0xa3a7521e105bc501b3c9aea0a2064441ea3dab4ff25825f9611d2bcbd64d1151"));

        //!< Regtest mode doesn't have any fixed seeds.
        vFixedSeeds.clear();
        //!< Regtest mode doesn't have any DNS seeds.
        vSeeds.clear();

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            .mapCheckpoints = {
                    {0, uint256S("0x98df12433b40e2ac03774aa911de4683099e707ccaff03d7ace0ba57f49f3be8")},
            }};

        chainTxData = ChainTxData{0, 0, 0};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 0x38);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
    }

    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime,
                              int64_t nTimeout) {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};

static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN) {
        return mainParams;
    }

    if (chain == CBaseChainParams::TESTNET) {
        return testNetParams;
    }

    if (chain == CBaseChainParams::REGTEST) {
        return regTestParams;
    }

    throw std::runtime_error(
        strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string &network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime,
                                 int64_t nTimeout) {
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}

/** methods for calculate old release PTA **/
CAmount CChainParams::OldChainSubsidyForBlock(uint32_t blockHeight) const
{
    double subsidy = 1560 * COIN * std::pow(consensus.nDecayRatio, centuryForBlock(blockHeight) - 1);//1560 = 4680 / 3
    return static_cast<CAmount>(subsidy);
}

CAmount CChainParams::OldChainSubsidyTillBlock(uint32_t blockHeight) const
{
    CAmount totalSubsidy = 499200000*COIN;//genesis subsidy
    std::vector<uint32_t> centuryBlocks(blockHeight / consensus.nSubsidyHalvingInterval, consensus.nSubsidyHalvingInterval);
    uint32_t blocksInLastCentury = blockHeight % consensus.nSubsidyHalvingInterval;
    if(blocksInLastCentury > 0)
    {
        centuryBlocks.push_back(blocksInLastCentury);
    }
    for(unsigned int i = 0; i < centuryBlocks.size(); ++i)
    {
        totalSubsidy += this->OldChainSubsidyForBlock(consensus.nSubsidyHalvingInterval * i + 1) * centuryBlocks[i];
    }
    return totalSubsidy;
}

CAmount CChainParams::OldChainLotteryTillCentury(int oldChainCentury) const
{
    CAmount lottery = 0;
    double reward = 100000.0 * COIN;
    for (int i = 1; i <= oldChainCentury; ++i)
    {
        lottery += static_cast<CAmount>(reward);
        reward *= consensus.nDecayRatio;
    }
    return lottery * 100;
}
