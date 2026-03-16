// Copyright (c) 2026 Bitok Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// ATOM Layer — Pure L1 token on Bitok.
//
// Transfers are encoded as OP_RETURN outputs in Bitok transactions.
//
// OP_RETURN payload format — TRANSFER / COINBASE / BRIDGE_TO_BITOK (58 bytes):
//   [0..3]   magic     4 bytes   0x41544f4d ("ATOM")
//   [4]      version   1 byte    0x01
//   [5]      type      1 byte    0x01=transfer  0x05=bridge_to_bitok
//                                (0x04=coinbase is internal-only; user txs with this type are rejected)
//   [6..25]  from      20 bytes  HASH160(sender pubkey)
//   [26..45] to        20 bytes  HASH160(recipient pubkey)
//   [46..53] amount    8 bytes   base units big-endian
//   [54..57] nonce     4 bytes   per-sender monotonic counter big-endian
//
// OP_RETURN payload format — BRIDGE_TO_SOL (70 bytes):
//   [0..3]   magic     4 bytes   0x41544f4d ("ATOM")
//   [4]      version   1 byte    0x01
//   [5]      type      1 byte    0x03
//   [6..25]  from      20 bytes  HASH160(sender pubkey)
//   [26..57] sol_to    32 bytes  Solana destination public key (32 raw bytes)
//   [58..65] amount    8 bytes   base units big-endian
//   [66..69] nonce     4 bytes   per-sender monotonic counter big-endian

#ifndef ATOM_H
#define ATOM_H

class CTransaction;

static const unsigned char ATOM_MAGIC[4]           = { 0x41, 0x54, 0x4f, 0x4d };
static const unsigned char ATOM_VERSION            = 0x01;
static const unsigned char ATOM_TYPE_TRANSFER      = 0x01; // user-sendable: move ATOM
static const unsigned char ATOM_TYPE_BRIDGE_TO_SOL = 0x03; // user-sendable: bridge ATOM to Solana
static const unsigned char ATOM_TYPE_COINBASE      = 0x04; // INTERNAL: only written by ConnectBlock
static const unsigned char ATOM_TYPE_BRIDGE_TO_BITOK = 0x05; // bridge-only: mint ATOM from Solana side
static const unsigned char ATOM_TYPE_DEX_OFFER      = 0x06; // P2P DEX: place sell order (ATOM for BITOK)
static const unsigned char ATOM_TYPE_DEX_CANCEL      = 0x07; // P2P DEX: cancel an open order
static const unsigned char ATOM_TYPE_DEX_TAKE         = 0x08; // P2P DEX: take (fill) an existing order

// Bridge address: only this address may broadcast ATOM_TYPE_BRIDGE_TO_BITOK transactions.
// HASH160 of bridge
static const unsigned char ATOM_BRIDGE_HASH[20] = {
    0x70, 0x28, 0x78, 0x00, 0xce, 0x12, 0x3d, 0x72, 0x16, 0xb8,
    0x8d, 0x69, 0xe0, 0x8f, 0xa0, 0x1c, 0x32, 0x04, 0xce, 0xd2
};

static const int  ATOM_ACTIVATION_HEIGHT   = 25000;

static const int  ATOM_PAYLOAD_SIZE        = 58; // TRANSFER / COINBASE / BRIDGE_TO_BITOK
static const int  ATOM_BRIDGE_PAYLOAD_SIZE = 70; // BRIDGE_TO_SOL (32-byte Solana destination)
static const int  ATOM_DECIMALS_INT        = 6;
static const int64 ATOM_DECIMALS           = 1000000LL;                    // 6 decimal places
static const int64 ATOM_MAX_SUPPLY         = 1000000000LL * ATOM_DECIMALS; // 1,000,000,000.000000 ATOM

// Coinbase ATOM reward: 20 ATOM per 1 BITOK in block subsidy.
// At 50 BITOK block reward the miner receives 1000 ATOM.
// Scales with halvings identically to the BITOK subsidy.
static const int64 ATOM_PER_BITOK = 20LL * ATOM_DECIMALS; // raw ATOM units per 1 COIN

