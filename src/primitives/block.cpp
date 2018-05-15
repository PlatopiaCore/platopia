// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "crypto/common.h"
#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

uint256 CBlockHeader::GetHash() const {
    return SerializeHash(*this);
}

ethash_h256 CBlockHeader::GetEthash() const
{
    return SerializeEthash(*this);
}

uint256 CBlockHeaderBase::GetHash() const
{
    return SerializeHash(*this);
}

ethash_h256 CBlockHeaderBase::GetEthash() const
{
    return SerializeEthash(*this);
}

std::string CBlock::ToString() const {
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, nBlockHeight=%d, "
                   "hashMerkleRoot=%s, nTime=%u, nChainInterest=%llu, nBits=%08x, nNonce=%llu, "
                   "vtx=%u)\n",
                   GetHash().ToString(), nVersion, hashPrevBlock.ToString(), nBlockHeight,
                   hashMerkleRoot.ToString(), nTime, nChainInterest, nBits,
                   nNonce, vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        s << "  " << vtx[i]->ToString() << "\n";
    }
    return s.str();
}
