# BITOK/ATOM DEX — Technical Reference

Built-in peer-to-peer exchange between BITOK (native coin) and ATOM (trust scoring system token built on BITOK blockchain). Settlement is fully atomic and enforced at the consensus layer. No trusted intermediary, no off-chain components.

---

## Design Overview

The DEX uses **on-chain escrow + script settlement**. A maker sells ATOM by broadcasting a `DEX_OFFER` transaction that locks their ATOM in their account. A taker fills the order by broadcasting a `DEX_TAKE` transaction that pays BITOK directly to the maker's address. When `ConnectBlock` processes the take, ATOM is atomically credited to the taker and the escrow is released — all in a single block.

Only **SELL_ATOM** orders exist at the consensus level. "Buying" ATOM means finding and taking an existing sell order. The `dexbuy` RPC wraps this in one command with automatic order selection.

### Key Properties

- ATOM escrow is debited when `DEX_OFFER` is mined (not at broadcast)
- BITOK payment is validated by scanning tx outputs in `ConnectBlock`
- Taker's ATOM credit and order erasure happen in the same block as payment
- Full reorganization safety via `DisconnectBlock` reversal
- No third-party scripts, no multisig, no off-chain state

---

## Activation

```
ATOM_DEX_ACTIVATION_HEIGHT = 25000
```

DEX transactions before block 25000 are rejected. This is the same height at which the broader ATOM layer activates.

---

## Transaction Types

Three new OP_RETURN payload types extend the ATOM wire format:

| Type constant | Byte value | Purpose |
|---|---|---|
| `ATOM_TYPE_DEX_OFFER` | `0x06` | Place a sell order, escrow ATOM |
| `ATOM_TYPE_DEX_CANCEL` | `0x07` | Cancel an open sell order, return ATOM |
| `ATOM_TYPE_DEX_TAKE` | `0x08` | Fill an open sell order, pay BITOK, receive ATOM |

---

## Wire Format

All DEX payloads begin with the same 6-byte header as ATOM transfers:

```
[0..3]  magic    4 bytes   0x41 0x54 0x4F 0x4D  ("ATOM")
[4]     version  1 byte    0x01
[5]     type     1 byte    0x06 / 0x07 / 0x08
```

### DEX_OFFER payload — 62 bytes total

```
[0..3]   magic       4 bytes   0x41544f4d
[4]      version     1 byte    0x01
[5]      type        1 byte    0x06
[6..25]  from        20 bytes  HASH160(maker pubkey)
[26]     side        1 byte    0x00 (SELL_ATOM — only value accepted)
[27..34] atom_qty    8 bytes   ATOM amount in base units, big-endian
[35..42] price       8 bytes   BITOK satoshi per 1 ATOM base unit, big-endian
[43..46] nonce       4 bytes   per-sender monotonic counter, big-endian
[47..61] reserved    15 bytes  zero
```

### DEX_CANCEL payload — 58 bytes total

```
[0..3]   magic       4 bytes   0x41544f4d
[4]      version     1 byte    0x01
[5]      type        1 byte    0x07
[6..25]  from        20 bytes  HASH160(maker pubkey) — must match original offer
[26..57] offer_tx    32 bytes  txhash of the DEX_OFFER being cancelled
```

### DEX_TAKE payload — 58 bytes total

```
[0..3]   magic       4 bytes   0x41544f4d
[4]      version     1 byte    0x01
[5]      type        1 byte    0x08
[6..25]  from        20 bytes  HASH160(taker pubkey)
[26..57] offer_tx    32 bytes  txhash of the DEX_OFFER being taken
```

All payloads are wrapped in a single `OP_RETURN` output script:
```
OP_RETURN <payload_bytes>
```

---

## Data Structures

### `CAtomDexOffer` (atom.h)

```cpp
unsigned char  nVersion     // must be ATOM_VERSION (0x01)
unsigned char  nType        // ATOM_TYPE_DEX_OFFER (0x06)
uint160        addrFrom     // maker's HASH160 address
unsigned char  nSide        // ATOM_DEX_SIDE_SELL_ATOM (0x00)
int64          nAtomAmount  // ATOM amount in base units (1 ATOM = 1000000 units)
int64          nPrice       // BITOK satoshi per 1 ATOM base unit
uint32_t       nNonce       // per-sender monotonic counter
```