static const uint32_t ATOM_MAX_NONCE_GAP = 100;

static const int  ATOM_DEX_ACTIVATION_HEIGHT = 25000;
static const int  ATOM_DEX_OFFER_PAYLOAD_SIZE  = 62;
static const int  ATOM_DEX_CANCEL_PAYLOAD_SIZE = 58;
static const int  ATOM_DEX_TAKE_PAYLOAD_SIZE   = 58;
static const unsigned char ATOM_DEX_SIDE_SELL_ATOM = 0x00;

struct CAtomTransfer
{
    unsigned char nVersion;
    unsigned char nType;
    uint160       addrFrom;
    uint160       addrTo;          // zero for BRIDGE_TO_SOL; use solAddrTo instead
    unsigned char solAddrTo[32];   // Solana destination pubkey; only valid when nType == ATOM_TYPE_BRIDGE_TO_SOL
    int64         nAmount;
    uint32_t      nNonce;

    CAtomTransfer() : nVersion(ATOM_VERSION), nType(ATOM_TYPE_TRANSFER),
                      addrFrom(0), addrTo(0), nAmount(0), nNonce(0)
    {
        memset(solAddrTo, 0, 32);
    }

    bool IsValid() const
    {
        return (nVersion == ATOM_VERSION &&
                (nType == ATOM_TYPE_TRANSFER || nType == ATOM_TYPE_BRIDGE_TO_SOL ||
                 nType == ATOM_TYPE_BRIDGE_TO_BITOK) &&
                nAmount > 0 && nAmount <= ATOM_MAX_SUPPLY);
    }

    bool IsValidCoinbase() const
    {
        return (nVersion == ATOM_VERSION &&
                nType == ATOM_TYPE_COINBASE &&
                nAmount > 0 && nAmount <= ATOM_MAX_SUPPLY);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(addrFrom);
        READWRITE(addrTo);
        READWRITE(nAmount);
        READWRITE(nNonce);
    )
};

// ---------------------------------------------------------------------------
// DEX Order payload formats:
//
// DEX_OFFER (62 bytes):
//   [0..3]   magic     4 bytes   0x41544f4d
//   [4]      version   1 byte    0x01
//   [5]      type      1 byte    0x06
//   [6..25]  from      20 bytes  HASH160(maker pubkey)
//   [26]     side      1 byte    0x00=sell_atom (only sell supported)
//   [27..34] atom_qty  8 bytes   ATOM amount in base units, big-endian
//   [35..42] price     8 bytes   BITOK satoshi per 1 ATOM (base unit), big-endian
//   [43..46] nonce     4 bytes   per-sender monotonic counter, big-endian
//   [47..61] reserved  15 bytes  zero (future use)
//
// DEX_CANCEL (58 bytes):
//   [0..3]   magic     4 bytes
//   [4]      version   1 byte    0x01
//   [5]      type      1 byte    0x07
//   [6..25]  from      20 bytes  HASH160(maker pubkey) — must match original offer
//   [26..57] offer_tx  32 bytes  txhash of the DEX_OFFER being cancelled
//
// DEX_TAKE (58 bytes):
//   [0..3]   magic     4 bytes
//   [4]      version   1 byte    0x01
//   [5]      type      1 byte    0x08
//   [6..25]  from      20 bytes  HASH160(taker pubkey)
//   [26..57] offer_tx  32 bytes  txhash of the DEX_OFFER being taken
//
// Settlement: when DEX_TAKE is mined, ConnectBlock atomically:
//   debit maker ATOM (escrowed at offer time), credit taker ATOM;
//   taker pays BITOK to maker via tx outputs (validated in consensus)
// ---------------------------------------------------------------------------

struct CAtomDexOffer
{
    unsigned char nVersion;
    unsigned char nType;
    uint160       addrFrom;
    unsigned char nSide;
    int64         nAtomAmount;
    int64         nPrice;
    uint32_t      nNonce;

