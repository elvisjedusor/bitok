# ATOM — Continuing Satoshi's Vision

## The Origin: Satoshi's Atoms and Trust

In November 2008, weeks before Bitcoin's public launch, Satoshi Nakamoto drafted a pre-release version of the Bitcoin source code that never reached public release. Researcher Francis Pouliot discovered this code in 2019. It contained several notable concepts including the **"atoms"** tied to **"user address"**, widely interpreted as a node reputation or trust scoring system

Satoshi built trust-minimisation into every layer of early Bitcoin.

## The Gap Satoshi Left

Satoshi's original atoms were tied to addresses permanently. In Bitcoin's original privacy model (including the latest 0.3.19 version that Bitok is based on), a miner generates a fresh address for every block reward. This is intentional: fresh addresses preserve privacy by breaking the linkage between coins.

But this creates a fundamental tension with any trust or reputation system built on addresses:

- Each new block is mined to a new address
- Atoms (trust units) accumulate per address, not per identity
- A miner who mines 1,000 blocks across 1,000 fresh addresses holds 1,000 isolated atom balances
- No single address grows in trust — it is scattered and effectively lost

**A miner cannot accumulate atoms and build trust because each block reward goes to a new address.** Satoshi's design, brilliant for privacy, made the atoms concept unworkable in its original non-transferable form.

## My Fix: Transferable Atoms

I am continuing Satoshi's work by solving this gap. ATOM on Bitok is the completion of what Satoshi sketched but left unfinished.

The fix is precise:

- Atoms are **transferable** between addresses
- A miner collecting rewards across many fresh addresses can consolidate atoms into a single identity address
- Trust accumulates on that identity over time
- Privacy is preserved — consolidation is an explicit opt-in action, not automatic linkage

This is the minimum change needed to make the concept viable. The rest of the design follows Satoshi's principles directly.

---

## ATOM — The power of trust.

ATOM is a native token layer built directly on the Bitok blockchain. Every ATOM transfer is a regular Bitok transaction. ATOM state is maintained by every full node unconditionally — no special mode required.

ATOM holders act as trusted counterparties in the Bitok marketplace, P2P trading, and all Bitok ecosystem apps. When any deal or trade is completed through the platform, a percentage of the transactions flows to ATOM holders proportional to their stake — earning yield simply by holding trust. The more atoms an address accumulates, the more trusted it is, and the greater share of fees it earns.

No intermediary. No custodian. Trust built on-chain, rewarded on-chain.

---

## Table of Contents