`IsValid()` requires: version matches, type matches, side == SELL_ATOM, amount > 0 and <= ATOM_MAX_SUPPLY, price > 0.

### `CAtomDexCancel` (atom.h)

```cpp
unsigned char  nVersion     // 0x01
unsigned char  nType        // ATOM_TYPE_DEX_CANCEL (0x07)
uint160        addrFrom     // maker address (must match original offer)
uint256        hashOffer    // txhash of DEX_OFFER being cancelled
```

### `CAtomDexTake` (atom.h)

```cpp
unsigned char  nVersion     // 0x01
unsigned char  nType        // ATOM_TYPE_DEX_TAKE (0x08)
uint160        addrFrom     // taker address
uint256        hashOffer    // txhash of DEX_OFFER being taken
```

### `CDexOrderEntry` (atom.h)

Persistent order book entry, stored in BerkeleyDB and loaded into memory.

```cpp
uint256        hashTx       // txhash of the DEX_OFFER transaction
uint160        addrMaker    // maker's HASH160 address
unsigned char  nSide        // 0x00 (SELL_ATOM)
int64          nAtomAmount  // ATOM escrow amount in base units
int64          nPrice       // BITOK price per ATOM base unit
uint32_t       nNonce       // nonce from the original offer
int            nHeight      // block height where offer was confirmed
unsigned int   nTime        // block timestamp
```

---

## Price and Payment Calculation

### Units

- ATOM amounts: base units where **1 ATOM = 1,000,000 base units** (`ATOM_DECIMALS`)
- BITOK amounts: satoshis where **1 BITOK = 100,000,000 satoshis** (`COIN`)
- `nPrice` in an order = BITOK satoshis per 1 ATOM **base unit**

Example: user specifies price "0.05 BITOK per ATOM"
- `nPrice = round(0.05 × 100000000) = 5000000` satoshis per ATOM base unit

### `DexComputeBitokPayment` (atom.h inline)

```cpp
inline int64 DexComputeBitokPayment(int64 nAtomAmount, int64 nPrice)
{
    __int128 n = (__int128)nAtomAmount * (__int128)nPrice;
    int64 result = (int64)(n / ATOM_DECIMALS);
    if (result < 0) result = 0;
    return result;
}
```

Uses 128-bit intermediate multiplication to prevent overflow. Returns 0 if result is negative (safety clamp).

Example: sell 100 ATOM at 0.05 BITOK/ATOM
- `nAtomAmount = 100 × 1000000 = 100000000`
- `nPrice = 5000000`
- `payment = (100000000 × 5000000) / 1000000 = 500000000 satoshis = 5.0 BITOK`

---

## Global State

### Persistent Order Book

```cpp
std::map<uint256, CDexOrderEntry> mapDexOrders  // offer txhash -> order entry
CCriticalSection cs_mapDexOrders
```

Loaded from BerkeleyDB at startup. Updated by `ConnectBlock`/`DisconnectBlock`.

### Mempool Tracking (cs_mapDexOrders)

```cpp
std::map<uint256, CAtomDexOffer>  mapDexMempoolOffers   // offer txhash -> offer
std::set<uint256>                 setDexMempoolCancels  // offer txhashes with pending cancel
std::set<uint256>                 setDexMempoolTakes    // offer txhashes with pending take
std::map<uint256, uint256>        mapDexMempoolCancelTx // cancel txhash -> offer txhash
std::map<uint256, uint256>        mapDexMempoolTakeTx   // take txhash -> offer txhash
```

### Nonce Tracking (cs_mapAtomMempool, shared with ATOM transfers)

```cpp
std::map<uint160, uint32_t> mapAtomMempoolNonce  // address -> highest pending nonce
```

DEX offers and ATOM transfers share the same nonce space per address. `DexMempoolAddOffer` updates this map; `DexMempoolRemove` recalculates it from remaining pending txs of both types.