    CAtomDexOffer() : nVersion(ATOM_VERSION), nType(ATOM_TYPE_DEX_OFFER),
                      addrFrom(0), nSide(ATOM_DEX_SIDE_SELL_ATOM),
                      nAtomAmount(0), nPrice(0), nNonce(0) {}

    bool IsValid() const
    {
        return (nVersion == ATOM_VERSION &&
                nType == ATOM_TYPE_DEX_OFFER &&
                nSide == ATOM_DEX_SIDE_SELL_ATOM &&
                nAtomAmount > 0 && nAtomAmount <= ATOM_MAX_SUPPLY &&
                nPrice > 0);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(addrFrom);
        READWRITE(nSide);
        READWRITE(nAtomAmount);
        READWRITE(nPrice);
        READWRITE(nNonce);
    )
};

struct CAtomDexCancel
{
    unsigned char nVersion;
    unsigned char nType;
    uint160       addrFrom;
    uint256       hashOffer;

    CAtomDexCancel() : nVersion(ATOM_VERSION), nType(ATOM_TYPE_DEX_CANCEL),
                       addrFrom(0), hashOffer(0) {}

    bool IsValid() const
    {
        return (nVersion == ATOM_VERSION &&
                nType == ATOM_TYPE_DEX_CANCEL &&
                hashOffer != 0);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(addrFrom);
        READWRITE(hashOffer);
    )
};

struct CAtomDexTake
{
    unsigned char nVersion;
    unsigned char nType;
    uint160       addrFrom;
    uint256       hashOffer;

    CAtomDexTake() : nVersion(ATOM_VERSION), nType(ATOM_TYPE_DEX_TAKE),
                     addrFrom(0), hashOffer(0) {}

    bool IsValid() const
    {
        return (nVersion == ATOM_VERSION &&
                nType == ATOM_TYPE_DEX_TAKE &&
                hashOffer != 0);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(addrFrom);
        READWRITE(hashOffer);
    )
};

// Parse ATOM transfer from a CScript containing OP_RETURN data.
// Returns true if the script is a valid ATOM OP_RETURN payload.
bool ParseAtomScript(const CScript& script, CAtomTransfer& transferOut);

// Build an OP_RETURN CScript encoding a CAtomTransfer.
CScript BuildAtomScript(const CAtomTransfer& transfer);

// Encode/decode the ATOM payload to/from raw bytes (big-endian fields).
// BRIDGE_TO_SOL uses the 70-byte extended format; all others use 58 bytes.
bool EncodeAtomPayload(const CAtomTransfer& transfer, std::vector<unsigned char>& vchOut);
bool DecodeAtomPayload(const std::vector<unsigned char>& vch, CAtomTransfer& transferOut);

// Scan all outputs of a transaction and return the first ATOM transfer found.
bool GetAtomTransfer(const CTransaction& tx, CAtomTransfer& transferOut);

// In-memory mempool ATOM nonce index.
// Prevents nonce replay and double-spend of ATOM before a block is mined.
extern CCriticalSection cs_mapAtomMempool;
extern std::map<uint160, uint32_t> mapAtomMempoolNonce;  // highest pending nonce per sender
extern std::map<uint256, CAtomTransfer> mapAtomMempoolTx; // txhash -> pending transfer

// Add/remove a transfer from the mempool ATOM index.
void AtomMempoolAdd(const uint256& txhash, const CAtomTransfer& transfer);
void AtomMempoolRemove(const uint256& txhash);

// Check that a nonce is acceptable: must be > nConfirmedNonce and not already in mempool.
bool AtomMempoolCheckNonce(const uint160& addrFrom, uint32_t nNonce, uint32_t nConfirmedNonce);

// Compute how much of addrFrom's confirmed balance is already committed in the mempool
// (sum of all pending outbound transfer amounts for that address).
// Used by sendatom and AcceptTransaction to prevent overdraft.
int64 AtomMempoolPendingOutbound(const uint160& addrFrom);

