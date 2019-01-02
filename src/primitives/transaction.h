// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"
#include <string>
using std::string;

static const int SERIALIZE_TRANSACTION = 0x00;

/**
 * A TxId is the identifier of a transaction. Currently identical to TxHash but
 * differentiated for type safety.
 */
struct TxId : public uint256 {
    explicit TxId(const uint256 &b) : uint256(b) {}
};

/**
 * A TxHash is the double sha256 hash of the full transaction data.
 */
struct TxHash : public uint256 {
    explicit TxHash(const uint256 &b) : uint256(b) {}
};

/**
 * An outpoint - a combination of a transaction hash and an index n into its
 * vout.
 */
class COutPoint {
public:
    uint256 hash;
    uint32_t n;
    CAmount nValue;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, uint32_t nIn, CAmount nValueIn=0) {
        hash = hashIn;
        n = nIn;
        nValue = nValueIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(VARINT(n));
        READWRITE(VARINT(nValue));
    }

    void SetNull() {
        hash.SetNull();
        n = (uint32_t)-1;
        nValue=0;
    }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t)-1); }

    friend bool operator<(const COutPoint &a, const COutPoint &b) {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint &a, const COutPoint &b) {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint &a, const COutPoint &b) {
        return !(a == b);
    }

    std::string ToString() const;
};

/**
 * An input of a transaction. It contains the location of the previous
 * transaction's output that it claims and a signature that matches the output's
 * public key.
 */
class CTxIn {
public:
    COutPoint prevout;
    CScript scriptSig;


    CTxIn() { }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn = CScript());
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CAmount nValueIn, CScript scriptSigIn = CScript());

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(prevout);
        READWRITE(*(CScriptBase *)(&scriptSig));
    }

    friend bool operator==(const CTxIn &a, const CTxIn &b) {
        return (a.prevout == b.prevout && a.scriptSig == b.scriptSig);
    }

    friend bool operator!=(const CTxIn &a, const CTxIn &b) { return !(a == b); }

    std::string ToString() const;
};

/**
 * An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut {
public:
    CAmount nValue;
    CAmount nPrincipal;
    CScript scriptPubKey;
    string strContent;
    uint32_t nLockTime;

    CTxOut() { SetNull(); }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, string strContentIn="",uint32_t nLockTime=0, const CAmount& nPrincipalIn=0);
    CTxOut(const CTxOut &txout);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(VARINT(nValue));
        READWRITE(VARINT(nPrincipal));
        READWRITE(*(CScriptBase *)(&scriptPubKey));
        READWRITE(LIMITED_STRING(strContent,1050000));
        READWRITE(VARINT(nLockTime));
    }

    void SetNull() {
        nValue = -1;
        nPrincipal = 0;
        scriptPubKey.clear();
        strContent="";
        nLockTime=0;
    }

    bool IsNull() const { return (nValue == -1 && nPrincipal == 0); }

    CAmount GetDustThreshold(const CFeeRate &minRelayTxFee) const {
        /**
         * "Dust" is defined in terms of CTransaction::minRelayTxFee, which has
         * units satoshis-per-kilobyte. If you'd pay more than 1/3 in fees to
         * spend something, then we consider it dust. A typical spendable
         * non-segwit txout is 34 bytes big, and will need a CTxIn of at least
         * 148 bytes to spend: so dust is a spendable txout less than
         * 546*minRelayTxFee/1000 (in satoshis). A typical spendable segwit
         * txout is 31 bytes big, and will need a CTxIn of at least 67 bytes to
         * spend: so dust is a spendable txout less than 294*minRelayTxFee/1000
         * (in satoshis).
         */
        if (scriptPubKey.IsUnspendable()) return 0;

        size_t nSize = GetSerializeSize(*this, SER_DISK, 0);

        // the 148 mentioned above
        nSize += (32 + 4 + 1 + 107 + 4);

        return 3 * minRelayTxFee.GetFee(nSize);
    }

    bool IsDust(const CFeeRate &minRelayTxFee) const {
        return (nValue < GetDustThreshold(minRelayTxFee));
    }

    friend bool operator==(const CTxOut &a, const CTxOut &b) {
        return (a.nValue       == b.nValue &&
                a.nPrincipal == b.nPrincipal &&
                a.scriptPubKey == b.scriptPubKey &&
                a.strContent == b.strContent &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTxOut &a, const CTxOut &b) {
        return !(a == b);
    }

    std::string ToString() const;
};

