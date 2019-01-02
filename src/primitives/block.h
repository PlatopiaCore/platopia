// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include "ethash/ethash.h"

/** What block version to use for new blocks (pre versionbits) */
static const int32_t VERSIONBITS_LAST_OLD_BLOCK_VERSION = 4;

/**
 * Nodes collect new transactions into a block, hash them into a hash tree, and
 * scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements. When they solve the proof-of-work, they broadcast the block to
 * everyone and the block is added to the block chain. The first transaction in
 * the block is a special one that creates a new coin owned by the creator of
 * the block.
 */
class CBlockHeader {
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nBlockHeight;
    uint32_t nTime;
    uint64_t nChainInterest;
    uint32_t nBits;
    ethash_h256_t hashMix;
    uint64_t nNonce;

    CBlockHeader() { SetNull(); }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nBlockHeight);
        READWRITE(nTime);
        READWRITE(nChainInterest);
        READWRITE(nBits);
        READWRITE(FLATDATA(hashMix));
        READWRITE(nNonce);
    }

    void SetNull() {
    	nVersion = VERSIONBITS_LAST_OLD_BLOCK_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nBlockHeight = 0;
        nTime = 0;
        nChainInterest=0;
        nBits = 0;
        hashMix = {0};
        nNonce = 0;
    }

    bool IsNull() const { return (nBits == 0); }

    uint256 GetHash() const;

    ethash_h256 GetEthash() const;

    int64_t GetBlockTime() const { return (int64_t)nTime; }
};

class CBlockHeaderBase {
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nBlockHeight;
    uint32_t nTime;
    uint64_t nChainInterest;
    uint32_t nBits;

    CBlockHeaderBase() { SetNull(); }

    CBlockHeaderBase(const CBlockHeader &header) {
        nVersion = header.nVersion;
        hashPrevBlock = header.hashPrevBlock;
        hashMerkleRoot = header.hashMerkleRoot;
        nBlockHeight = header.nBlockHeight;
        nTime = header.nTime;
        nChainInterest = header.nChainInterest;
        nBits = header.nBits;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nBlockHeight);
        READWRITE(nTime);
        READWRITE(nChainInterest);
        READWRITE(nBits);
    }

    void SetNull() {
    	nVersion = VERSIONBITS_LAST_OLD_BLOCK_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nBlockHeight = 0;
        nTime = 0;
        nChainInterest=0;
        nBits = 0;
    }

    bool IsNull() const { return (nBits == 0); }

    uint256 GetHash() const;

    ethash_h256 GetEthash() const;

    int64_t GetBlockTime() const { return (int64_t)nTime; }
};


class CBlock : public CBlockHeader {
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;

    CBlock() { SetNull(); }

    CBlock(const CBlockHeader &header) {
        SetNull();
        *((CBlockHeader *)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(*(CBlockHeader *)this);
        READWRITE(vtx);
    }

    void SetNull() {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nBlockHeight   = nBlockHeight;
        block.nTime          = nTime;
        block.nChainInterest = nChainInterest;
        block.nBits          = nBits;
        block.hashMix        = hashMix;
        block.nNonce         = nNonce;
        return block;
    }

    std::string ToString() const;
};

/**
 * Describes a place in the block chain to another node such that if the other
 * node doesn't have the same branch, it can find a recent common trunk.  The
 * further back it is, the further before the fork it may be.
 */
struct CBlockLocator {
    std::vector<uint256> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uint256> &vHaveIn) { vHave = vHaveIn; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull() { vHave.clear(); }

    bool IsNull() const { return vHave.empty(); }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
