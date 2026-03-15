# Bitok

Peer-to-peer electronic cash. Started from Bitcoin v0.3.19, the last release Satoshi ran.

---

Bitcoin changed. Not all of the changes were improvements.

The script engine got neutered. The priority system got removed. Mining moved to ASICs. Replace-by-fee broke zero-confirm payments. What is left is a settlement layer for large holders, not the thing Satoshi built.

Bitok finishes the parts that were left unfinished:

- Script engine with all original opcodes, running inside a bounded VM. OP_CAT, OP_MUL, OP_AND, OP_SUBSTR, all of them. Bitcoin Core disabled them. We made them safe instead.
- Priority-based fees, as Satoshi designed. Coins held a day move free. Fees exist to stop spam.
- Yespower proof-of-work. Memory-hard, CPU-friendly. A laptop can mine.
- SPV clients. Header sync, bloom filters, Merkle proofs. Section 8 of the whitepaper.
- BIP 22 mining RPC. Full getblocktemplate support for pools.

New genesis. Separate network. Same 21M cap, same halving, same 10-minute blocks.

---

## Quick Start

```bash
./bitokd                     # start node
./bitokd -gen                # start node and mine
./bitokd getinfo             # check status
./bitokd stop                # stop
```

```bash
./bitok                      # GUI wallet
```

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Proof-of-work | Yespower 1.0 (N=2048, r=32, "BitokPoW") |
| Block time | 10 minutes |
| Block reward | 50 BITOK, halving every 210,000 blocks |
| Max supply | 21,000,000 BITOK |
| Coinbase maturity | 100 blocks |
| P2P port | 18333 |
| RPC port | 8332 |
| Address prefix | 1 (same encoding as Bitcoin) |
| Genesis hash | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` |
| Script exec active | block 18,000 |
| Time warp protection | block 16,000 |
| Network magic | `b4 0b c0 de` |

---

## Script Engine

All original opcodes run. No exceptions.

| Limit | Value |
|-------|-------|
| Max script size | 10,000 bytes |
| Max stack depth | 1,000 items |
| Max element size | 520 bytes |
| Max opcodes per script | 201 |
| Max sigops per block | 20,000 |
| Max multisig keys | 20 |

What works:

- **OP_CAT** -- concatenation, covenant patterns
- **OP_MUL, OP_DIV, OP_MOD** -- on-chain arithmetic
- **OP_LSHIFT, OP_RSHIFT** -- bit shifts (up to 31)
- **OP_AND, OP_OR, OP_XOR, OP_INVERT** -- bitwise logic
- **OP_SUBSTR, OP_LEFT, OP_RIGHT** -- string slicing
- **OP_CHECKMULTISIG** -- bare m-of-n, up to 20 keys
- **All sighash types** -- ALL, NONE, SINGLE, each with ANYONECANPAY
- **OP_RETURN** -- provably unspendable (original was bugged, fixed)
- Any well-formed script up to 10KB is standard and relays

What is not in Bitok:

- No P2SH
- No SegWit, no witness data
- No CLTV, no CSV (use nLockTime)
- No Taproot, no Schnorr
- No Schnorr multisig

Signatures are strict DER with low-S. Scripts use separated evaluation. Minimal push encoding. NULLDUMMY enforcement on CHECKMULTISIG.

---

## Fee Policy

```
priority = sum(input_value * confirmations) / tx_size
```

- Threshold: 57,600,000 (1 BITOK confirmed 1 day in 250-byte tx)
- First 27KB of each block: reserved for high-priority free transactions
- Rest of block: sorted by fee-per-byte
- When fee required: 0.01 BITOK per KB
- Dust threshold: 0.01 BITOK (outputs below this always need a fee)

The wallet handles fee calculation automatically.

---

## Address Indexer

Optional per-address UTXO and history index for exchanges, dApps, block explorers.

```ini
indexer=1
```

Auto-syncs on startup. RPC: `getaddressbalance`, `getaddressutxos`, `getaddresstxids`, `getindexerinfo`.

---

## Configuration

| OS | Data directory |
|----|----------------|
| Linux | `~/.bitokd/` |
| macOS | `~/Library/Application Support/Bitok/` |
| Windows | `%APPDATA%\Bitok\` |

`bitok.conf` example:

```ini
server=1
rpcuser=user
rpcpassword=pass
gen=0
addnode=seed1.bitokd.run
```

For web clients:

```ini
cors=1
corsorigin=http://localhost:3000
```

---

## Building

### Ubuntu 24.04 (daemon)

```bash
sudo apt-get install build-essential libssl-dev libdb5.3-dev libboost-all-dev
make -f makefile.unix
```

### Ubuntu 24.04 (with GUI)

```bash
sudo apt-get install libwxgtk3.2-dev libgtk-3-dev
make -f makefile.unix gui
```

Full build instructions for each platform are in docs/.

---

## DNS Seeds

```
seed1.bitokd.run
seed2.bitokd.run
seed3.bitokd.run
```

Manual peer:

```bash
./bitokd -addnode=1.2.3.4
```

---

## Security

Included from launch:

- Value overflow protection
- Blockchain checkpoints (0, 6666, 14000, 16000)
- Connection and message size rate limits

Added:

- Time warp protection (soft-fork, block 16,000)
- DNS peer discovery with hardcoded fallback
- External IP learned from peers, no HTTP calls
- Anchor connections for eclipse resistance
- Peer group diversity for sybil resistance
- Strict DER + low-S signatures
- SIGHASH_SINGLE out-of-range fix
- CHECKMULTISIG NULLDUMMY
- OP_RETURN hard fail
- Separated script evaluation
- Bounded script execution

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/RPC_API.md](docs/RPC_API.md) | Full JSON-RPC reference, all 61 commands |
| [docs/RAW_TRANSACTIONS.md](docs/RAW_TRANSACTIONS.md) | Raw transaction construction, signing, multisig workflows |
| [docs/SCRIPT_DEV.md](docs/SCRIPT_DEV.md) | Script developer guide: opcodes, limits, contract patterns, examples |
| [docs/SCRIPT_EXEC.md](docs/SCRIPT_EXEC.md) | Script engine implementation: what changed and why |
| [docs/FEES.md](docs/FEES.md) | Priority-based fee policy details |
| [docs/SPV_CLIENT.md](docs/SPV_CLIENT.md) | SPV protocol specification |
| [docs/SECURITY_FIXES.md](docs/SECURITY_FIXES.md) | Security hardening details |
| [docs/BITOKPOW.md](docs/BITOKPOW.md) | BitokPoW proof-of-work algorithm |
| [docs/SOLO_MINING.md](docs/SOLO_MINING.md) | Solo mining |
| [docs/POOL_INTEGRATION.md](docs/POOL_INTEGRATION.md) | Mining pool integration, Stratum, NOMP |
| [docs/MINING_OPTIMIZATIONS.md](docs/MINING_OPTIMIZATIONS.md) | Mining performance tuning |
| [docs/BUILD_UNIX.md](docs/BUILD_UNIX.md) | Linux/BSD build |
| [docs/BUILD_MACOS.md](docs/BUILD_MACOS.md) | macOS build |
| [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) | Windows build overview |
| [docs/BUILD_WINDOWS_MINGW.md](docs/BUILD_WINDOWS_MINGW.md) | Windows MinGW/MSYS2 |
| [docs/BUILD_WINDOWS_VC.md](docs/BUILD_WINDOWS_VC.md) | Windows Visual Studio |
| [docs/BITOK_DEVELOPMENT.md](docs/BITOK_DEVELOPMENT.md) | Design notes and development philosophy |
| [docs/GENESIS.md](docs/GENESIS.md) | Genesis block parameters |

---

## License

MIT. See [license.txt](license.txt).