// Compute the spendable balance: confirmed balance minus pending outbound in mempool.
int64 AtomGetEffectiveBalance(const uint160& addr, int64 nConfirmedBalance);

// Verify that at least one input in the transaction was signed by a key whose
// Hash160 matches addrFrom.  Prevents ATOM theft via forged addrFrom fields.
// For coinbase transactions (no real inputs) this always returns false.
bool VerifyAtomSender(const CTransaction& tx, const uint160& addrFrom);

inline int64 DexComputeBitokPayment(int64 nAtomAmount, int64 nPrice)
{
    __int128 n = (__int128)nAtomAmount * (__int128)nPrice;
    int64 result = (int64)(n / ATOM_DECIMALS);
    if (result < 0) result = 0;
    return result;
}

// ---------------------------------------------------------------------------
// DEX payload encode/decode
// ---------------------------------------------------------------------------
bool EncodeDexOfferPayload(const CAtomDexOffer& offer, std::vector<unsigned char>& vchOut);
bool DecodeDexOfferPayload(const std::vector<unsigned char>& vch, CAtomDexOffer& offer);
bool EncodeDexCancelPayload(const CAtomDexCancel& cancel, std::vector<unsigned char>& vchOut);
bool DecodeDexCancelPayload(const std::vector<unsigned char>& vch, CAtomDexCancel& cancel);
bool EncodeDexTakePayload(const CAtomDexTake& take, std::vector<unsigned char>& vchOut);
bool DecodeDexTakePayload(const std::vector<unsigned char>& vch, CAtomDexTake& take);

CScript BuildDexOfferScript(const CAtomDexOffer& offer);
CScript BuildDexCancelScript(const CAtomDexCancel& cancel);
CScript BuildDexTakeScript(const CAtomDexTake& take);

bool ParseDexScript(const CScript& script, unsigned char& nTypeOut,
                    CAtomDexOffer& offer, CAtomDexCancel& cancel, CAtomDexTake& take);
bool GetDexPayload(const CTransaction& tx, unsigned char& nTypeOut,
                   CAtomDexOffer& offer, CAtomDexCancel& cancel, CAtomDexTake& take);

// ---------------------------------------------------------------------------
// DEX order book (in-memory, rebuilt from DB on startup)
// ---------------------------------------------------------------------------
struct CDexOrderEntry
{
    uint256       hashTx;
    uint160       addrMaker;
    unsigned char nSide;
    int64         nAtomAmount;
    int64         nPrice;
    uint32_t      nNonce;
    int           nHeight;
    unsigned int  nTime;

    CDexOrderEntry() : hashTx(0), addrMaker(0), nSide(0),
                       nAtomAmount(0), nPrice(0), nNonce(0), nHeight(0), nTime(0) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(hashTx);
        READWRITE(addrMaker);
        READWRITE(nSide);
        READWRITE(nAtomAmount);
        READWRITE(nPrice);
        READWRITE(nNonce);
        READWRITE(nHeight);
        READWRITE(nTime);
    )
};

extern CCriticalSection cs_mapDexOrders;
extern std::map<uint256, CDexOrderEntry> mapDexOrders;
extern std::map<uint256, CAtomDexOffer> mapDexMempoolOffers;
extern std::set<uint256> setDexMempoolCancels;
extern std::set<uint256> setDexMempoolTakes;
extern std::map<uint256, uint256> mapDexMempoolCancelTx;
extern std::map<uint256, uint256> mapDexMempoolTakeTx;

void DexMempoolAddOffer(const uint256& txhash, const CAtomDexOffer& offer);
void DexMempoolAddCancel(const uint256& txhash, const uint256& hashOffer);
void DexMempoolAddTake(const uint256& txhash, const uint256& hashOffer);
bool DexMempoolHasCancelForOffer(const uint256& hashOffer);
bool DexMempoolHasTakeForOffer(const uint256& hashOffer);
void DexMempoolRemove(const uint256& txhash);

#endif // ATOM_H