class CTxOutVerbose : public CTxOut {
public:
    uint256 txid;
    int n;
    int height;

public:
    CTxOutVerbose():CTxOut(){
        n = -1;
        height = -1;
    }

    CTxOutVerbose(const CTxOut &txout, const uint256 &txid, int n, int height):
        CTxOut(txout), txid(txid), n(n), height(height)
    {
    }

    CTxOutVerbose(const CTxOutVerbose &txout):
        CTxOut(txout), txid(txout.txid), n(txout.n), height(txout.height)
    {
    }
};

class CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - int32_t nFlags
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 */
template <typename Stream, typename TxType>
inline void UnserializeTransaction(TxType &tx, Stream &s) {
    s >> VARINT(tx.nVersion);
    s >> VARINT(tx.nFlags);
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an
     * empty vector. */
    s >> tx.vin;
    /* We read a non-empty vin. Assume a normal vout follows. */
    s >> tx.vout;
}

template <typename Stream, typename TxType>
inline void SerializeTransaction(const TxType &tx, Stream &s) {
    s << VARINT(tx.nVersion);
    s << VARINT(tx.nFlags);
    s << tx.vin;
    s << tx.vout;
}

enum txFlags {
    TX_FLAGS_NORMAL = 0,
    TX_FLAGS_COINBASE=1,
};


/**
 * The basic transaction that is broadcasted on the network and contained in
 * blocks. A transaction can contain multiple inputs and outputs.
 */
class CTransaction {
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION = 1;

    // Changing the default transaction version requires a two step process:
    // first adapting relay policy by bumping MAX_STANDARD_VERSION, and then
    // later date bumping the default CURRENT_VERSION at which point both
    // CURRENT_VERSION and MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION = 1;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const int32_t nFlags;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    CTransaction& operator=(const CTransaction& tx);


    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    /**
     * This deserializing constructor is provided instead of an Unserialize
     * method. Unserialize is not possible, since it would require overwriting
     * const fields.
     */
    template <typename Stream>
    CTransaction(deserialize_type, Stream &s)
        : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const { return vin.empty() && vout.empty(); }

    const TxId GetId() const { return TxId(hash); }
    const TxHash GetHash() const { return TxHash(hash); }

    // Return sum of txouts.
    CAmount GetValueOut() const;
    CAmount GetValueOutWithoutInterest() const;
    CAmount GetInterest() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    void ClearContent(CTransaction& newTx) const;

    // Compute priority, given priority of inputs and (optionally) tx size
    double ComputePriority(double dPriorityInputs,
                           unsigned int nTxSize = 0) const;

    // Compute modified tx size for priority calculation (optionally given tx
    // size)
    unsigned int CalculateModifiedSize(unsigned int nTxSize = 0) const;

    /**
     * Get the total transaction size in bytes.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const {
        return nFlags&TX_FLAGS_COINBASE;
        //return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction &a, const CTransaction &b) {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction &a, const CTransaction &b) {
        return a.hash != b.hash;
    }

    std::string ToString() const;
};

/**
 * A mutable version of CTransaction.
 */
class CMutableTransaction {
public:
    int32_t nVersion;
    int32_t nFlags;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;

    CMutableTransaction();
    CMutableTransaction(const CTransaction &tx);

    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream &s) {
        Unserialize(s);
    }

    /**
     * Compute the id and hash of this CMutableTransaction. This is computed on
     * the fly, as opposed to GetId() and GetHash() in CTransaction, which uses
     * a cached result.
     */
    TxId GetId() const;
    TxHash GetHash() const;

    friend bool operator==(const CMutableTransaction &a,
                           const CMutableTransaction &b) {
        return a.GetId() == b.GetId();
    }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() {
    return std::make_shared<const CTransaction>();
}
template <typename Tx>
static inline CTransactionRef MakeTransactionRef(Tx &&txIn) {
    return std::make_shared<const CTransaction>(std::forward<Tx>(txIn));
}

/** Compute the size of a transaction */
int64_t GetTransactionSize(const CTransaction &tx);

/** Precompute sighash midstate to avoid quadratic hashing */
struct PrecomputedTransactionData {
    uint256 hashPrevouts, hashOutputs;

    PrecomputedTransactionData()
        : hashPrevouts(), hashOutputs() {}

    PrecomputedTransactionData(const PrecomputedTransactionData &txdata)
        : hashPrevouts(txdata.hashPrevouts),
          hashOutputs(txdata.hashOutputs) {}

    PrecomputedTransactionData(const CTransaction &tx);
};

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