---

## Nonce System

Each ATOM address has a per-sender monotonic counter stored on-chain. Every `DEX_OFFER` (and ATOM transfer) must include a nonce that is:

1. Strictly greater than the current confirmed on-chain nonce
2. No more than `ATOM_MAX_NONCE_GAP` (100) above the confirmed nonce
3. Not duplicated by another pending mempool transaction from the same address

`ConnectBlock` updates the confirmed nonce when a DEX_OFFER is mined (sets it to `dexOffer.nNonce`). `DEX_CANCEL` and `DEX_TAKE` do **not** modify the nonce.

`DisconnectBlock` rolls back the nonce when a `DEX_OFFER` is disconnected: if `nNonce == dexOffer.nNonce` and `nNonce > 0`, decrement by 1.

---

## Sender Verification

`VerifyAtomSender(tx, addrFrom)` is called for all DEX transaction types. It extracts the public key from each input's `scriptSig`, computes its `HASH160`, and checks that at least one matches `addrFrom`. This prevents address spoofing — a transaction cannot claim to be from an address unless it has a valid signature for that address.

Coinbase transactions always fail this check.

---

## Transaction Flow: Sell Order

1. Maker calls `dexsell <atom_amount> <price>`
2. Client picks wallet address with highest spendable ATOM
3. Nonce selected: `nNextNonce = max(confirmedNonce+1, highestPendingNonce+1)`
4. `CAtomDexOffer` built and encoded into 62-byte OP_RETURN payload
5. Transaction created: one output to maker's address (dust), one OP_RETURN output
6. Broadcast to network
7. When mined: `ConnectBlock` debits `nAtomAmount` from maker, writes order to DB, inserts into `mapDexOrders`

## Transaction Flow: Cancel Order

1. Maker calls `dexcancel <offer_txid>`
2. Client looks up order in `mapDexOrders`, verifies ownership
3. `CAtomDexCancel` encoded into 58-byte OP_RETURN
4. Transaction broadcast
5. When mined: `ConnectBlock` credits back `nAtomAmount` to maker, erases order from DB and memory

## Transaction Flow: Buy ATOM

1. Buyer calls `dexbuy <atom_amount> [max_price]`
2. Client scans `mapDexOrders` for cheapest matching sell order:
   - Filters by `nSide == SELL_ATOM`
   - Filters by `nPrice <= max_price` (if specified)
   - Filters by `nAtomAmount >= atom_amount` (if specified)
   - Skips orders with pending takes or cancels in mempool
   - Selects cheapest; at equal price, prefers smallest amount
3. Computes `nBitokPayment = DexComputeBitokPayment(order.nAtomAmount, order.nPrice)`
4. `CAtomDexTake` encoded into 58-byte OP_RETURN
5. Transaction created:
   - One output to **maker's address** with `nBitokPayment` satoshis (the BITOK payment)
   - One OP_RETURN output with DEX_TAKE payload
6. Broadcast
7. When mined: `ConnectBlock` validates BITOK payment, credits taker with ATOM, erases order

---

## ConnectBlock Processing

Runs when a block is added to the best chain. Only active at or above `ATOM_DEX_ACTIVATION_HEIGHT`.

### DEX_OFFER

```
1. VerifyAtomSender — sender must control addrFrom
2. Reject if nSide != SELL_ATOM
3. Reject if DexComputeBitokPayment(amount, price) <= 0
4. Read maker's confirmed balance and nonce
5. Reject if nNonce <= confirmedNonce (nonce too low)
6. Reject if nNonce > confirmedNonce + 100 (nonce gap too large)
7. Reject if balance < nAtomAmount (insufficient ATOM)
8. Debit: balance -= nAtomAmount
9. Update: confirmedNonce = nNonce
10. Write balance + nonce to DB
11. Build CDexOrderEntry { hashTx, addrMaker, nSide, nAtomAmount, nPrice, nNonce, nHeight=pindex->nHeight, nTime=pindex->nTime }
12. WriteDexOrder to DB
13. Insert into mapDexOrders
14. DexMempoolRemove(txhash)
```

