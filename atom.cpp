// Copyright (c) 2026 Bitok Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "atom.h"

CCriticalSection cs_mapAtomMempool;
std::map<uint160, uint32_t>      mapAtomMempoolNonce;
std::map<uint256, CAtomTransfer> mapAtomMempoolTx;


// ---------------------------------------------------------------------------
// Payload encoding / decoding
//   BRIDGE_TO_SOL: 70 bytes  (32-byte Solana destination replaces 20-byte addrTo)
//   all other types: 58 bytes
// ---------------------------------------------------------------------------

bool EncodeAtomPayload(const CAtomTransfer& t, std::vector<unsigned char>& vchOut)
{
    if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
    {
        vchOut.resize(ATOM_BRIDGE_PAYLOAD_SIZE);
        unsigned char* p = &vchOut[0];

        p[0] = ATOM_MAGIC[0];
        p[1] = ATOM_MAGIC[1];
        p[2] = ATOM_MAGIC[2];
        p[3] = ATOM_MAGIC[3];
        p[4] = t.nVersion;
        p[5] = t.nType;

        memcpy(p + 6,  t.addrFrom.begin(), 20);
        memcpy(p + 26, t.solAddrTo,        32);

        int64 nAmt = t.nAmount;
        for (int i = 7; i >= 0; i--)
        {
            p[58 + i] = (unsigned char)(nAmt & 0xff);
            nAmt >>= 8;
        }

        uint32_t nN = t.nNonce;
        p[66] = (unsigned char)((nN >> 24) & 0xff);
        p[67] = (unsigned char)((nN >> 16) & 0xff);
        p[68] = (unsigned char)((nN >>  8) & 0xff);
        p[69] = (unsigned char)( nN        & 0xff);

        return true;
    }

    vchOut.resize(ATOM_PAYLOAD_SIZE);
    unsigned char* p = &vchOut[0];

    p[0] = ATOM_MAGIC[0];
    p[1] = ATOM_MAGIC[1];
    p[2] = ATOM_MAGIC[2];
    p[3] = ATOM_MAGIC[3];
    p[4] = t.nVersion;
    p[5] = t.nType;

    memcpy(p + 6,  t.addrFrom.begin(), 20);
    memcpy(p + 26, t.addrTo.begin(),   20);

    int64 nAmt = t.nAmount;
    for (int i = 7; i >= 0; i--)
    {
        p[46 + i] = (unsigned char)(nAmt & 0xff);
        nAmt >>= 8;
    }

    uint32_t nN = t.nNonce;
    p[54] = (unsigned char)((nN >> 24) & 0xff);
    p[55] = (unsigned char)((nN >> 16) & 0xff);
    p[56] = (unsigned char)((nN >>  8) & 0xff);
    p[57] = (unsigned char)( nN        & 0xff);

    return true;
}

bool DecodeAtomPayload(const std::vector<unsigned char>& vch, CAtomTransfer& t)
{
    if ((int)vch.size() < ATOM_PAYLOAD_SIZE)
        return false;

    const unsigned char* p = &vch[0];

    if (p[0] != ATOM_MAGIC[0] || p[1] != ATOM_MAGIC[1] ||
        p[2] != ATOM_MAGIC[2] || p[3] != ATOM_MAGIC[3])
        return false;

    t.nVersion = p[4];
    if (t.nVersion != ATOM_VERSION)
        return false;

    t.nType = p[5];
    if (t.nType != ATOM_TYPE_TRANSFER &&
        t.nType != ATOM_TYPE_BRIDGE_TO_SOL &&
        t.nType != ATOM_TYPE_BRIDGE_TO_BITOK)
        return false;

    if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
    {
        if ((int)vch.size() < ATOM_BRIDGE_PAYLOAD_SIZE)
            return false;

        t.addrFrom = uint160(std::vector<unsigned char>(p + 6, p + 26));
        t.addrTo   = uint160(0);
        memcpy(t.solAddrTo, p + 26, 32);

        bool fAllZero = true;
        for (int i = 0; i < 32; i++)
            if (t.solAddrTo[i] != 0) { fAllZero = false; break; }
        if (fAllZero)
            return false;

        int64 nAmt = 0;
        for (int i = 0; i < 8; i++)
        {
            nAmt <<= 8;
            nAmt |= (int64)(unsigned char)p[58 + i];
        }
        t.nAmount = nAmt;

        t.nNonce = ((uint32_t)p[66] << 24) |
                   ((uint32_t)p[67] << 16) |
                   ((uint32_t)p[68] <<  8) |
                   ((uint32_t)p[69]);

        return t.IsValid();
    }

    t.addrFrom = uint160(std::vector<unsigned char>(p + 6,  p + 26));
    t.addrTo   = uint160(std::vector<unsigned char>(p + 26, p + 46));
    memset(t.solAddrTo, 0, 32);

    int64 nAmt = 0;
    for (int i = 0; i < 8; i++)
    {
        nAmt <<= 8;
        nAmt |= (int64)(unsigned char)p[46 + i];
    }
    t.nAmount = nAmt;

    t.nNonce = ((uint32_t)p[54] << 24) |
               ((uint32_t)p[55] << 16) |
               ((uint32_t)p[56] <<  8) |
               ((uint32_t)p[57]);

    return t.IsValid();
}


