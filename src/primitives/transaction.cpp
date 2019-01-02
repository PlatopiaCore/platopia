// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

std::string COutPoint::ToString() const {
    return strprintf("COutPoint(%s, %u, %d.%06d)", hash.ToString().substr(0,10), n,nValue/COIN, nValue % COIN);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn) {
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CAmount nValueIn, CScript scriptSigIn) {
    prevout = COutPoint(hashPrevTx, nOut, nValueIn);
    scriptSig = scriptSigIn;
}

std::string CTxIn::ToString() const {
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull()) {
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    } else {
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    }
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn,string strContentIn,uint32_t nLockTimeIn, const CAmount& nPrincipalIn)
{
    nValue = nValueIn;
    nPrincipal = nPrincipalIn;
    scriptPubKey = scriptPubKeyIn;
    strContent=strContentIn;
    nLockTime=nLockTimeIn;
}

CTxOut::CTxOut(const CTxOut &txout)
{
    nValue = txout.nValue;
    nPrincipal = txout.nPrincipal;
    scriptPubKey = txout.scriptPubKey;
    strContent = txout.strContent;
    nLockTime = txout.nLockTime;
}

std::string CTxOut::ToString() const {
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s,"
                     "strContent=%s, nLockTime=%u, nPrincipal=%d.%08d)",
                     nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30),
                     HexStr(strContent.length()>100? strContent.substr(0,100):strContent),
                     nLockTime, nPrincipal / COIN, nPrincipal % COIN);
}

CMutableTransaction::CMutableTransaction()
    : nVersion(CTransaction::CURRENT_VERSION), nFlags(TX_FLAGS_NORMAL) {}
CMutableTransaction::CMutableTransaction(const CTransaction &tx)
    : nVersion(tx.nVersion), nFlags(tx.nFlags), vin(tx.vin), vout(tx.vout){}

static uint256 ComputeCMutableTransactionHash(const CMutableTransaction &tx) {
    return SerializeHash(tx, SER_GETHASH, 0);
}

TxId CMutableTransaction::GetId() const {
    return TxId(ComputeCMutableTransactionHash(*this));
}

TxHash CMutableTransaction::GetHash() const {
    return TxHash(ComputeCMutableTransactionHash(*this));
}

uint256 CTransaction::ComputeHash() const {
    return SerializeHash(*this, SER_GETHASH, 0);
}

/**
 * For backward compatibility, the hash is initialized to 0.
 * TODO: remove the need for this default constructor entirely.
 */
CTransaction::CTransaction()
    : nVersion(CTransaction::CURRENT_VERSION), nFlags(TX_FLAGS_NORMAL), vin(), vout(),
      hash() {}
CTransaction::CTransaction(const CMutableTransaction &tx)
    : nVersion(tx.nVersion), nFlags(tx.nFlags), vin(tx.vin), vout(tx.vout),
      hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx)
    : nVersion(tx.nVersion), nFlags(tx.nFlags), vin(std::move(tx.vin)), vout(std::move(tx.vout)),
      hash(ComputeHash()) {}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<int*>(&nFlags) = tx.nFlags;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    //*const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<uint256*>(&hash) = tx.hash;
    return *this;
}

CAmount CTransaction::GetValueOutWithoutInterest() const
{
    CAmount nValueOut = 0;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        nValueOut += it->nPrincipal > 0  ? it->nPrincipal : it->nValue;
        if (!MoneyRange(it->nValue) || !MoneyRange(it->nPrincipal) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
    }
    return nValueOut;
}

CAmount CTransaction::GetInterest() const
{
    CAmount nInterest = 0;
    if (IsCoinBase())
        return nInterest;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        if (it->nPrincipal == 0)
            continue;
        if (it->nValue > it->nPrincipal)
            nInterest += (it->nValue - it->nPrincipal);
        if (!MoneyRange(it->nValue) || !MoneyRange(it->nPrincipal) || !MoneyRange(nInterest))
            throw std::runtime_error("CTransaction::GetValueOut() : value out of rannInterestge");
    }
    return nInterest;
}

CAmount CTransaction::GetValueOut() const {
    CAmount nValueOut(0);
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end();
         ++it) {
        nValueOut += it->nValue;
        if (!MoneyRange(it->nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error(std::string(__func__) +
                                     ": value out of range");
    }
    return nValueOut;
}

double CTransaction::ComputePriority(double dPriorityInputs,
                                     unsigned int nTxSize) const {
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const {
    // In order to avoid disincentivizing cleaning up the UTXO set we don't
    // count the constant overhead for each txin and up to 110 bytes of
    // scriptSig (which is enough to cover a compressed pubkey p2sh redemption)
    // for priority. Providing any more cleanup incentive than making additional
    // inputs free would risk encouraging people to create junk outputs to
    // redeem later.
    if (nTxSize == 0) nTxSize = GetTransactionSize(*this);
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end();
         ++it) {
        unsigned int offset =
            41U + std::min(110U, (unsigned int)it->scriptSig.size());
        if (nTxSize > offset) nTxSize -= offset;
    }
    return nTxSize;
}

unsigned int CTransaction::GetTotalSize() const {
    return ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const {
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, flags=%i,vin.size=%u, vout.size=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        nFlags,
        vin.size(),
        vout.size());
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}

void CTransaction::ClearContent(CTransaction& newTx) const {
    CMutableTransaction mtx;
    mtx.nVersion=nVersion;
    mtx.nFlags=nFlags;
    mtx.vin=vin;
    mtx.vout=vout;
    for(unsigned int i = 0; i < mtx.vout.size(); i++)
        mtx.vout[i].strContent="";
    newTx = CTransaction(mtx);
}

int64_t GetTransactionSize(const CTransaction &tx) {
    return ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
}

uint256 GetPrevoutHash(const CTransaction& txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (unsigned int n = 0; n < txTo.vin.size(); n++) {
        ss << txTo.vin[n].prevout;
    }
    return ss.GetHash();
}

uint256 GetOutputsHash(const CTransaction& txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (unsigned int n = 0; n < txTo.vout.size(); n++) {
        ss << txTo.vout[n];
    }
    return ss.GetHash();
}

PrecomputedTransactionData::PrecomputedTransactionData(const CTransaction& txTo)
{
    hashPrevouts = GetPrevoutHash(txTo);
    hashOutputs = GetOutputsHash(txTo);
}