### DEX_CANCEL

```
1. VerifyAtomSender
2. Find order in mapDexOrders by dexCancel.hashOffer — MUST exist or reject block
3. Verify addrMaker == dexCancel.addrFrom
4. Read maker's balance
5. Credit: balance += nAtomAmount (return escrowed ATOM)
6. Write balance to DB
7. EraseDexOrder from DB
8. Erase from mapDexOrders
9. DexMempoolRemove(txhash)
```

### DEX_TAKE

```
1. VerifyAtomSender
2. Find order in mapDexOrders — MUST exist or reject block
3. Reject if taker == maker
4. Compute nRequiredBitok = DexComputeBitokPayment(order.nAtomAmount, order.nPrice)
5. Reject if nRequiredBitok <= 0
6. Sum all tx outputs where output address == order.addrMaker
7. Reject if sum < nRequiredBitok (insufficient BITOK payment)
8. Read taker's balance
9. Credit: takerBalance += nAtomAmount (atomic ATOM delivery)
10. Write taker's balance to DB (nonce unchanged)
11. EraseDexOrder from DB
12. Erase from mapDexOrders
13. DexMempoolRemove(txhash)
```

---

## DisconnectBlock Processing

Runs when a block is removed from the best chain (reorg). Reverses all DEX state changes.

### Reversal: DEX_OFFER

```
1. Read maker's balance and nonce
2. Credit: balance += nAtomAmount (return escrowed ATOM)
3. If confirmedNonce == offer.nNonce and nNonce > 0: decrement confirmedNonce by 1
4. Write balance + nonce to DB
5. EraseDexOrder from DB
6. Erase from mapDexOrders
```

### Reversal: DEX_CANCEL

```
1. Read original offer tx from disk (by hashOffer)
2. Parse DEX_OFFER from that tx
3. Read maker's balance
4. Debit: balance -= nAtomAmount (re-escrow returned ATOM)
5. Recreate CDexOrderEntry from original offer fields
6. Recover block height: ReadTxIndex → ReadFromDisk → mapBlockIndex lookup
7. WriteDexOrder to DB
8. Insert into mapDexOrders
```

### Reversal: DEX_TAKE

```
1. Read original offer tx from disk (by hashOffer)
2. Parse DEX_OFFER
3. Read taker's balance
4. Debit: balance -= nAtomAmount (remove ATOM given to taker)
5. Recreate CDexOrderEntry from original offer fields
6. Recover block height (same as cancel reversal)
7. WriteDexOrder to DB
8. Insert into mapDexOrders
```

Note: BITOK already in the maker's wallet after a take; no reversal needed for BITOK (handled by standard UTXO disconnection).

---

## AcceptTransaction Validation

Mempool pre-validation. Only runs if `nBestHeight + 1 >= ATOM_DEX_ACTIVATION_HEIGHT` and the transaction does not also contain an ATOM transfer.

### DEX_OFFER checks

1. `VerifyAtomSender`
2. `nSide == SELL_ATOM` — BUY_ATOM payloads rejected
3. `AtomMempoolCheckNonce(addrFrom, nNonce, confirmedNonce)` — checks nonce validity and uniqueness across both ATOM transfers and DEX offers in mempool
4. `AtomGetEffectiveBalance(addrFrom, confirmedBalance)` — confirmed balance minus all pending transfers and DEX sell offers
5. Reject if effective balance < nAtomAmount

### DEX_CANCEL checks (all within single `cs_mapDexOrders` lock)

1. `VerifyAtomSender`
2. Reject if offer already in `setDexMempoolCancels` (duplicate cancel)
3. Reject if offer already in `setDexMempoolTakes` (take pending)
4. Reject if offer not in `mapDexOrders`
5. Reject if `addrMaker != dexCancel.addrFrom`

### DEX_TAKE checks (all within single `cs_mapDexOrders` lock)

1. `VerifyAtomSender`
2. Reject if offer already in `setDexMempoolTakes` (duplicate take)
3. Reject if offer already in `setDexMempoolCancels` (cancel pending)
4. Reject if offer not in `mapDexOrders`
5. Reject if taker == maker