// ---------------------------------------------------------------------------
// Script parsing / building
// ---------------------------------------------------------------------------

bool ParseAtomScript(const CScript& script, CAtomTransfer& transferOut)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> vchData;

    if (!script.GetOp(pc, opcode))
        return false;
    if (opcode != OP_RETURN)
        return false;

    if (!script.GetOp(pc, opcode, vchData))
        return false;

    return DecodeAtomPayload(vchData, transferOut);
}

CScript BuildAtomScript(const CAtomTransfer& transfer)
{
    std::vector<unsigned char> vchPayload;
    EncodeAtomPayload(transfer, vchPayload);

    CScript script;
    script << OP_RETURN << vchPayload;
    return script;
}

bool GetAtomTransfer(const CTransaction& tx, CAtomTransfer& transferOut)
{
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        if (ParseAtomScript(tx.vout[i].scriptPubKey, transferOut))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Mempool ATOM index
// ---------------------------------------------------------------------------

void AtomMempoolAdd(const uint256& txhash, const CAtomTransfer& transfer)
{
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        mapAtomMempoolTx[txhash] = transfer;

        // Track highest pending nonce for the sender
        std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(transfer.addrFrom);
        if (it == mapAtomMempoolNonce.end() || transfer.nNonce > it->second)
            mapAtomMempoolNonce[transfer.addrFrom] = transfer.nNonce;
    }
}

void AtomMempoolRemove(const uint256& txhash)
{
    uint160 addrFrom;
    bool fRemoved = false;

    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.find(txhash);
        if (it == mapAtomMempoolTx.end())
            return;

        addrFrom = it->second.addrFrom;
        mapAtomMempoolTx.erase(it);
        fRemoved = true;
    }

    if (fRemoved)
    {
        uint32_t nNewMax = 0;
        bool fFound = false;
        CRITICAL_BLOCK(cs_mapDexOrders)
        {
            for (std::map<uint256, CAtomDexOffer>::iterator jt = mapDexMempoolOffers.begin();
                 jt != mapDexMempoolOffers.end(); ++jt)
            {
                if (jt->second.addrFrom == addrFrom)
                {
                    if (!fFound || jt->second.nNonce > nNewMax)
                        nNewMax = jt->second.nNonce;
                    fFound = true;
                }
            }
        }
        CRITICAL_BLOCK(cs_mapAtomMempool)
        {
            for (std::map<uint256, CAtomTransfer>::iterator jt = mapAtomMempoolTx.begin();
                 jt != mapAtomMempoolTx.end(); ++jt)
            {
                if (jt->second.addrFrom == addrFrom)
                {
                    if (!fFound || jt->second.nNonce > nNewMax)
                        nNewMax = jt->second.nNonce;
                    fFound = true;
                }
            }
            if (fFound)
                mapAtomMempoolNonce[addrFrom] = nNewMax;
            else
                mapAtomMempoolNonce.erase(addrFrom);
        }
    }
}