1. [Token Parameters](#token-parameters)
2. [OP_RETURN Payload Format](#op_return-payload-format)
3. [Transaction Types](#transaction-types)
4. [How a Transfer Works](#how-a-transfer-works)
5. [Nonce System](#nonce-system)
6. [Balance Model](#balance-model)
7. [Bridge — Solana Integration](#bridge--solana-integration)
8. [State Indexer](#state-indexer)
9. [Reorg Safety](#reorg-safety)
10. [RPC API Reference](#rpc-api-reference)
11. [Implementation Details](#implementation-details)
12. [Security Properties](#security-properties)
13. [Limitations](#limitations)

---

## Token Parameters

| Parameter | Value | Source |
|-----------|-------|--------|
| Name | ATOM | `atom.h` |
| Magic bytes | `0x41 0x54 0x4F 0x4D` ("ATOM") | `ATOM_MAGIC[4]` |
| Protocol version | `0x01` | `ATOM_VERSION` |
| Maximum supply | 1,000,000,000.000000 ATOM | `ATOM_MAX_SUPPLY` |
| Maximum supply (raw) | `1,000,000,000,000,000` base units | `ATOM_MAX_SUPPLY = 1000000000LL * ATOM_DECIMALS` |
| Decimal places | 6 | `ATOM_DECIMALS_INT = 6` |
| Base unit | 1 ATOM = 1,000,000 base units | `ATOM_DECIMALS = 1000000LL` |
| Payload size (TRANSFER/COINBASE/BRIDGE_TO_BITOK) | 58 bytes | `ATOM_PAYLOAD_SIZE = 58` |
| Payload size (BRIDGE_TO_SOL) | 70 bytes | `ATOM_BRIDGE_PAYLOAD_SIZE = 70` |
| Coinbase reward | 20 ATOM per 1 BITOK of block subsidy | `ATOM_PER_BITOK = 20LL * ATOM_DECIMALS` |
| Activation height | 25,000 | `ATOM_ACTIVATION_HEIGHT = 25000` |
| Max nonce gap | 100 | `ATOM_MAX_NONCE_GAP = 100` |

**Supply math**: At 50 BITOK initial block reward, each block mints 1,000 ATOM coinbase. Halving every 210,000 blocks, the geometric series sums to 1,000 × 210,000 × 2 = **420,000,000 ATOM** from coinbase over all time. The remaining ~580,000,000 ATOM of the 1 billion cap is reserved for bridge minting (Solana ↔ Bitok).

**Activation**: ATOM processing (coinbase rewards, transfers, bridge operations) is completely inactive below block height 25,000. Blocks before this height are ignored by all ATOM code paths. This provides a clean activation window for miners to upgrade.

---

## OP_RETURN Payload Format

ATOM operations are encoded as a blob pushed after `OP_RETURN` in a transaction output. The output value is always 0 (unspendable by definition). The payload size depends on the transaction type.

```
scriptPubKey (vout):  OP_RETURN <ATOM payload>
```

The node scans all outputs looking for the first `OP_RETURN` that matches the ATOM magic bytes. All non-matching outputs are silently ignored. Parsed by `ParseAtomScript()` in `atom.cpp`.

### Standard format — TRANSFER / COINBASE / BRIDGE_TO_BITOK (58 bytes)

```
Offset  Length  Field       Description
------  ------  ----------  -------------------------------------------
0       4       magic       0x41 0x54 0x4F 0x4D  ("ATOM" in ASCII)
4       1       version     0x01
5       1       type        0x01=transfer  0x05=bridge_to_bitok
                            (0x04=coinbase is internal-only; rejected in user transactions)
6       20      addrFrom    HASH160 of sender public key (big-endian)
26      20      addrTo      HASH160 of recipient public key (big-endian)
46      8       amount      Base units, big-endian int64
54      4       nonce       Per-sender counter, big-endian uint32
------  ------  ----------  -------------------------------------------
Total   58
```

### Extended format — BRIDGE_TO_SOL (70 bytes)

The `addrTo` field is replaced with the 32-byte Solana destination public key. No `addrTo` (Bitok address) is present in this format.

```
Offset  Length  Field       Description
------  ------  ----------  -------------------------------------------
0       4       magic       0x41 0x54 0x4F 0x4D  ("ATOM" in ASCII)
4       1       version     0x01
5       1       type        0x03
6       20      addrFrom    HASH160 of sender public key (big-endian)
26      32      sol_to      Solana destination pubkey (32 raw bytes, big-endian)
58      8       amount      Base units, big-endian int64
66      4       nonce       Per-sender counter, big-endian uint32
------  ------  ----------  -------------------------------------------
Total   70
```

All multi-byte integers are **big-endian**. Bitok addresses are 20-byte RIPEMD-160(SHA-256(pubkey)) hashes. Solana addresses are stored as raw 32-byte Ed25519 public keys in the payload; at the RPC layer they are represented in standard Base58 format (the same format the Solana ecosystem uses).

**Validity Rules (enforced at decode time in `DecodeAtomPayload`):**

- Magic bytes == `0x41 0x54 0x4F 0x4D`
- Version == `0x01`
- Type is one of `0x01` (TRANSFER), `0x03` (BRIDGE_TO_SOL), or `0x05` (BRIDGE_TO_BITOK)
- For BRIDGE_TO_SOL: payload size >= 70 bytes; `sol_to` must not be all zeros
- For all other types: payload size >= 58 bytes
- Amount > 0
- Amount <= `ATOM_MAX_SUPPLY`

Any payload that fails any of these checks is silently rejected.

---

## Transaction Types

### 0x01 — TRANSFER

Moves ATOM from `addrFrom` to `addrTo`.

**Effect on state:**
- `addrFrom.balance -= amount`
- `addrTo.balance += amount`
- `addrFrom.nonce = max(confirmed_nonce, tx.nonce)`

**Validation:**
- Balance check: `addrFrom.confirmed_balance >= amount` (enforced in `ConnectBlock`)
- Mempool: spendable balance (confirmed - pending outbound) >= amount
- Nonce: `tx.nonce > addrFrom.confirmed_nonce` AND not already pending for this sender

### 0x03 — BRIDGE_TO_SOL

Burns ATOM on the Bitok chain and signals the bridge to mint equivalent SPL tokens on Solana. The Solana destination address (`sol_to`, 32 bytes) is embedded directly in the payload.

**Payload format:** 70-byte extended format (see above). The standard 20-byte `addrTo` field is absent; the 32-byte `sol_to` field takes its place.

**Effect on state:**
- `addrFrom.balance -= amount`
- No credit to any Bitok address
- `addrFrom.nonce = max(confirmed_nonce, tx.nonce)`

**Validation:** Same as TRANSFER (balance check + nonce), plus `sol_to` must not be all zeros.

**RPC:** Use `bridgeatomtosol <solana_address> <amount>` to create and broadcast a bridge transaction. The `solana_address` parameter is the standard Base58 Solana address (e.g. `6e5ZCp6V5ogM7o8bkJhcjzJSqJqbKMs4jAtSqDujDaxr`). All RPC outputs (`getatomtx`, `getatomhistory`, `listatomtransactions`) expose the Solana destination as `solana_destination` in Base58 format instead of `to`.

### 0x05 — BRIDGE_TO_BITOK

Mints new ATOM to `addrTo`. **Only valid when `addrFrom` == `ATOM_BRIDGE_HASH`.**

**Effect on state:**
- `addrTo.balance += amount`
- No balance check on any address (minting from bridge reserve)
- Nonce field present but not validated

**Enforcement:** Both `AcceptTransaction` (mempool) and `ConnectBlock` verify that `addrFrom` equals the 20-byte `ATOM_BRIDGE_HASH` constant defined in `atom.h`. Any BRIDGE_TO_BITOK from a non-bridge address is rejected.

Used when Solana-side ATOM tokens are locked on Solana and an equivalent amount is released on Bitok.

### 0x04 — COINBASE (internal only)

Credits ATOM to a miner's address as a block subsidy reward. **Never parsed from OP_RETURN scripts** — `DecodeAtomPayload` rejects type `0x04` from external payloads.

Generated directly by `ConnectBlock` from the block subsidy calculation:

```cpp
int64 nSubsidy = (50 * COIN) >> (pindex->nHeight / 210000);
int64 nAtomReward = (nSubsidy / COIN) * ATOM_PER_BITOK;
```

`addrFrom` is `uint160(0)`, nonce is `0`.

---

## How a Transfer Works

### User sends ATOM via `sendatom`

```
sendatom <toaddress> <amount>
```

The wallet draws from all wallet addresses with ATOM balance, largest-first. If one address covers the amount, one transaction is broadcast. Otherwise the wallet chains multiple transactions until the full amount is sent.

**Step by step:**

1. **Collect all sources**: Iterate all keys in `wallet.dat`. For each key read confirmed ATOM balance from DB and subtract pending outbound in mempool (`AtomGetEffectiveBalance`). Sort by spendable balance, largest-first.

2. **Check total**: If sum of spendable balances < `amount`, fail before any transaction is created.

3. **Loop through sources** until amount is fully covered:

   a. `nSend = min(address_spendable, remaining_amount)`

   b. Read confirmed state for this address: balance and nonce from DB.

   c. Compute next nonce:
      ```
      next_nonce = max(confirmed_nonce, highest_mempool_nonce_for_sender) + 1
      ```

   d. Encode: `BuildAtomScript(transfer)` → 58-byte OP_RETURN payload.

   e. Build Bitok transaction via `CreateStealthTransaction`:
      - Output sending `DUST_THRESHOLD` (0.01 BITOK) to `toaddress`
      - OP_RETURN output carrying the 58-byte ATOM payload (value = 0)
      - Change output if needed

   f. Sign and broadcast via `CommitTransaction`.

   g. Register in mempool index: `AtomMempoolAdd(txhash, transfer)`.

4. **Return**: single `txid` if one tx was enough; `txids` array with `tx_count` if multiple.

### Full web wallet workflow (createatomrawtx)

```
1. getnextatomnonce <fromaddress>          → confirmed_nonce, next_nonce
2. listunspent  OR  getaddressutxos        → select UTXOs covering dust + fee
3. createatomrawtx <from> <to> <amt> <nonce> [inputs]  → unsigned hex
4. signrawtransaction <hex> [] ["WIF"]     → signed hex, complete: true
5. sendrawtransaction <signed_hex>         → txid
```

---

## Nonce System

Every ATOM TRANSFER and BRIDGE_TO_SOL carries a `nonce` — a per-sender monotonic counter. BRIDGE_TO_BITOK has a nonce field in the payload but it is not validated.

### Purpose

1. **Replay prevention**: A confirmed nonce is stored in the DB. Any new transaction with `nonce <= confirmed_nonce` is rejected.
2. **Double-spend prevention**: Within the mempool, no two pending txs from the same sender may share a nonce.

### Nonce Lifecycle

| Phase | Action |
|-------|--------|
| New address | `confirmed_nonce = 0` (default for unknown key in DB) |
| Mempool entry | `nonce > confirmed_nonce` AND `nonce <= confirmed_nonce + ATOM_MAX_NONCE_GAP` AND `nonce` not already in `mapAtomMempoolTx` for this sender |
| `ConnectBlock` (TRANSFER/BRIDGE_TO_SOL) | `confirmed_nonce = max(confirmed_nonce, tx.nonce)` |
| `DisconnectBlock` (TRANSFER/BRIDGE_TO_SOL) | `if confirmed_nonce == tx.nonce and tx.nonce > 0: confirmed_nonce = tx.nonce - 1` |
| Node restart | Mempool cleared; `confirmed_nonce` restored from DB |

### Nonce Gaps

Small gaps are allowed up to `ATOM_MAX_NONCE_GAP` (100). If `confirmed_nonce = 5` and a tx with `nonce = 10` is broadcast, it is valid because the gap (5) is within the limit. Once mined, `confirmed_nonce` becomes 10. Nonces 6-9 are permanently retired.

A transaction with `nonce > confirmed_nonce + 100` is rejected by both the mempool and `ConnectBlock`. This prevents users from accidentally burning large ranges of nonces (e.g., sending nonce=1000000 when confirmed_nonce=5).

Always use `getnextatomnonce` -- it computes:
```
next_nonce = max(confirmed_nonce, highest_mempool_nonce_for_sender) + 1
```

### Nonce Rollback in DisconnectBlock

Transactions are processed in reverse block order (newest first):
```
if confirmed_nonce == tx.nonce and tx.nonce > 0:
    confirmed_nonce = tx.nonce - 1
```

If a block contains nonces 5 and 6 from the same sender, reversing nonce=6 first brings `confirmed_nonce` to 5, then reversing nonce=5 brings it to 4. The condition prevents over-rolling when multiple transfers from the same sender were in the same block.

---

## Balance Model

### Two Layers of State

**Layer 1 — Confirmed DB** (persistent, survives restarts):

Stored in `blkindex.dat` under key `("atombal", addr)`. Updated only by `ConnectBlock` and `DisconnectBlock`.

**Layer 2 — Mempool index** (in-memory, cleared on restart):

`mapAtomMempoolTx` in RAM. Updated by `AcceptTransaction` (via `AtomMempoolAdd`) and cleared per-tx when mined (via `AtomMempoolRemove`).

**Spendable balance:**
```
spendable = DB_confirmed - sum(mempool TRANSFER/BRIDGE_TO_SOL outbound from addr)
```

### DB Storage Schema

| Namespace | Key | Value | Purpose |
|-----------|-----|-------|---------|
| `atombal` | `(addr)` | `{int64 balance, uint32_t nonce}` | Mined balance and last confirmed nonce per address |
| `atomtx` | `(txhash)` | `{int height, unsigned int time, uchar type, uint160 from, uint160 to, int64 amount, uint32_t nonce, uchar[32] solAddrTo}` | Full record of each mined ATOM tx (solAddrTo populated for BRIDGE_TO_SOL, zeroed otherwise) |
| `atomaddrtx` | `(addr, txhash)` | `int height` | History index: all mined txs for an address (requires `-indexer`) |

`ReadAtomBalance` returns `{balance=0, nonce=0}` for any address not yet in the DB.

### Balance Invariants

- `confirmed_balance >= 0` always (clamped at write time)
- `AtomGetEffectiveBalance() >= 0` (clamped to 0 before return)
- Every accepted TRANSFER/BRIDGE_TO_SOL satisfies `confirmed - pending_outbound >= amount` at mempool acceptance

---

## Bridge — Solana Integration

ATOM exists on both Bitok (native L1) and Solana (wrapped SPL token). The bridge operator holds a special address (`ATOM_BRIDGE_HASH` in `atom.h`) that is the only address authorised to issue `ATOM_TYPE_BRIDGE_TO_BITOK` transactions.

### Bitok → Solana (wrapping)

1. User sends a **BRIDGE_TO_SOL** transaction from their Bitok address. In the 70-byte payload, `addrTo` is `uint160(0)` — only `sol_to` (the 32-byte Solana pubkey) carries the destination. A dust output is sent back to the sender's own address to satisfy the UTXO model.
2. Bridge monitors the Bitok chain for confirmed BRIDGE_TO_SOL transactions.
3. After the transaction confirms, the bridge mints equivalent SPL tokens on Solana to the user's Solana address.

### Solana → Bitok (unwrapping)

1. User burns their SPL tokens on Solana through the bridge contract.
2. Bridge detects the Solana burn, then broadcasts a **BRIDGE_TO_BITOK** transaction on Bitok from `ATOM_BRIDGE_HASH` crediting the user's Bitok address.
3. `ConnectBlock` verifies `addrFrom == ATOM_BRIDGE_HASH` before accepting the mint. Any BRIDGE_TO_BITOK from a non-bridge address is rejected and invalidates the block.

### Security

The bridge address check in both `AcceptTransaction` and `ConnectBlock` ensures that no node — honest or malicious — can mine a block containing an unauthorised mint.

```cpp
// atom.h — HASH160 of bridge operator key (address: 1BE3AJdLVeDJZ5mH8SSuXCqpnCswxK7DDp)
static const unsigned char ATOM_BRIDGE_HASH[20] = {
    0x70, 0x28, 0x78, 0x00, 0xce, 0x12, 0x3d, 0x72, 0x16, 0xb8,
    0x8d, 0x69, 0xe0, 0x8f, 0xa0, 0x1c, 0x32, 0x04, 0xce, 0xd2
};
```

The nonce is not enforced for BRIDGE_TO_BITOK, because mints are created by the bridge daemon and do not need replay protection beyond the balance enforcement already present for the Solana side.

---

## State Indexer

ATOM balance state is maintained by every node unconditionally — no flag required. The `-indexer` flag enables the additional address history index (used by `getatomhistory` and `getaddressatombalance`) as well as the coin address index.

- **`ConnectBlock`**: always updates confirmed balances in DB. With `-indexer` also writes `("atomaddrtx", addr, txhash)`.
- **`DisconnectBlock`**: always reverses ATOM balance changes. With `-indexer` also erases address history entries.
- **`AcceptTransaction`**: validates nonce and spendable balance before accepting any ATOM-carrying transaction.

### Automatic ATOM Rescan on Startup

On every startup, the node checks whether ATOM index data exists in the database (by reading `atomtotalminted`). If the key is missing — meaning the node was running before ATOM was introduced, or the database was rebuilt without ATOM data — the node automatically runs a full ATOM rescan before completing startup.

- **Daemon (`bitokd`)**: Logs progress to console in 10% increments (`[ATOM] RescanAtom progress: 10% (5000/50000 blocks, height 5000)`).
- **GUI (`bitok`)**: Displays a modal progress bar dialog ("Building ATOM Index") showing block-by-block progress with elapsed/estimated time.

No manual action is required. The rescan replays every block from genesis and rebuilds all ATOM state (`atombal`, `atomtx`, `atomaddrtx`, `atomtotalminted`).

### Manual Rebuild via rescanwallet

If ATOM state becomes corrupted, or you want to force a full rebuild:

```
rescanwallet
```

This rescans the blockchain for wallet transactions **and** erases and rebuilds all ATOM data from genesis. There is no separate `atomrescan` command — `rescanwallet` covers both wallet and ATOM state.

---

## Reorg Safety

When a reorg occurs, `DisconnectBlock` is called on each block being rolled back. For every ATOM transaction in the disconnected block, the exact inverse of `ConnectBlock` runs. Transactions are processed in reverse block order (newest tx first).

### Reversing a TRANSFER

```
sender.balance += amount
if sender.confirmed_nonce == tx.nonce and tx.nonce > 0:
    sender.confirmed_nonce = tx.nonce - 1
recipient.balance -= amount
recipient.balance = max(0, recipient.balance)
```

### Reversing a BRIDGE_TO_SOL

```
sender.balance += amount
if sender.confirmed_nonce == tx.nonce and tx.nonce > 0:
    sender.confirmed_nonce = tx.nonce - 1
```

### Reversing a BRIDGE_TO_BITOK

```
recipient.balance -= amount
recipient.balance = max(0, recipient.balance)
```

### Reversing a COINBASE

```
miner.balance -= amount
miner.balance = max(0, miner.balance)
```

The coinbase amount is re-derived from the block subsidy at disconnect time using `ATOM_PER_BITOK`. No nonce involved.

After reversing all balance changes, `EraseAtomTx(txhash)` removes the mined record.

---

## RPC API Reference

### Command Summary

| Command | Requires wallet | Requires `-indexer` | Purpose |
|---------|----------------|---------------------|---------|
| `getatominfo` | no | no | Protocol parameters and live mempool stats |
| `gettotalatomsupply` | no | no | Sum of all confirmed ATOM balances on-chain |
| `getatombalance` | **yes** | no | Total confirmed ATOM balance across all wallet addresses |
| `getaddressatombalance <address>` | no | **yes** | Confirmed ATOM balance for a specific address |
| `getnextatomnonce <address>` | no | no | Next safe nonce for an address |
| `getatomtx <txid>` | no | no | Look up single transfer by txid |
| `getatomhistory <address> [count]` | no | **yes** | Full history for any address |
| `listatomtransactions [count]` | **yes** | no | Wallet ATOM history (mempool + mined) |
| `sendatom <toaddress> <amount>` | **yes** | no | Send from wallet |
| `createatomrawtx <from> <to> <amount> <nonce> [inputs]` | no | no | Build unsigned raw ATOM tx |
| `bridgeatomtosol <solana_address> <amount>` | **yes** | no | Bridge ATOM from wallet to Solana |

ATOM state is rebuilt automatically on startup if missing. Use `rescanwallet` to force a manual rebuild (also rescans wallet transactions).

---

### `getatominfo`

Returns protocol-level ATOM parameters and the current pending transaction count.

```json
{
  "name": "ATOM",
  "version": 1,
  "decimals": 6,
  "decimals_unit": 1000000,
  "max_supply": 1000000000.0,
  "max_supply_raw": 1000000000000000,
  "payload_size": 58,
  "activation_height": 25000,
  "max_nonce_gap": 100,
  "finality": "confirmed",
  "pending_txs": 3
}
```

---

### `gettotalatomsupply`

Returns the total circulating ATOM supply — the sum of all confirmed ATOM balances stored in the database.

Takes no arguments. No wallet required. No `-indexer` required.

```json
{
  "total_supply":     42000000.500000,
  "total_supply_raw": 42000000500000,
  "max_supply":       1000000000.000000,
  "max_supply_raw":   1000000000000000,
  "decimals":         6
}
```

| Field | Meaning |
|-------|---------|
| `total_supply` | Circulating supply in ATOM units (6 decimal places) |
| `total_supply_raw` | Circulating supply in base units |
| `max_supply` | Protocol cap in ATOM units |
| `max_supply_raw` | Protocol cap in base units (`ATOM_MAX_SUPPLY`) |
| `decimals` | Decimal places (`ATOM_DECIMALS_INT = 6`) |

Only confirmed (mined) balances are counted. Pending mempool transfers do not affect the result.

---

### `getatombalance`

Returns the wallet's total confirmed ATOM balance — the sum of confirmed balances across all addresses in `wallet.dat`. Takes no arguments. Requires a wallet. No `-indexer` required.

```
95.5
```

Returns a plain number (double), not a JSON object. Pending amounts are not included.

---

### `getaddressatombalance <address>`

Returns the confirmed ATOM balance for a specific address. **Requires `-indexer`**.

```
95.5
```

Returns a plain number (double). Only confirmed (mined) balance returned.

---

### `getnextatomnonce <address>`

Returns the next valid ATOM nonce for an address. Accounts for both the confirmed DB nonce and any pending mempool transactions.

```json
{
  "address": "BitokXXX...",
  "confirmed_nonce": 7,
  "next_nonce": 8
}
```

| Field | Meaning |
|-------|---------|
| `confirmed_nonce` | Last nonce written to DB |
| `next_nonce` | `max(confirmed_nonce, highest_mempool_nonce) + 1` |

---

### `getatomtx <txid>`

Look up an ATOM transfer by its Bitok transaction ID. Checks the mempool first (status `"pending"`), then the confirmed DB (status `"mined"`).

**Pending (mempool) response:**

```json
{
  "txid": "a1b2c3d4...",
  "type": "transfer",
  "status": "pending",
  "from": "BitokXXX...",
  "to": "BitokYYY...",
  "amount": 1.5,
  "amount_raw": 1500000,
  "nonce": 8
}
```

**Mined response:**

```json
{
  "txid": "a1b2c3d4...",
  "type": "transfer",
  "status": "mined",
  "height": 100230,
  "time": 1710000000,
  "from": "BitokXXX...",
  "to": "BitokYYY...",
  "amount": 1.5,
  "amount_raw": 1500000,
  "nonce": 8
}
```

`height` and `time` are only present when `status` is `"mined"`. `type` is one of `"transfer"`, `"bridge_to_sol"`, `"bridge_to_bitok"`, or `"coinbase"`.

---

### `getatomhistory <address> [count=50]`

Returns ATOM transactions involving `address`, newest first. **Requires `-indexer`**. Mempool entries (status `"pending"`) appear before mined entries. Deduplication prevents a tx appearing in both sections simultaneously.

```json
[
  {
    "txid": "...",
    "type": "transfer",
    "status": "pending",
    "direction": "received",
    "from": "BitokXXX...",
    "to": "BitokYYY...",
    "amount": 10.0,
    "amount_raw": 10000000,
    "nonce": 1
  },
  {
    "txid": "...",
    "type": "transfer",
    "status": "mined",
    "direction": "sent",
    "height": 100230,
    "time": 1710000000,
    "from": "BitokYYY...",
    "to": "BitokZZZ...",
    "amount": 5.0,
    "amount_raw": 5000000,
    "nonce": 0
  }
]
```

| Field | Values |
|-------|--------|
| `direction` | `"sent"`, `"received"`, `"self"` (from == to == address), `"bridge_to_sol"`, `"bridge_to_bitok"`, or `"coinbase"` |
| `status` | `"pending"` (mempool) or `"mined"` |
| `height` | Block height (only present when `status` is `"mined"`) |
| `time` | Unix timestamp (only present when `status` is `"mined"`) |

---

### `listatomtransactions [count=50]`

Returns ATOM transactions involving any address in the local wallet, newest first. Does **not** require `-indexer`. Mempool entries appear first, then mined entries sorted by `nTimeReceived` descending.

```json
[
  {
    "txid": "a1b2c3d4...",
    "type": "transfer",
    "status": "pending",
    "direction": "received",
    "from": "BitokXXX...",
    "to": "BitokYYY...",
    "amount": 10.0,
    "amount_raw": 10000000,
    "nonce": 1
  },
  {
    "txid": "b2c3d4e5...",
    "type": "transfer",
    "status": "mined",
    "direction": "sent",
    "height": 100230,
    "time": 1710000000,
    "from": "BitokYYY...",
    "to": "BitokZZZ...",
    "amount": 5.0,
    "amount_raw": 5000000,
    "nonce": 2
  }
]
```

---

### `sendatom <toaddress> <amount>`

Send ATOM to `toaddress` using the local wallet. Funds are drawn from all wallet addresses with ATOM balance, sorted largest-first.

`amount` is in ATOM units (e.g. `1.5` = 1.5 ATOM = 1,500,000 base units).

**Single-address response:**

```json
{
  "txid": "a1b2c3d4...",
  "from": "BitokXXX...",
  "to": "BitokYYY...",
  "amount": 1.5,
  "amount_raw": 1500000,
  "status": "pending"
}
```

**Multi-address response:**

```json
{
  "txids": ["a1b2c3d4...", "b2c3d4e5..."],
  "to": "BitokYYY...",
  "amount": 300.0,
  "amount_raw": 300000000,
  "tx_count": 2,
  "status": "pending"
}
```

A small Bitok fee is required per transaction from the wallet's coin balance.

---

### `createatomrawtx <fromaddress> <toaddress> <amount> <nonce> [inputs]`

Builds an **unsigned** raw transaction carrying an ATOM transfer in its OP_RETURN output. Intended for web wallets and applications that manage keys outside of `wallet.dat`.

| Parameter | Description |
|-----------|-------------|
| `fromaddress` | Sender's Bitok address — embedded as HASH160 in the ATOM payload |
| `toaddress` | Recipient's Bitok address |
| `amount` | ATOM amount in ATOM units (e.g. `1.5` = 1.5 ATOM) |
| `nonce` | Per-sender monotonic counter — always obtain from `getnextatomnonce` |
| `inputs` | Optional array of coin UTXOs `[{"txid":"...","vout":n}]`. If omitted, the raw tx has no inputs. |

```json
{
  "hex": "0100000000...",
  "from": "BitokXXX...",
  "to": "BitokYYY...",
  "amount": 1.5,
  "amount_raw": 1500000,
  "nonce": 8,
  "dust_out": 1000000,
  "note": "Add coin inputs (listunspent), sign with signrawtransaction, broadcast with sendrawtransaction"
}
```

The returned transaction always has exactly two outputs:
1. A dust output (`DUST_THRESHOLD` = 0.01 BITOK) to `<toaddress>`
2. An `OP_RETURN` output carrying the 58-byte ATOM payload (value = 0)

Note: `createatomrawtx` only builds TRANSFER payloads. Use `bridgeatomtosol` for bridge transactions.

---

### `bridgeatomtosol <solana_address> <amount>`

Bridge ATOM from the local wallet to a Solana address. Burns ATOM on the Bitok chain and signals the bridge daemon to mint equivalent SPL tokens on Solana.

| Parameter | Description |
|-----------|-------------|
| `solana_address` | Solana address in Base58 format (e.g. `6e5ZCp6V5ogM7o8bkJhcjzJSqJqbKMs4jAtSqDujDaxr`) |
| `amount` | ATOM amount in ATOM units (e.g. `10.0` = 10 ATOM = 10,000,000 base units) |

**Logic**: Selects the wallet address with the largest confirmed spendable ATOM balance. Computes next nonce for that address. Builds a single 70-byte BRIDGE_TO_SOL payload. Broadcasts via `CommitTransaction`. Adds a dust output (`DUST_THRESHOLD` = 0.01 BITOK) back to the sender as the `OP_RETURN` output requires a spending vehicle.

```json
{
  "txid": "a1b2c3d4...",
  "from": "BitokXXX...",
  "solana_destination": "6e5ZCp6V5ogM7o8bkJhcjzJSqJqbKMs4jAtSqDujDaxr",
  "amount": 10.0,
  "amount_raw": 10000000,
  "status": "pending"
}
```

**Note**: `solana_destination` is always returned in Base58 format regardless of how you passed the address in. No `to` field — the ATOM is burned, not sent to any Bitok address. Once the on-chain confirmation is observed by the bridge daemon, SPL tokens are minted to that Solana address.

---

## Implementation Details

### Files

| File | Purpose |
|------|---------|
| `atom.h` | Constants, `CAtomTransfer` struct, `ATOM_BRIDGE_HASH`, all function declarations |
| `atom.cpp` | Payload encode/decode, script parsing/building, mempool index management |
| `script.h` | `SCRIPT_VERIFY_EXEC` flag; `EvalScript`/`VerifyScript`/`VerifySignature` declarations — no ATOM-specific opcodes |
| `script.cpp` | Standard script execution; no ATOM-specific opcodes |
| `bitcoin_db.h` | `CTxDB` ATOM method declarations |
| `bitcoin_db.cpp` | All `CTxDB` ATOM method implementations; `EraseAllAtomData` clears `atombal`, `atomtx`, `atomaddrtx` |
| `main.h` | `SCRIPT_EXEC_ACTIVATION_HEIGHT = 18000` |
| `main.cpp` | Two-pass `ConnectBlock`; `DisconnectBlock`; `AcceptTransaction`; `RescanAtom` |
| `headers.h` | `#include "atom.h"` added after `bitcoin_db.h` |
| `rpc.cpp` | 11 ATOM RPC command implementations |

### Key Constants (`atom.h`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `ATOM_MAGIC` | `{0x41, 0x54, 0x4F, 0x4D}` | ASCII "ATOM" |
| `ATOM_VERSION` | `0x01` | Current protocol version |
| `ATOM_TYPE_TRANSFER` | `0x01` | Move tokens |
| `ATOM_TYPE_BRIDGE_TO_SOL` | `0x03` | Bridge ATOM to Solana |
| `ATOM_TYPE_COINBASE` | `0x04` | Block reward minting; generated by `ConnectBlock` only |
| `ATOM_TYPE_BRIDGE_TO_BITOK` | `0x05` | Mint from bridge; only valid from `ATOM_BRIDGE_HASH` |
| `ATOM_PAYLOAD_SIZE` | `58` | Payload bytes for TRANSFER / COINBASE / BRIDGE_TO_BITOK |
| `ATOM_BRIDGE_PAYLOAD_SIZE` | `70` | Payload bytes for BRIDGE_TO_SOL (32-byte Solana destination) |
| `ATOM_DECIMALS_INT` | `6` | Decimal places |
| `ATOM_DECIMALS` | `1,000,000LL` | 1 ATOM = 1,000,000 base units |
| `ATOM_MAX_SUPPLY` | `1,000,000,000,000,000LL` | 1 billion ATOM in base units |
| `ATOM_PER_BITOK` | `20,000,000LL` | Raw ATOM per 1 BITOK of block subsidy (= 20 ATOM) |
| `ATOM_BRIDGE_HASH` | `[20 bytes]` | HASH160 of bridge operator key; **must be set before release** |

### `CAtomTransfer` Struct (`atom.h`)

```cpp
struct CAtomTransfer {
    unsigned char nVersion;      // ATOM_VERSION = 0x01
    unsigned char nType;         // ATOM_TYPE_TRANSFER/BRIDGE_TO_SOL/BRIDGE_TO_BITOK/COINBASE
    uint160       addrFrom;      // HASH160(sender pubkey), 20 bytes
    uint160       addrTo;        // HASH160(recipient pubkey), 20 bytes; zero for BRIDGE_TO_SOL
    unsigned char solAddrTo[32]; // Solana destination pubkey; only valid for BRIDGE_TO_SOL
    int64         nAmount;       // base units
    uint32_t      nNonce;        // per-sender monotonic counter
};
```

`IsValid()` accepts TRANSFER, BRIDGE_TO_SOL, and BRIDGE_TO_BITOK. `IsValidCoinbase()` is used internally by `ConnectBlock` for the coinbase reward record.

For BRIDGE_TO_SOL, `addrTo` is always `uint160(0)` — the Solana destination lives in `solAddrTo[32]`.

Has `IMPLEMENT_SERIALIZE` for BerkeleyDB serialisation (serialises `addrTo`; `solAddrTo` is payload-only).

### Atom Detection Functions (`atom.cpp`)

| Function | Description |
|----------|-------------|
| `ParseAtomScript(script, out)` | Parse OP_RETURN payload from a `CScript` |
| `BuildAtomScript(transfer)` | Build OP_RETURN `CScript` from a `CAtomTransfer` |
| `EncodeAtomPayload(transfer, vchOut)` | Encode `CAtomTransfer` to 58-byte (standard) or 70-byte (BRIDGE_TO_SOL) big-endian binary |
| `DecodeAtomPayload(vch, out)` | Decode binary to `CAtomTransfer`; validates magic, version, type, amount; uses extended 70-byte format for BRIDGE_TO_SOL |
| `GetAtomTransfer(tx, out)` | Scan all vout for OP_RETURN ATOM payload; returns first match |

### In-Memory Mempool Index (`atom.cpp`)

```
cs_mapAtomMempool           -- CCriticalSection guarding both maps
mapAtomMempoolTx            -- map<uint256, CAtomTransfer>   (txhash -> transfer)
mapAtomMempoolNonce         -- map<uint160, uint32_t>        (addr -> highest pending nonce)
```

| Function | Description |
|----------|-------------|
| `AtomMempoolAdd(txhash, transfer)` | Register a new pending ATOM transfer; update highest-nonce map |
| `AtomMempoolRemove(txhash)` | Remove when confirmed or evicted; recompute highest-nonce map for sender |
| `AtomMempoolCheckNonce(addr, nonce, confirmed_nonce)` | Returns true only if `nonce > confirmed_nonce` AND not already in mempool for this sender |
| `AtomMempoolPendingOutbound(addr)` | Sum of all pending TRANSFER/BRIDGE_TO_SOL outbound amounts for addr |
| `AtomGetEffectiveBalance(addr, confirmed)` | `confirmed - pending_outbound`, clamped to 0 |

### Database Layer (`bitcoin_db.cpp`)

| Method | Description |
|--------|-------------|
| `ReadAtomBalance(addr, bal, nonce)` | Read confirmed balance + nonce; returns 0/0 for unknown address |
| `WriteAtomBalance(addr, bal, nonce)` | Write confirmed balance + nonce atomically |
| `ReadAtomTx(txhash, ...)` | Read full mined ATOM tx record |
| `WriteAtomTx(txhash, ...)` | Write full mined ATOM tx record |
| `EraseAtomTx(txhash)` | Remove mined ATOM tx record (called from `DisconnectBlock`) |
| `WriteAtomAddrTx(addr, txhash, height)` | Write address history entry (requires `-indexer`) |
| `EraseAtomAddrTx(addr, txhash)` | Remove address history entry (called from `DisconnectBlock` with `-indexer`) |
| `ReadAtomAddrTxids(addr, vtxids)` | Cursor-based scan of all mined txids for an address |
| `SumAllAtomBalances(nTotalOut)` | Cursor scan of all `atombal` entries; sums all positive balances; used by `gettotalatomsupply` |
| `EraseAllAtomData()` | Wipe all three ATOM key prefixes: `atombal`, `atomtx`, `atomaddrtx` |

### ConnectBlock Pass-1 (`main.cpp`)

```cpp
if (GetAtomTransfer(tx, atomTransfer)) {
    // reject GENESIS, COINBASE
    // reject BRIDGE_TO_BITOK from non-bridge address
    // TRANSFER: balance check, debit sender, credit recipient
    // BRIDGE_TO_SOL: balance check, debit sender
    // BRIDGE_TO_BITOK: credit recipient (no balance check)
    txdb.WriteAtomBalance(...)
    txdb.WriteAtomTx(...)
    AtomMempoolRemove(txhash);
}
```

**Coinbase ATOM reward** (separate from OP_RETURN scan — runs at end of pass-2):
```cpp
int64 nSubsidy = (50 * COIN) >> (pindex->nHeight / 210000);
int64 nAtomReward = (nSubsidy / COIN) * ATOM_PER_BITOK;
// Credits miner address with ATOM_TYPE_COINBASE record
```

### DisconnectBlock (`main.cpp`)

Exactly inverts ConnectBlock for each type:
- TRANSFER: restore sender balance + nonce, debit recipient
- BRIDGE_TO_SOL: restore sender balance + nonce
- BRIDGE_TO_BITOK: debit recipient (clamped to 0)
- COINBASE: debit miner (clamped to 0)

---

## Security Properties

### No Double-Spend

Each ATOM TRANSFER and BRIDGE_TO_SOL uses a nonce. The node enforces:
1. `nonce > confirmed_nonce` (rejects replays of any confirmed tx)
2. `nonce <= confirmed_nonce + ATOM_MAX_NONCE_GAP` (prevents accidentally burning large nonce ranges)
3. `nonce` is unique among all pending mempool txs from the same sender

### No Overdraft

`sendatom` and `AcceptTransaction` both compute `spendable = confirmed - pending_outbound` per address before broadcasting or accepting any transaction. `ConnectBlock` performs a hard balance check before crediting any state change.

### Sender Verification (addrFrom Binding)

For TRANSFER and BRIDGE_TO_SOL transactions, the node verifies that at least one transaction input was signed by a key whose `Hash160` matches the ATOM payload's `addrFrom` field. This is enforced at both the mempool gate (`AcceptTransaction`) and block validation (`ConnectBlock`). The check uses `VerifyAtomSender()` in `atom.cpp`, which extracts the public key from each input's `scriptSig` and computes `Hash160(pubkey)`. A transaction where no input's signing key matches `addrFrom` is rejected.

This prevents ATOM theft: even if an attacker knows another user's address and ATOM balance, they cannot spend that user's ATOM without possessing the private key.

### Bridge Mint Authorization

Only `ATOM_BRIDGE_HASH` can issue `ATOM_TYPE_BRIDGE_TO_BITOK`. The check runs at both the mempool gate (`AcceptTransaction`) and at block validation (`ConnectBlock`). A block containing an unauthorized bridge mint is rejected.

### Supply Cap Enforcement

A running `atomtotalminted` counter is maintained in the database, tracking the cumulative ATOM created through coinbase rewards and bridge minting. Both `ConnectBlock` and `RescanAtom` enforce that `nTotalMinted + nNewMint <= ATOM_MAX_SUPPLY`. If coinbase reward would exceed the cap, it is clamped. If a bridge mint would exceed it, the block is rejected.

`DisconnectBlock` correctly decrements the counter when rolling back bridge mints or coinbase rewards.

### Activation Height

ATOM processing is gated behind `ATOM_ACTIVATION_HEIGHT = 25000`. Below this height:
- `ConnectBlock` does not process any ATOM payloads or credit coinbase ATOM
- `DisconnectBlock` does not reverse any ATOM state
- `AcceptTransaction` does not validate ATOM payloads
- `RescanAtom` skips blocks below the activation height

This prevents chain splits between old nodes (without ATOM) and new nodes during the upgrade window. Old nodes continue mining valid blocks because no ATOM validation occurs before height 25,000. Once the activation height is reached, all miners must be running ATOM-aware code.

### Reorg Safety

`DisconnectBlock` exactly inverts every `ConnectBlock` operation. Balance deltas are symmetric. Nonce rollback uses a conditional check to prevent over-rolling when multiple transfers from the same sender were in the same block. The `atomtotalminted` counter is correctly reversed for both bridge mints and coinbase rewards.

### Restart Recovery

On restart, the confirmed DB state is fully intact. The in-memory mempool index is cleared. `getnextatomnonce` uses the DB nonce as the floor. Pending txs must be rebroadcast.

---

## Limitations

### Mempool Volatility

ATOM transfers in the mempool can be evicted if the Bitok transaction pays too low a fee or the network partitions. `AtomMempoolRemove` cleans up the in-memory index; confirmed balances remain unchanged. No ATOM is lost — the transfer must be rebroadcast.

### Node Restart Clears Mempool

The ATOM mempool index is in-memory only. After a restart, confirmed DB state is intact but pending ATOM transfers must be rebroadcast.

### Indexer Optional

ATOM balances are maintained by all full nodes unconditionally. The `-indexer` flag is required only for address history (`getatomhistory`) and per-address balance queries (`getaddressatombalance`). `sendatom`, `getatombalance`, `listatomtransactions`, `getatomtx`, `getnextatomnonce`, and `createatomrawtx` all work without `-indexer`.

### Bridge Address

`ATOM_BRIDGE_HASH` in `atom.h` is set to the HASH160 of address `1BE3AJdLVeDJZ5mH8SSuXCqpnCswxK7DDp`. Only this address can issue BRIDGE_TO_BITOK mint transactions.