Checks 2–5 run within a single lock acquisition to prevent TOCTOU races.

---

## Database Persistence

Uses BerkeleyDB through `CTxDB`. DEX orders use key prefix `"dexorder"`.

| Method | Operation |
|---|---|
| `WriteDexOrder(txhash, entry)` | Store order: key = ("dexorder", txhash) |
| `ReadDexOrder(txhash, entry)` | Read single order |
| `EraseDexOrder(txhash)` | Delete order |
| `LoadAllDexOrders(map)` | Cursor scan of all "dexorder" entries at startup |

### Startup Loading (init.cpp)

```cpp
CTxDB txdb("r");
if (txdb.LoadAllDexOrders(orders))
{
    CRITICAL_BLOCK(cs_mapDexOrders)
        mapDexOrders = orders;
    printf("[DEX] Loaded %u open orders from database\n", orders.size());
}
```

---

## RPC Commands

### `dexsell <atom_amount> <price>`

Place a sell order. Your ATOM is locked on-chain until the order is taken or cancelled.

**Parameters:**
- `atom_amount` — ATOM quantity (decimal, e.g. `100.5`)
- `price` — Ask price in BITOK per 1 ATOM (decimal, e.g. `0.05`)

**Behavior:**
1. Finds wallet address with highest spendable ATOM balance
2. Validates atom_amount <= spendable (confirmed balance minus pending transfers and DEX escrows)
3. Rejects if `DexComputeBitokPayment(amount, price) <= 0` (order too small)
4. Selects next nonce: `max(confirmedNonce+1, pendingMax+1)`
5. Broadcasts `DEX_OFFER` transaction

**Returns:**
```json
{
  "txid":        "abc123...",
  "from":        "Bxxx...",
  "atom_amount": 100.5,
  "price":       0.05,
  "total_bitok": 5.025,
  "nonce":       7,
  "status":      "pending"
}
```

---

### `dexcancel <offer_txid>`

Cancel an open sell order. Your escrowed ATOM will be returned.

**Parameters:**
- `offer_txid` — Transaction ID of the `DEX_OFFER` to cancel

**Behavior:**
1. Looks up offer in `mapDexOrders`
2. Verifies the calling wallet owns the order (addrMaker must match a wallet key)
3. Broadcasts `DEX_CANCEL` transaction

**Returns:**
```json
{
  "txid":      "abc123...",
  "cancelled": "offer_txid...",
  "status":    "pending"
}
```

---

### `dexbuy <atom_amount> [max_price]`

Buy ATOM from the cheapest available sell order.

**Parameters:**
- `atom_amount` — Minimum ATOM to buy. Set to `0` to fill the cheapest order entirely. If > 0, only considers orders with at least this amount.
- `max_price` *(optional)* — Maximum price in BITOK per ATOM. Orders above this are ignored.

**Order selection:**
- Finds the cheapest order (lowest `nPrice`) matching all filters
- Skips orders with a pending take or cancel in the mempool
- At equal price, prefers the order with the smallest `nAtomAmount`

**Behavior:**
1. Selects best matching sell order
2. Computes BITOK payment
3. Verifies wallet has sufficient BITOK
4. Broadcasts `DEX_TAKE` transaction with BITOK output to maker's address

**Returns:**
```json
{
  "txid":          "abc123...",
  "offer_taken":   "offer_txid...",
  "taker":         "Bxxx...",
  "atom_received": 100.5,
  "bitok_paid":    5.025,
  "price":         0.05,
  "status":        "pending"
}
```

---

### `dextake <offer_txid>`

Take a specific order by its transaction ID. Identical to `dexbuy` but targets a specific offer.

**Parameters:**
- `offer_txid` — Transaction ID of the `DEX_OFFER` to fill

**Returns:** Same structure as `dexbuy`.

---

### `dexorderbook`

List all open sell orders, sorted by price ascending (cheapest first).

**Parameters:** None