// Returns true if nNonce is acceptable:
//   - strictly greater than the last confirmed nonce
//   - not duplicated by another pending mempool tx from the same sender
bool AtomMempoolCheckNonce(const uint160& addrFrom, uint32_t nNonce, uint32_t nConfirmedNonce)
{
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        if (nNonce <= nConfirmedNonce)
            return false;

        if (nNonce > nConfirmedNonce + ATOM_MAX_NONCE_GAP)
            return false;

        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            if (it->second.addrFrom == addrFrom && it->second.nNonce == nNonce)
                return false;
        }
    }
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CAtomDexOffer>::iterator it = mapDexMempoolOffers.begin();
             it != mapDexMempoolOffers.end(); ++it)
        {
            if (it->second.addrFrom == addrFrom && it->second.nNonce == nNonce)
                return false;
        }
    }
    return true;
}

// Sum of all outbound ATOM amounts pending in the mempool for the given address.
// Covers TRANSFER and BURN since both debit the sender.
int64 AtomMempoolPendingOutbound(const uint160& addrFrom)
{
    int64 nPending = 0;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            const CAtomTransfer& t = it->second;
            if (t.addrFrom == addrFrom &&
                (t.nType == ATOM_TYPE_TRANSFER || t.nType == ATOM_TYPE_BRIDGE_TO_SOL))
            {
                if (t.nAmount > 0 && nPending > ATOM_MAX_SUPPLY - t.nAmount)
                    nPending = ATOM_MAX_SUPPLY;
                else
                    nPending += t.nAmount;
            }
        }
    }
    return nPending;
}

// Spendable balance: confirmed balance minus pending outbound in the mempool.
int64 AtomGetEffectiveBalance(const uint160& addr, int64 nConfirmedBalance)
{
    int64 nEffective = nConfirmedBalance;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            const CAtomTransfer& t = it->second;
            if (t.nType == ATOM_TYPE_TRANSFER || t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
            {
                if (t.addrFrom == addr) nEffective -= t.nAmount;
            }
        }
    }
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CAtomDexOffer>::iterator it = mapDexMempoolOffers.begin();
             it != mapDexMempoolOffers.end(); ++it)
        {
            const CAtomDexOffer& o = it->second;
            if (o.nSide == ATOM_DEX_SIDE_SELL_ATOM && o.addrFrom == addr)
                nEffective -= o.nAtomAmount;
        }
    }
    if (nEffective < 0) nEffective = 0;
    return nEffective;
}

// ---------------------------------------------------------------------------
// Sender verification — extract pubkey from scriptSig and check Hash160
// ---------------------------------------------------------------------------

static bool ExtractPubKeyFromScriptSig(const CScript& scriptSig, std::vector<unsigned char>& vchPubKeyOut)
{
    CScript::const_iterator pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<unsigned char> vchLast;
    while (pc < scriptSig.end())
    {
        std::vector<unsigned char> vchData;
        if (!scriptSig.GetOp(pc, opcode, vchData))
            break;
        if (vchData.size() >= 33 && vchData.size() <= 65)
            vchLast = vchData;
    }
    if (vchLast.empty())
        return false;
    vchPubKeyOut = vchLast;
    return true;
}

bool VerifyAtomSender(const CTransaction& tx, const uint160& addrFrom)
{
    if (tx.IsCoinBase())
        return false;

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        std::vector<unsigned char> vchPubKey;
        if (ExtractPubKeyFromScriptSig(tx.vin[i].scriptSig, vchPubKey))
        {
            if (Hash160(vchPubKey) == addrFrom)
                return true;
        }
    }
    return false;
}


// ---------------------------------------------------------------------------
// DEX globals
// ---------------------------------------------------------------------------

CCriticalSection cs_mapDexOrders;
std::map<uint256, CDexOrderEntry> mapDexOrders;
std::map<uint256, CAtomDexOffer>  mapDexMempoolOffers;
std::set<uint256> setDexMempoolCancels;
std::set<uint256> setDexMempoolTakes;
std::map<uint256, uint256> mapDexMempoolCancelTx;
std::map<uint256, uint256> mapDexMempoolTakeTx;


// ---------------------------------------------------------------------------
// DEX payload encoding
// ---------------------------------------------------------------------------

static void WriteBE64(unsigned char* p, int64 v)
{
    for (int i = 7; i >= 0; i--)
    {
        p[i] = (unsigned char)(v & 0xff);
        v >>= 8;
    }
}

static int64 ReadBE64(const unsigned char* p)
{
    int64 v = 0;
    for (int i = 0; i < 8; i++)
    {
        v <<= 8;
        v |= (int64)(unsigned char)p[i];
    }
    return v;
}

static void WriteBE32(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)((v >> 24) & 0xff);
    p[1] = (unsigned char)((v >> 16) & 0xff);
    p[2] = (unsigned char)((v >>  8) & 0xff);
    p[3] = (unsigned char)( v        & 0xff);
}

static uint32_t ReadBE32(const unsigned char* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) | ((uint32_t)p[3]);
}

bool EncodeDexOfferPayload(const CAtomDexOffer& o, std::vector<unsigned char>& vchOut)
{
    vchOut.resize(ATOM_DEX_OFFER_PAYLOAD_SIZE, 0);
    unsigned char* p = &vchOut[0];

    p[0] = ATOM_MAGIC[0]; p[1] = ATOM_MAGIC[1];
    p[2] = ATOM_MAGIC[2]; p[3] = ATOM_MAGIC[3];
    p[4] = o.nVersion;
    p[5] = o.nType;
    memcpy(p + 6, o.addrFrom.begin(), 20);
    p[26] = o.nSide;
    WriteBE64(p + 27, o.nAtomAmount);
    WriteBE64(p + 35, o.nPrice);
    WriteBE32(p + 43, o.nNonce);
    return true;
}

bool DecodeDexOfferPayload(const std::vector<unsigned char>& vch, CAtomDexOffer& o)
{
    if ((int)vch.size() < ATOM_DEX_OFFER_PAYLOAD_SIZE)
        return false;

    const unsigned char* p = &vch[0];

    if (p[0] != ATOM_MAGIC[0] || p[1] != ATOM_MAGIC[1] ||
        p[2] != ATOM_MAGIC[2] || p[3] != ATOM_MAGIC[3])
        return false;

    o.nVersion = p[4];
    if (o.nVersion != ATOM_VERSION)
        return false;
    o.nType = p[5];
    if (o.nType != ATOM_TYPE_DEX_OFFER)
        return false;

    o.addrFrom    = uint160(std::vector<unsigned char>(p + 6, p + 26));
    o.nSide       = p[26];
    o.nAtomAmount = ReadBE64(p + 27);
    o.nPrice      = ReadBE64(p + 35);
    o.nNonce      = ReadBE32(p + 43);

    return o.IsValid();
}

bool EncodeDexCancelPayload(const CAtomDexCancel& c, std::vector<unsigned char>& vchOut)
{
    vchOut.resize(ATOM_DEX_CANCEL_PAYLOAD_SIZE, 0);
    unsigned char* p = &vchOut[0];

    p[0] = ATOM_MAGIC[0]; p[1] = ATOM_MAGIC[1];
    p[2] = ATOM_MAGIC[2]; p[3] = ATOM_MAGIC[3];
    p[4] = c.nVersion;
    p[5] = c.nType;
    memcpy(p + 6, c.addrFrom.begin(), 20);
    memcpy(p + 26, c.hashOffer.begin(), 32);
    return true;
}

bool DecodeDexCancelPayload(const std::vector<unsigned char>& vch, CAtomDexCancel& c)
{
    if ((int)vch.size() < ATOM_DEX_CANCEL_PAYLOAD_SIZE)
        return false;

    const unsigned char* p = &vch[0];

    if (p[0] != ATOM_MAGIC[0] || p[1] != ATOM_MAGIC[1] ||
        p[2] != ATOM_MAGIC[2] || p[3] != ATOM_MAGIC[3])
        return false;

    c.nVersion = p[4];
    if (c.nVersion != ATOM_VERSION)
        return false;
    c.nType = p[5];
    if (c.nType != ATOM_TYPE_DEX_CANCEL)
        return false;

    c.addrFrom  = uint160(std::vector<unsigned char>(p + 6, p + 26));
    c.hashOffer = uint256(std::vector<unsigned char>(p + 26, p + 58));

    return c.IsValid();
}