**Returns:**
```json
{
  "orders": [
    {
      "txid":        "abc123...",
      "seller":      "Bxxx...",
      "atom_amount": 100.0,
      "price":       0.05,
      "total_bitok": 5.0,
      "height":      25100
    }
  ],
  "total_count": 1
}
```

---

### `dexmyorders`

List your own open sell orders.

**Parameters:** None

**Returns:** Array of order objects (same fields as `dexorderbook` entries, with `address` instead of `seller`):
```json
[
  {
    "txid":        "abc123...",
    "address":     "Bxxx...",
    "atom_amount": 100.0,
    "price":       0.05,
    "total_bitok": 5.0,
    "height":      25100
  }
]
```

---

### `dexinfo`

DEX statistics and current parameters.

**Parameters:** None

**Returns:**
```json
{
  "activation_height":  25000,
  "open_orders":        42,
  "total_atom":         12345.5,
  "total_bitok_value":  617.275,
  "cheapest_ask":       0.045,
  "pending_sells":      3,
  "pending_cancels":    1,
  "pending_buys":       2
}
```

- `cheapest_ask` — lowest price in active order book, or `null` if no orders
- `pending_sells` — DEX_OFFER transactions in mempool (unconfirmed)
- `pending_cancels` — DEX_CANCEL transactions in mempool
- `pending_buys` — DEX_TAKE transactions in mempool

---

## Security Properties

### Atomic settlement

ATOM delivery and BITOK payment are validated in the same `ConnectBlock` call for the same transaction. There is no state where one side settles without the other.

### Escrow enforcement

ATOM is debited from the maker's account when `DEX_OFFER` is mined, not when broadcast. This prevents double-spending ATOM across multiple pending offers.

### BITOK payment enforcement

`ConnectBlock` scans the DEX_TAKE transaction's outputs and sums all satoshis sent to `order.addrMaker`. If this sum is less than `DexComputeBitokPayment(amount, price)`, the entire block is rejected.

### Sender authentication

Every DEX payload includes `addrFrom`. `VerifyAtomSender` checks that at least one input in the transaction is signed by the key whose `HASH160` matches `addrFrom`. This is checked both at mempool acceptance and at block connection.

### Overflow prevention

`DexComputeBitokPayment` uses `__int128` for intermediate multiplication, preventing 64-bit overflow for any combination of valid `nAtomAmount` and `nPrice` values.

### Zero-payment prevention

Both `ConnectBlock` (at offer creation and take) and `dexsell` RPC reject offers where `DexComputeBitokPayment` returns zero.

### Mempool deduplication

`setDexMempoolTakes` and `setDexMempoolCancels` track which offers have pending operations. Duplicate takes and cancels are rejected at mempool acceptance. These checks run inside a single `cs_mapDexOrders` critical section to prevent TOCTOU races.

### Reorg safety

`DisconnectBlock` fully reverses all ATOM balance changes and order book state. Block heights of restored orders are recovered by reading the original offer block from disk and looking up its index in `mapBlockIndex`.

---

## Effective Balance Calculation

`AtomGetEffectiveBalance(addr, confirmedBalance)` returns the spendable ATOM balance, accounting for pending outbound transactions in the mempool:

```
effectiveBalance = confirmedBalance
                 - sum(pending ATOM transfers from addr)
                 - sum(pending DEX sell escrows from addr)
```

This ensures a user cannot create more DEX offers than their balance supports, even if earlier offers are still unconfirmed.

---

## Nonce Flow Example

Suppose address A has confirmed nonce = 5:

1. A sends ATOM transfer with nonce 6 → mempool; `mapAtomMempoolNonce[A] = 6`
2. A calls `dexsell` → nonce 7 selected; `mapAtomMempoolNonce[A] = 7`
3. Block mines both txs → confirmed nonce becomes 7
4. Both txs removed from mempool; `mapAtomMempoolNonce[A]` erased
5. Next tx for A must use nonce >= 8

---

## Related Documentation

- `docs/ATOM.md` — ATOM token layer overview
- `docs/BITOKPOW.md` — Proof of work details
- `docs/RPC_API.md` — Full RPC reference