bool EncodeDexTakePayload(const CAtomDexTake& t, std::vector<unsigned char>& vchOut)
{
    vchOut.resize(ATOM_DEX_TAKE_PAYLOAD_SIZE, 0);
    unsigned char* p = &vchOut[0];

    p[0] = ATOM_MAGIC[0]; p[1] = ATOM_MAGIC[1];
    p[2] = ATOM_MAGIC[2]; p[3] = ATOM_MAGIC[3];
    p[4] = t.nVersion;
    p[5] = t.nType;
    memcpy(p + 6, t.addrFrom.begin(), 20);
    memcpy(p + 26, t.hashOffer.begin(), 32);
    return true;
}

bool DecodeDexTakePayload(const std::vector<unsigned char>& vch, CAtomDexTake& t)
{
    if ((int)vch.size() < ATOM_DEX_TAKE_PAYLOAD_SIZE)
        return false;

    const unsigned char* p = &vch[0];

    if (p[0] != ATOM_MAGIC[0] || p[1] != ATOM_MAGIC[1] ||
        p[2] != ATOM_MAGIC[2] || p[3] != ATOM_MAGIC[3])
        return false;

    t.nVersion = p[4];
    if (t.nVersion != ATOM_VERSION)
        return false;
    t.nType = p[5];
    if (t.nType != ATOM_TYPE_DEX_TAKE)
        return false;

    t.addrFrom  = uint160(std::vector<unsigned char>(p + 6, p + 26));
    t.hashOffer = uint256(std::vector<unsigned char>(p + 26, p + 58));

    return t.IsValid();
}


// ---------------------------------------------------------------------------
// DEX script build / parse
// ---------------------------------------------------------------------------

CScript BuildDexOfferScript(const CAtomDexOffer& offer)
{
    std::vector<unsigned char> vch;
    EncodeDexOfferPayload(offer, vch);
    CScript script;
    script << OP_RETURN << vch;
    return script;
}

CScript BuildDexCancelScript(const CAtomDexCancel& cancel)
{
    std::vector<unsigned char> vch;
    EncodeDexCancelPayload(cancel, vch);
    CScript script;
    script << OP_RETURN << vch;
    return script;
}

CScript BuildDexTakeScript(const CAtomDexTake& take)
{
    std::vector<unsigned char> vch;
    EncodeDexTakePayload(take, vch);
    CScript script;
    script << OP_RETURN << vch;
    return script;
}

bool ParseDexScript(const CScript& script, unsigned char& nTypeOut,
                    CAtomDexOffer& offer, CAtomDexCancel& cancel, CAtomDexTake& take)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> vchData;

    if (!script.GetOp(pc, opcode))
        return false;
    if (opcode != OP_RETURN)
        return false;
    if (!script.GetOp(pc, opcode, vchData))
        return false;

    if ((int)vchData.size() < 6)
        return false;
    if (vchData[0] != ATOM_MAGIC[0] || vchData[1] != ATOM_MAGIC[1] ||
        vchData[2] != ATOM_MAGIC[2] || vchData[3] != ATOM_MAGIC[3])
        return false;

    unsigned char nType = vchData[5];
    if (nType == ATOM_TYPE_DEX_OFFER && DecodeDexOfferPayload(vchData, offer))
    {
        nTypeOut = ATOM_TYPE_DEX_OFFER;
        return true;
    }
    if (nType == ATOM_TYPE_DEX_CANCEL && DecodeDexCancelPayload(vchData, cancel))
    {
        nTypeOut = ATOM_TYPE_DEX_CANCEL;
        return true;
    }
    if (nType == ATOM_TYPE_DEX_TAKE && DecodeDexTakePayload(vchData, take))
    {
        nTypeOut = ATOM_TYPE_DEX_TAKE;
        return true;
    }
    return false;
}

bool GetDexPayload(const CTransaction& tx, unsigned char& nTypeOut,
                   CAtomDexOffer& offer, CAtomDexCancel& cancel, CAtomDexTake& take)
{
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        if (ParseDexScript(tx.vout[i].scriptPubKey, nTypeOut, offer, cancel, take))
            return true;
    }
    return false;
}


// ---------------------------------------------------------------------------
// DEX mempool helpers
// ---------------------------------------------------------------------------

void DexMempoolAddOffer(const uint256& txhash, const CAtomDexOffer& offer)
{
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        mapDexMempoolOffers[txhash] = offer;
    }
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(offer.addrFrom);
        if (it == mapAtomMempoolNonce.end() || offer.nNonce > it->second)
            mapAtomMempoolNonce[offer.addrFrom] = offer.nNonce;
    }
}

void DexMempoolAddCancel(const uint256& txhash, const uint256& hashOffer)
{
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        setDexMempoolCancels.insert(hashOffer);
        mapDexMempoolCancelTx[txhash] = hashOffer;
    }
}

void DexMempoolAddTake(const uint256& txhash, const uint256& hashOffer)
{
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        setDexMempoolTakes.insert(hashOffer);
        mapDexMempoolTakeTx[txhash] = hashOffer;
    }
}

bool DexMempoolHasCancelForOffer(const uint256& hashOffer)
{
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        return setDexMempoolCancels.count(hashOffer) > 0;
    }
    return false;
}

bool DexMempoolHasTakeForOffer(const uint256& hashOffer)
{
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        return setDexMempoolTakes.count(hashOffer) > 0;
    }
    return false;
}

void DexMempoolRemove(const uint256& txhash)
{
    uint160 addrOffer;
    bool fWasOffer = false;

    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        std::map<uint256, CAtomDexOffer>::iterator itOffer = mapDexMempoolOffers.find(txhash);
        if (itOffer != mapDexMempoolOffers.end())
        {
            addrOffer = itOffer->second.addrFrom;
            fWasOffer = true;
            mapDexMempoolOffers.erase(itOffer);
        }
        {
            std::map<uint256, uint256>::iterator it = mapDexMempoolCancelTx.find(txhash);
            if (it != mapDexMempoolCancelTx.end())
            {
                setDexMempoolCancels.erase(it->second);
                mapDexMempoolCancelTx.erase(it);
            }
        }
        {
            std::map<uint256, uint256>::iterator it = mapDexMempoolTakeTx.find(txhash);
            if (it != mapDexMempoolTakeTx.end())
            {
                setDexMempoolTakes.erase(it->second);
                mapDexMempoolTakeTx.erase(it);
            }
        }
    }

    if (fWasOffer)
    {
        uint32_t nNewMax = 0;
        bool fFound = false;
        CRITICAL_BLOCK(cs_mapDexOrders)
        {
            for (std::map<uint256, CAtomDexOffer>::iterator jt = mapDexMempoolOffers.begin();
                 jt != mapDexMempoolOffers.end(); ++jt)
            {
                if (jt->second.addrFrom == addrOffer)
                {
                    if (!fFound || jt->second.nNonce > nNewMax)
                        nNewMax = jt->second.nNonce;
                    fFound = true;
                }
            }
        }
        CRITICAL_BLOCK(cs_mapAtomMempool)
        {
            for (std::map<uint256, CAtomTransfer>::iterator jt = mapAtomMempoolTx.begin();
                 jt != mapAtomMempoolTx.end(); ++jt)
            {
                if (jt->second.addrFrom == addrOffer)
                {
                    if (!fFound || jt->second.nNonce > nNewMax)
                        nNewMax = jt->second.nNonce;
                    fFound = true;
                }
            }
            if (fFound)
                mapAtomMempoolNonce[addrOffer] = nNewMax;
            else
                mapAtomMempoolNonce.erase(addrOffer);
        }
    }
}
