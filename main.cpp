// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "sha.h"
#include "crypto/sha256.h"
#include "atom.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif





//
// Global state
//

CCriticalSection cs_main;

map<uint256, CTransaction> mapTransactions;
CCriticalSection cs_mapTransactions;
unsigned int nTransactionsUpdated = 0;
map<COutPoint, CInPoint> mapNextTx;

map<uint256, CBlockIndex*> mapBlockIndex;

// uint256 hashGenesisBlock("0x0000000000000000000000000000000000000000000000000000000000000000");
uint256 hashGenesisBlock("0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617");
CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;
uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;
int64 nTimeBestReceived = 0;

map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;

map<uint256, CDataStream*> mapOrphanTransactions;
multimap<uint256, CDataStream*> mapOrphanTransactionsByPrev;

map<uint256, CWalletTx> mapWallet;
vector<uint256> vWalletUpdated;
CCriticalSection cs_mapWallet;

map<vector<unsigned char>, CPrivKey> mapKeys;
map<uint160, vector<unsigned char> > mapPubKeys;
map<vector<unsigned char>, vector<unsigned char> > mapHashPreimages;
CCriticalSection cs_mapKeys;
CKey keyUser;

map<uint256, int> mapRequestCount;
CCriticalSection cs_mapRequestCount;

map<string, string> mapAddressBook;
CCriticalSection cs_mapAddressBook;

vector<unsigned char> vchDefaultKey;

// Settings
int fGenerateBitcoins = false;
int fTestMode = false;
int64 nTransactionFee = 0;
CAddress addrIncoming;
int fLimitProcessors = false;
int nLimitProcessors = 1;
int fMinimizeToTray = true;
int fMinimizeOnClose = true;


//////////////////////////////////////////////////////////////////////////////
//
// Blockchain Checkpoints
//

namespace Checkpoints
{
    typedef map<int, uint256> MapCheckpoints;

    static MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        (0, hashGenesisBlock)
        (6666, uint256("0xe4845bb3b5426ace955dea347359030656921883d8723105e4ab79343c27cdca"))
        (14000, uint256("0x10bb78b6ff9825b407f8d30e41f0aee7664759573382875dcf12bb947082c747"))
        (16000, uint256("0xf3506cfb336359ccba2245c630010fd387c7b9fc1c5102b75bb15d9680d3be60"))
        (18000, uint256("0x51bd3ac4edc1b47739987bf7f8657bd075850bb05e65fa6bf78597a9eb3462c2"))
        ;

    bool CheckBlock(int nHeight, const uint256& hash)
    {
        if (fTestMode)
            return true;
        MapCheckpoints::const_iterator i = mapCheckpoints.find(nHeight);
        if (i == mapCheckpoints.end()) return true;
        return hash == i->second;
    }

    int GetTotalBlocksEstimate()
    {
        if (mapCheckpoints.empty())
            return 0;
        return mapCheckpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const map<uint256, CBlockIndex*>& mapBlockIndex)
    {
        int64 nResult = 0;
        BOOST_REVERSE_FOREACH(const MapCheckpoints::value_type& i, mapCheckpoints)
        {
            const uint256& hash = i.second;
            auto t = mapBlockIndex.find(hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return NULL;
    }
}




//////////////////////////////////////////////////////////////////////////////
//
// Transaction Priority (Satoshi's coin-age formula)
//
// priority = sum(value_in * confirmations) / tx_size
//

double ComputePriority(const CTransaction& tx, CTxDB& txdb, int nHeight)
{
    if (tx.IsCoinBase())
        return 0.0;

    double dPriority = 0.0;
    for (int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint prevout = tx.vin[i].prevout;

        CTransaction txPrev;
        CTxIndex txindex;
        if (txdb.ReadTxIndex(prevout.hash, txindex) && txPrev.ReadFromDisk(txindex.pos))
        {
            if (prevout.n < txPrev.vout.size())
            {
                int nInputHeight = 0;
                CBlock block;
                if (block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                {
                    auto mi = mapBlockIndex.find(block.GetHash());
                    if (mi != mapBlockIndex.end())
                        nInputHeight = (*mi).second->nHeight;
                }

                int nConf = nHeight - nInputHeight;
                if (nConf < 1)
                    nConf = 1;

                dPriority += (double)txPrev.vout[prevout.n].nValue * nConf;
            }
        }
    }

    unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
    if (nTxSize == 0)
        return 0.0;

    return dPriority / nTxSize;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transaction Standardness Check
//

bool IsStandard(const CScript& scriptPubKey)
{
    if (nBestHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
    {
        if (scriptPubKey.size() > MAX_SCRIPT_SIZE)
            return false;

        if (scriptPubKey.empty())
            return false;

        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        while (pc < scriptPubKey.end())
        {
            if (!scriptPubKey.GetOp(pc, opcode))
                return false;
        }

        return true;
    }

    if (scriptPubKey.size() == 35 && scriptPubKey[0] == 33 && scriptPubKey[34] == OP_CHECKSIG)
        return true;

    if (scriptPubKey.size() == 25 &&
        scriptPubKey[0] == OP_DUP &&
        scriptPubKey[1] == OP_HASH160 &&
        scriptPubKey[2] == 20 &&
        scriptPubKey[23] == OP_EQUALVERIFY &&
        scriptPubKey[24] == OP_CHECKSIG)
        return true;

    return false;
}

bool IsStandardTx(const CTransaction& tx)
{
    if (tx.nVersion > 1)
        return false;

    foreach(const CTxIn& txin, tx.vin)
    {
        if (nBestHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
        {
            if (txin.scriptSig.size() > MAX_SCRIPT_SIZE)
                return false;
        }
        else
        {
            if (txin.scriptSig.size() > 500)
                return false;
        }
        if (!txin.scriptSig.IsPushOnly())
            return false;
    }

    foreach(const CTxOut& txout, tx.vout)
        if (!IsStandard(txout.scriptPubKey))
            return false;

    return true;
}


//////////////////////////////////////////////////////////////////////////////
//
// mapKeys
//

bool AddKey(const CKey& key)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        mapKeys[key.GetPubKey()] = key.GetPrivKey();
        mapPubKeys[Hash160(key.GetPubKey())] = key.GetPubKey();
    }
    return CWalletDB().WriteKey(key.GetPubKey(), key.GetPrivKey());
}

vector<unsigned char> GenerateNewKey()
{
    CKey key;
    key.MakeNewKey();
    if (!AddKey(key))
        throw runtime_error("GenerateNewKey() : AddKey failed\n");
    return key.GetPubKey();
}




//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

bool AddToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
            wtx.nTimeReceived = GetAdjustedTime();

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            if (wtxIn.fSpent && wtxIn.fSpent != wtx.fSpent)
            {
                wtx.fSpent = wtxIn.fSpent;
                fUpdated = true;
            }
            if (wtxIn.mapValue.count("stealth_address") && !wtxIn.mapValue.find("stealth_address")->second.empty()
                && wtx.mapValue["stealth_address"].empty())
            {
                wtx.mapValue["stealth_address"] = wtxIn.mapValue.find("stealth_address")->second;
                fUpdated = true;
            }
        }

        if (fDebug)
            printf("[WALLET] AddToWallet %s %s%s\n", wtxIn.GetHash().ToString().substr(0,6).c_str(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;

        // If default receiving address gets used, replace it with a new one
        CScript scriptDefaultKey;
        scriptDefaultKey.SetBitcoinAddress(vchDefaultKey);
        foreach(const CTxOut& txout, wtx.vout)
        {
            if (txout.scriptPubKey == scriptDefaultKey)
            {
                CWalletDB walletdb;
                walletdb.WriteDefaultKey(GenerateNewKey());
                walletdb.WriteName(PubKeyToAddress(vchDefaultKey), "");
            }
        }

        // Notify UI
        vWalletUpdated.push_back(hash);
    }

    // Refresh UI
    MainFrameRepaint();
    return true;
}

bool CheckStealthTransaction(const CTransaction& tx, string& strStealthAddrOut)
{
    strStealthAddrOut.clear();
    vector<unsigned char> vchEphemPub;
    bool fFoundEphem = false;

    foreach(const CTxOut& txout, tx.vout)
    {
        if (ParseStealthOpReturn(txout.scriptPubKey, vchEphemPub))
        {
            fFoundEphem = true;
            break;
        }
    }

    if (!fFoundEphem)
        return false;

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        foreach(const CStealthAddress& sxAddr, vStealthAddresses)
        {
            CWalletDB walletdb;
            CPrivKey vchScanPrivKey;
            if (!walletdb.ReadStealthScanKey(sxAddr.scan_pubkey, vchScanPrivKey))
                continue;

            CKey scanKey;
            if (!scanKey.SetPrivKey(vchScanPrivKey))
                continue;

            vector<unsigned char> vchScanPriv = scanKey.GetSecret();

            vector<unsigned char> vchDestPubKey;
            if (!StealthScan(vchScanPriv, sxAddr.spend_pubkey, vchEphemPub, vchDestPubKey))
                continue;

            uint160 destHash = Hash160(vchDestPubKey);

            foreach(const CTxOut& txout, tx.vout)
            {
                uint160 scriptHash = txout.scriptPubKey.GetBitcoinAddressHash160();
                if (scriptHash == 0)
                    continue;

                if (scriptHash == destHash)
                {
                    CPrivKey vchSpendPrivKey;
                    if (!walletdb.ReadStealthSpendKey(sxAddr.spend_pubkey, vchSpendPrivKey))
                        break;

                    CKey spendKey;
                    if (!spendKey.SetPrivKey(vchSpendPrivKey))
                        break;

                    vector<unsigned char> vchSpendPriv = spendKey.GetSecret();
                    vector<unsigned char> vchDestPriv;
                    if (!StealthSecretSpend(vchScanPriv, vchEphemPub, vchSpendPriv, vchDestPriv))
                        break;

                    CKey destKey;
                    if (!destKey.SetSecret(vchDestPriv))
                        break;

                    if (!AddKey(destKey))
                        break;

                    mapStealthDestToScan[vchDestPubKey] = make_pair(sxAddr.scan_pubkey, vchEphemPub);
                    walletdb.WriteStealthDestMap(vchDestPubKey, make_pair(sxAddr.scan_pubkey, vchEphemPub));

                    strStealthAddrOut = sxAddr.Encoded();

                    printf("stealth: found payment to stealth address %s (dest=%s)\n",
                           sxAddr.Encoded().substr(0, 16).c_str(),
                           PubKeyToAddress(vchDestPubKey).c_str());
                    return true;
                }
            }
        }
    }

    return false;
}


bool AddToWalletIfMine(const CTransaction& tx, const CBlock* pblock)
{
    string strStealthAddr;
    CheckStealthTransaction(tx, strStealthAddr);

    if (tx.IsMine() || mapWallet.count(tx.GetHash()))
    {
        CWalletTx wtx(tx);
        if (!strStealthAddr.empty())
            wtx.mapValue["stealth_address"] = strStealthAddr;
        if (pblock)
            wtx.SetMerkleBranch(pblock);
        return AddToWallet(wtx);
    }
    return true;
}

int ScanWalletTransactions(CBlockIndex* pindexStart, boost::function<bool (int, int, int)> progressCallback, set<uint160>* psetOnChainAddresses)
{
    int nFound = 0;
    int nTotalBlocks = nBestHeight - (pindexStart ? pindexStart->nHeight : 0) + 1;
    if (nTotalBlocks < 1)
        nTotalBlocks = 1;
    int nScanned = 0;
    int nLastPct = -1;

    printf("[WALLET] Rescan starting from height %d (%d blocks to scan)\n",
           pindexStart ? pindexStart->nHeight : 0, nTotalBlocks);

    CBlockIndex* pindex = pindexStart;
    while (pindex)
    {
        CBlock block;
        block.ReadFromDisk(pindex, true);
        foreach(CTransaction& tx, block.vtx)
        {
            if (psetOnChainAddresses)
            {
                foreach(const CTxOut& txout, tx.vout)
                {
                    uint160 h = txout.scriptPubKey.GetBitcoinAddressHash160();
                    if (h != 0)
                        psetOnChainAddresses->insert(h);
                }
            }
            if (tx.IsMine() && !mapWallet.count(tx.GetHash()))
                nFound++;
            AddToWalletIfMine(tx, &block);
            foreach(const CTxIn& txin, tx.vin)
                WalletUpdateSpent(txin.prevout);
        }
        nScanned++;
        int nPct = (nScanned * 100) / nTotalBlocks;
        if (nPct / 10 > nLastPct / 10 && nPct <= 100)
        {
            nLastPct = nPct;
            printf("[WALLET] Rescan progress: %d%% (%d/%d blocks, %d found so far)\n",
                   (nPct / 10) * 10, nScanned, nTotalBlocks, nFound);
        }
        if (progressCallback && nScanned % 100 == 0)
        {
            if (!progressCallback(nScanned, nTotalBlocks, nFound))
                break;
        }
        pindex = pindex->pnext;
    }
    if (progressCallback)
        progressCallback(nTotalBlocks, nTotalBlocks, nFound);
    printf("[WALLET] Rescan complete: scanned %d blocks, found %d new wallet transactions\n", nScanned, nFound);
    return nFound;
}

bool EraseFromWallet(uint256 hash)
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        if (mapWallet.erase(hash))
            CWalletDB().EraseTx(hash);
    }
    return true;
}

void WalletUpdateSpent(const COutPoint& prevout)
{
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& wtx = (*mi).second;
            if (!wtx.fSpent && wtx.vout[prevout.n].IsMine())
            {
                if (fDebug)
                    printf("[WALLET] Found spent coin %sbc %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                wtx.fSpent = true;
                wtx.WriteToDisk();
                vWalletUpdated.push_back(prevout.hash);
            }
        }
    }
}








//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

void AddOrphanTx(const CDataStream& vMsg)
{
    CTransaction tx;
    CDataStream(vMsg) >> tx;
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return;
    CDataStream* pvMsg = mapOrphanTransactions[hash] = new CDataStream(vMsg);
    foreach(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev.insert(make_pair(txin.prevout.hash, pvMsg));
}

void EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CDataStream* pvMsg = mapOrphanTransactions[hash];
    CTransaction tx;
    CDataStream(*pvMsg) >> tx;
    foreach(const CTxIn& txin, tx.vin)
    {
        for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(txin.prevout.hash);
             mi != mapOrphanTransactionsByPrev.upper_bound(txin.prevout.hash);)
        {
            if ((*mi).second == pvMsg)
                mapOrphanTransactionsByPrev.erase(mi++);
            else
                mi++;
        }
    }
    delete pvMsg;
    mapOrphanTransactions.erase(hash);
}








//////////////////////////////////////////////////////////////////////////////
//
// CTransaction
//

bool CTxIn::IsMine() const
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (prevout.n < prev.vout.size())
                if (prev.vout[prevout.n].IsMine())
                    return true;
        }
    }
    return false;
}

int64 CTxIn::GetDebit() const
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (prevout.n < prev.vout.size())
                if (prev.vout[prevout.n].IsMine())
                    return prev.vout[prevout.n].nValue;
        }
    }
    return 0;
}

int64 CWalletTx::GetTxTime() const
{
    if (!fTimeReceivedIsTxTime && hashBlock != 0)
    {
        // If we did not receive the transaction directly, we rely on the block's
        // time to figure out when it happened.  We use the median over a range
        // of blocks to try to filter out inaccurate block times.
        auto mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (pindex)
                return pindex->GetMedianTime();
        }
    }
    return nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    CRITICAL_BLOCK(cs_mapRequestCount)
    {
        if (IsCoinBase())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::iterator mi = mapRequestCount.find(hashBlock);
                if (mi != mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::iterator mi = mapRequestCount.find(GetHash());
            if (mi != mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::iterator mi = mapRequestCount.find(hashBlock);
                    if (mi != mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}




int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    if (fClient)
    {
        if (hashBlock == 0)
            return 0;
    }
    else
    {
        CBlock blockTmp;
        if (pblock == NULL)
        {
            // Load the block this tx is in
            CTxIndex txindex;
            if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
                return 0;
            pblock = &blockTmp;
        }

        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();

        // Locate the transaction
        for (nIndex = 0; nIndex < pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == *(CTransaction*)this)
                break;
        if (nIndex == pblock->vtx.size())
        {
            vMerkleBranch.clear();
            nIndex = -1;
            printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
            return 0;
        }

        // Fill in merkle branch
        vMerkleBranch = pblock->GetMerkleBranch(nIndex);
    }

    // Is the tx in a block that's in the main chain
    auto mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}



void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        foreach(const CTxIn& txin, vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        CRITICAL_BLOCK(cs_mapWallet)
        {
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                if (mapWallet.count(hash))
                {
                    tx = mapWallet[hash];
                    foreach(const CMerkleTx& txWalletPrev, mapWallet[hash].vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (!fClient && txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                    foreach(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}











bool CTransaction::AcceptTransaction(CTxDB& txdb, bool fCheckInputs, bool* pfMissingInputs)
{
    if (pfMissingInputs)
        *pfMissingInputs = false;

    // Coinbase is only valid in a block, not as a loose transaction
    if (IsCoinBase())
        return error("AcceptTransaction() : coinbase as individual tx");

    if (!CheckTransaction())
        return error("AcceptTransaction() : CheckTransaction failed");

    // Check for standard transaction types
    if (!IsStandardTx(*this))
        return error("AcceptTransaction() : nonstandard transaction type");

    // To help v0.1.5 clients who would see it as a negative number
    if (nLockTime > INT_MAX)
        return error("AcceptTransaction() : not accepting nLockTime beyond 2038");

    // Do we already have it?
    uint256 hash = GetHash();
    CRITICAL_BLOCK(cs_mapTransactions)
        if (mapTransactions.count(hash))
            return false;
    if (fCheckInputs)
        if (txdb.ContainsTx(hash))
            return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    for (int i = 0; i < vin.size(); i++)
    {
        COutPoint outpoint = vin[i].prevout;
        if (mapNextTx.count(outpoint))
        {
            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = mapNextTx[outpoint].ptx;
            if (!IsNewerThan(*ptxOld))
                return false;
            for (int i = 0; i < vin.size(); i++)
            {
                COutPoint outpoint = vin[i].prevout;
                if (!mapNextTx.count(outpoint) || mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }

    // Check against previous transactions
    map<uint256, CTxIndex> mapUnused;
    int64 nFees = 0;
    if (fCheckInputs && !ConnectInputs(txdb, mapUnused, CDiskTxPos(1,1,1), nBestHeight + 1, nFees, false, false))
    {
        if (pfMissingInputs)
            *pfMissingInputs = true;
        return error("AcceptTransaction() : ConnectInputs failed %s", hash.ToString().substr(0,6).c_str());
    }

    if (fCheckInputs && nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
    {
        double dPriority = ComputePriority(*this, txdb, nBestHeight + 1);
        bool fAllowFree = (dPriority >= FREE_PRIORITY_THRESHOLD);
        int64 nMinRelayFee = GetMinFee(1, fAllowFree);

        if (nFees < nMinRelayFee)
            return error("AcceptTransaction() : not enough fees %s, %" PRI64d " < %" PRI64d,
                         hash.ToString().substr(0,6).c_str(), nFees, nMinRelayFee);
    }

    // ATOM layer: validate nonce and spendable balance before accepting to mempool.
    CAtomTransfer atomTransfer;
    bool fHasAtom = false;
    if (nBestHeight + 1 >= ATOM_ACTIVATION_HEIGHT)
        fHasAtom = GetAtomTransfer(*this, atomTransfer);

    if (fHasAtom)
    {
        if (atomTransfer.nType == ATOM_TYPE_COINBASE)
            return error("AcceptTransaction() : ATOM COINBASE type rejected in user tx %s",
                         hash.ToString().substr(0, 6).c_str());

        if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
        {
            uint160 bridgeAddr(std::vector<unsigned char>(ATOM_BRIDGE_HASH, ATOM_BRIDGE_HASH + 20));
            if (atomTransfer.addrFrom != bridgeAddr)
                return error("AcceptTransaction() : ATOM BRIDGE_TO_BITOK rejected: addrFrom is not bridge address in tx %s",
                             hash.ToString().substr(0, 6).c_str());
        }

        if (atomTransfer.nType == ATOM_TYPE_TRANSFER || atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL)
        {
            if (!VerifyAtomSender(*this, atomTransfer.addrFrom))
                return error("AcceptTransaction() : ATOM sender verification failed: no input signed by addrFrom %s in tx %s",
                             atomTransfer.addrFrom.ToString().c_str(),
                             hash.ToString().substr(0, 6).c_str());

            int64 nAtomBal = 0;
            uint32_t nConfirmedNonce = 0;
            if (fCheckInputs)
            {
                CTxDB txdbAtom("r");
                bool fDbError = false;
                if (!txdbAtom.ReadAtomBalance(atomTransfer.addrFrom, nAtomBal, nConfirmedNonce, fDbError) && fDbError)
                    return error("AcceptTransaction() : DB error reading ATOM balance for %s",
                                 atomTransfer.addrFrom.ToString().c_str());
            }

            if (!AtomMempoolCheckNonce(atomTransfer.addrFrom, atomTransfer.nNonce, nConfirmedNonce))
                return error("AcceptTransaction() : ATOM nonce invalid or already used for %s",
                             hash.ToString().substr(0, 6).c_str());

            int64 nAvailable = AtomGetEffectiveBalance(atomTransfer.addrFrom, nAtomBal);
            if (nAvailable < atomTransfer.nAmount)
                return error("AcceptTransaction() : insufficient ATOM balance for %s (spendable %" PRI64d " need %" PRI64d ")",
                             hash.ToString().substr(0, 6).c_str(), nAvailable, atomTransfer.nAmount);
        }
    }

    // DEX layer: validate DEX offer/cancel/take before accepting to mempool.
    unsigned char nDexType = 0;
    CAtomDexOffer dexOffer;
    CAtomDexCancel dexCancel;
    CAtomDexTake dexTake;
    bool fHasDex = false;
    if (nBestHeight + 1 >= ATOM_DEX_ACTIVATION_HEIGHT && !fHasAtom)
        fHasDex = GetDexPayload(*this, nDexType, dexOffer, dexCancel, dexTake);

    if (fHasDex)
    {
        if (nDexType == ATOM_TYPE_DEX_OFFER)
        {
            if (!VerifyAtomSender(*this, dexOffer.addrFrom))
                return error("AcceptTransaction() : DEX_OFFER sender verification failed in tx %s",
                             hash.ToString().substr(0, 6).c_str());

            if (dexOffer.nSide != ATOM_DEX_SIDE_SELL_ATOM)
                return error("AcceptTransaction() : DEX_OFFER only sell-side offers are supported in tx %s",
                             hash.ToString().substr(0, 6).c_str());

            int64 nAtomBal = 0;
            uint32_t nConfirmedNonce = 0;
            if (fCheckInputs)
            {
                CTxDB txdbDex("r");
                txdbDex.ReadAtomBalance(dexOffer.addrFrom, nAtomBal, nConfirmedNonce);
            }
            if (!AtomMempoolCheckNonce(dexOffer.addrFrom, dexOffer.nNonce, nConfirmedNonce))
                return error("AcceptTransaction() : DEX_OFFER nonce invalid in tx %s",
                             hash.ToString().substr(0, 6).c_str());
            int64 nAvailable = AtomGetEffectiveBalance(dexOffer.addrFrom, nAtomBal);
            if (nAvailable < dexOffer.nAtomAmount)
                return error("AcceptTransaction() : DEX_OFFER insufficient ATOM for sell in tx %s",
                             hash.ToString().substr(0, 6).c_str());
        }
        else if (nDexType == ATOM_TYPE_DEX_CANCEL)
        {
            if (!VerifyAtomSender(*this, dexCancel.addrFrom))
                return error("AcceptTransaction() : DEX_CANCEL sender verification failed in tx %s",
                             hash.ToString().substr(0, 6).c_str());

            CRITICAL_BLOCK(cs_mapDexOrders)
            {
                if (setDexMempoolCancels.count(dexCancel.hashOffer))
                    return error("AcceptTransaction() : DEX_CANCEL duplicate: offer %s already has pending cancel in tx %s",
                                 dexCancel.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());
                if (setDexMempoolTakes.count(dexCancel.hashOffer))
                    return error("AcceptTransaction() : DEX_CANCEL rejected: offer %s has pending take in tx %s",
                                 dexCancel.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());

                std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(dexCancel.hashOffer);
                if (it == mapDexOrders.end())
                    return error("AcceptTransaction() : DEX_CANCEL references unknown offer %s in tx %s",
                                 dexCancel.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());
                if (it->second.addrMaker != dexCancel.addrFrom)
                    return error("AcceptTransaction() : DEX_CANCEL addrFrom does not match offer maker in tx %s",
                                 hash.ToString().substr(0, 6).c_str());
            }
        }
        else if (nDexType == ATOM_TYPE_DEX_TAKE)
        {
            if (!VerifyAtomSender(*this, dexTake.addrFrom))
                return error("AcceptTransaction() : DEX_TAKE sender verification failed in tx %s",
                             hash.ToString().substr(0, 6).c_str());

            CRITICAL_BLOCK(cs_mapDexOrders)
            {
                if (setDexMempoolTakes.count(dexTake.hashOffer))
                    return error("AcceptTransaction() : DEX_TAKE duplicate: offer %s already has pending take in tx %s",
                                 dexTake.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());
                if (setDexMempoolCancels.count(dexTake.hashOffer))
                    return error("AcceptTransaction() : DEX_TAKE rejected: offer %s has pending cancel in tx %s",
                                 dexTake.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());

                std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(dexTake.hashOffer);
                if (it == mapDexOrders.end())
                    return error("AcceptTransaction() : DEX_TAKE references unknown offer %s in tx %s",
                                 dexTake.hashOffer.ToString().substr(0, 10).c_str(),
                                 hash.ToString().substr(0, 6).c_str());

                if (it->second.addrMaker == dexTake.addrFrom)
                    return error("AcceptTransaction() : DEX_TAKE taker cannot be the maker in tx %s",
                                 hash.ToString().substr(0, 6).c_str());
            }
        }
    }

    // Store transaction in memory
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        if (ptxOld)
        {
            if (fDebug)
                printf("[TX] Replacing tx %s with new version\n", ptxOld->GetHash().ToString().c_str());
            mapTransactions.erase(ptxOld->GetHash());
        }
        AddToMemoryPool();
    }

    // If this is an ATOM transfer, register it in the mempool ATOM index
    if (fHasAtom)
        AtomMempoolAdd(hash, atomTransfer);

    if (fHasDex)
    {
        if (nDexType == ATOM_TYPE_DEX_OFFER)
            DexMempoolAddOffer(hash, dexOffer);
        else if (nDexType == ATOM_TYPE_DEX_CANCEL)
            DexMempoolAddCancel(hash, dexCancel.hashOffer);
        else if (nDexType == ATOM_TYPE_DEX_TAKE)
            DexMempoolAddTake(hash, dexTake.hashOffer);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallet(ptxOld->GetHash());

    if (fDebug)
        printf("[TX] AcceptTransaction(): accepted %s\n", hash.ToString().substr(0,6).c_str());
    return true;
}


bool CTransaction::AddToMemoryPool()
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptTransaction to properly check the transaction first.
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        uint256 hash = GetHash();
        mapTransactions[hash] = *this;
        for (int i = 0; i < vin.size(); i++)
            mapNextTx[vin[i].prevout] = CInPoint(&mapTransactions[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}


bool CTransaction::RemoveFromMemoryPool()
{
    // Remove transaction from memory pool
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        foreach(const CTxIn& txin, vin)
            mapNextTx.erase(txin.prevout);
        mapTransactions.erase(GetHash());
        nTransactionsUpdated++;
    }
    return true;
}






int CMerkleTx::GetDepthInMainChain(int& nHeightRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;

    // Find the block it claims to be in
    auto mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified)
    {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    nHeightRet = pindex->nHeight;
    return pindexBest->nHeight - pindex->nHeight + 1;
}


int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    int nMaturity = GetCoinbaseMaturity();
    int nBuffer = (nMaturity > 0) ? 20 : 0;
    return max(0, (nMaturity + nBuffer) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptTransaction(CTxDB& txdb, bool fCheckInputs)
{
    if (fClient)
    {
        if (!IsInMainChain() && !ClientConnectInputs())
            return false;
        return CTransaction::AcceptTransaction(txdb, false);
    }
    else
    {
        return CTransaction::AcceptTransaction(txdb, fCheckInputs);
    }
}



bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs)
{
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        foreach(CMerkleTx& tx, vtxPrev)
        {
            if (!tx.IsCoinBase())
            {
                uint256 hash = tx.GetHash();
                if (!mapTransactions.count(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptTransaction(txdb, fCheckInputs);
            }
        }
        if (!IsCoinBase())
            return AcceptTransaction(txdb, fCheckInputs);
    }
    return true;
}

void ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    CRITICAL_BLOCK(cs_mapWallet)
    {
        foreach(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            if (wtx.fSpent && wtx.IsCoinBase())
                continue;

            CTxIndex txindex;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (!wtx.fSpent)
                {
                    if (txindex.vSpent.size() != wtx.vout.size())
                    {
                        printf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %d != wtx.vout.size() %d\n", txindex.vSpent.size(), wtx.vout.size());
                        continue;
                    }
                    for (int i = 0; i < txindex.vSpent.size(); i++)
                    {
                        if (!txindex.vSpent[i].IsNull() && wtx.vout[i].IsMine())
                        {
                            if (fDebug)
                                printf("[WALLET] ReacceptWalletTransactions found spent coin %sbc %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                            wtx.fSpent = true;
                            wtx.WriteToDisk();
                            break;
                        }
                    }
                }
            }
            else
            {
                // Reaccept any txes of ours that aren't already in a block
                if (!wtx.IsCoinBase())
                    wtx.AcceptWalletTransaction(txdb, false);
            }
        }
    }
}


void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    foreach(const CMerkleTx& tx, vtxPrev)
    {
        if (!tx.IsCoinBase())
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayMessage(CInv(MSG_TX, hash), (CTransaction)tx);
        }
    }
    if (!IsCoinBase())
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            if (fDebug)
                printf("[TX] Relaying wtx %s\n", hash.ToString().substr(0,6).c_str());
            RelayMessage(CInv(MSG_TX, hash), (CTransaction)*this);
        }
    }
}

void ResendWalletTransactions()
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    static int64 nNextTime;
    if (GetTime() < nNextTime)
        return;
    bool fFirst = (nNextTime == 0);
    nNextTime = GetTime() + GetRand(120 * 60);
    if (fFirst)
        return;

    // Rebroadcast any of our txes that aren't in a block yet
    if (fDebug)
        printf("[TX] ResendWalletTransactions()\n");
    CTxDB txdb("r");
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        foreach(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (nTimeBestReceived - wtx.nTimeReceived > 60 * 60)
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        foreach(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;
            wtx.RelayWalletTransaction(txdb);
        }
    }
}










//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool CBlock::ReadFromDisk(const CBlockIndex* pblockindex, bool fReadTransactions)
{
    return ReadFromDisk(pblockindex->nFile, pblockindex->nBlockPos, fReadTransactions);
}

uint256 GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}

int64 CBlock::GetBlockValue(int64 nFees) const
{
    int64 nSubsidy = 50 * COIN;

    nSubsidy >>= (nBestHeight / 210000);

    return nSubsidy + nFees;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast)
{
    const unsigned int nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const unsigned int nTargetSpacing = 10 * 60; // ten minutes
    const unsigned int nInterval = nTargetTimespan / nTargetSpacing; // 2016 blocks

    // Genesis block
    if (pindexLast == NULL)
        return bnProofOfWorkLimit.GetCompact();

    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;

    // Go back by what we want to be 7 blocks worth
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    unsigned int nActualTimespan = pindexLast->nTime - pindexFirst->nTime;
    if (fDebug)
        printf("[DIFF] nActualTimespan = %d before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    if (fDebug)
    {
        printf("[DIFF] RETARGET: target=%d actual=%d\n", nTargetTimespan, nActualTimespan);
        printf("[DIFF] Before: %08x  After: %08x\n", pindexLast->nBits, bnNew.GetCompact());
    }

    return bnNew.GetCompact();
}









bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase())
    {
        foreach(const CTxIn& txin, vin)
        {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            txdb.UpdateTxIndex(prevout.hash, txindex);
        }
    }

    // Remove transaction from index
    if (!txdb.EraseTxIndex(*this))
        return error("DisconnectInputs() : EraseTxPos failed");

    return true;
}


bool CTransaction::ConnectInputs(CTxDB& txdb, map<uint256, CTxIndex>& mapTestPool, CDiskTxPos posThisTx, int nHeight, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee)
{
    // Take over previous transactions' spent pointers
    if (!IsCoinBase())
    {
        int64 nValueIn = 0;
        for (int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;

            // Read txindex
            CTxIndex txindex;
            bool fFound = true;
            if (fMiner && mapTestPool.count(prevout.hash))
            {
                // Get txindex from current proposed changes
                txindex = mapTestPool[prevout.hash];
            }
            else
            {
                // Read txindex from txdb
                fFound = txdb.ReadTxIndex(prevout.hash, txindex);
            }
            if (!fFound && (fBlock || fMiner))
                return fMiner ? false : error("ConnectInputs() : %s prev tx %s index entry not found", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());

            // Read txPrev
            CTransaction txPrev;
            if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
            {
                // Get prev tx from single transactions in memory
                CRITICAL_BLOCK(cs_mapTransactions)
                {
                    if (!mapTransactions.count(prevout.hash))
                        return error("ConnectInputs() : %s mapTransactions prev not found %s", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());
                    txPrev = mapTransactions[prevout.hash];
                }
                if (!fFound)
                    txindex.vSpent.resize(txPrev.vout.size());
            }
            else
            {
                // Get prev tx from disk
                if (!txPrev.ReadFromDisk(txindex.pos))
                    return error("ConnectInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());
            }

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return error("ConnectInputs() : %s prevout.n out of range %d %d %d prev tx %s\n%s", GetHash().ToString().substr(0,6).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,6).c_str(), txPrev.ToString().c_str());

            // If prev is coinbase, check that it's matured
            if (txPrev.IsCoinBase())
                for (CBlockIndex* pindex = pindexBest; pindex && nBestHeight - pindex->nHeight < GetCoinbaseMaturity()-1; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                        return error("ConnectInputs() : tried to spend coinbase at depth %d", nBestHeight - pindex->nHeight);

            // Verify signature
            unsigned int nScriptFlags = SCRIPT_VERIFY_NONE;
            if (nHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                nScriptFlags |= SCRIPT_VERIFY_EXEC;
            if (!VerifySignature(txPrev, *this, i, 0, nScriptFlags))
                return error("ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,6).c_str());

            // Check for conflicts
            if (!txindex.vSpent[prevout.n].IsNull())
                return fMiner ? false : error("ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().substr(0,6).c_str(), txindex.vSpent[prevout.n].ToString().c_str());

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock)
                txdb.UpdateTxIndex(prevout.hash, txindex);
            else if (fMiner)
                mapTestPool[prevout.hash] = txindex;

            nValueIn += txPrev.vout[prevout.n].nValue;
        }

        // Tally transaction fees
        int64 nTxFee = nValueIn - GetValueOut();
        if (nTxFee < 0)
            return error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().substr(0,6).c_str());
        if (nTxFee < nMinFee)
            return false;
        nFees += nTxFee;
    }

    if (fBlock)
    {
        // Add transaction to disk index
        if (!txdb.AddTxIndex(*this, posThisTx, nHeight))
            return error("ConnectInputs() : AddTxPos failed");
    }
    else if (fMiner)
    {
        // Add transaction to test pool
        mapTestPool[GetHash()] = CTxIndex(CDiskTxPos(1,1,1), vout.size());
    }

    return true;
}


bool CTransaction::ClientConnectInputs()
{
    if (IsCoinBase())
        return false;

    // Take over previous transactions' spent pointers
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        int64 nValueIn = 0;
        for (int i = 0; i < vin.size(); i++)
        {
            // Get prev tx from single transactions in memory
            COutPoint prevout = vin[i].prevout;
            if (!mapTransactions.count(prevout.hash))
                return false;
            CTransaction& txPrev = mapTransactions[prevout.hash];

            if (prevout.n >= txPrev.vout.size())
                return false;

            // Verify signature
            unsigned int nScriptFlags = SCRIPT_VERIFY_NONE;
            if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                nScriptFlags |= SCRIPT_VERIFY_EXEC;
            if (!VerifySignature(txPrev, *this, i, 0, nScriptFlags))
                return error("ConnectInputs() : VerifySignature failed");

            ///// this is redundant with the mapNextTx stuff, not sure which I want to get rid of
            ///// this has to go away now that posNext is gone
            // // Check for conflicts
            // if (!txPrev.vout[prevout.n].posNext.IsNull())
            //     return error("ConnectInputs() : prev tx already used");
            //
            // // Flag outpoints as used
            // txPrev.vout[prevout.n].posNext = posThisTx;

            nValueIn += txPrev.vout[prevout.n].nValue;
        }
        if (GetValueOut() > nValueIn)
            return false;
    }

    return true;
}




static bool GetScriptAddr(const CScript& scriptPubKey, uint160& addrRet)
{
    if (ExtractHash160(scriptPubKey, addrRet))
        return true;
    if (scriptPubKey.size() == 35 && scriptPubKey[0] == 33 && scriptPubKey[34] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 34);
        addrRet = Hash160(vchPubKey);
        return true;
    }
    if (scriptPubKey.size() == 67 && scriptPubKey[0] == 65 && scriptPubKey[66] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 66);
        addrRet = Hash160(vchPubKey);
        return true;
    }
    return false;
}

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
    {
        const CTransaction& tx = vtx[i];
        uint256 txhash = tx.GetHash();

        if (fUseIndexer && !fIndexerRebuilding)
        {
            // Restore inputs: re-add addrout entries for UTXOs that were spent by this tx
            if (!tx.IsCoinBase())
            {
                for (unsigned int j = 0; j < tx.vin.size(); j++)
                {
                    const COutPoint& prevout = tx.vin[j].prevout;

                    CTransaction txPrev;
                    CTxIndex txPrevIndex;
                    if (txdb.ReadDiskTx(prevout.hash, txPrev, txPrevIndex))
                    {
                        if (prevout.n < txPrev.vout.size())
                        {
                            uint160 addr;
                            if (GetScriptAddr(txPrev.vout[prevout.n].scriptPubKey, addr))
                            {
                                int nPrevHeight = 0;
                                for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
                                {
                                    CBlockIndex* pb = mi->second;
                                    if (pb->nFile == txPrevIndex.pos.nFile && pb->nBlockPos == txPrevIndex.pos.nBlockPos)
                                    {
                                        nPrevHeight = pb->nHeight;
                                        break;
                                    }
                                }
                                bool fPrevCoinBase = txPrev.IsCoinBase();
                                txdb.WriteAddrOut(addr, prevout.hash, prevout.n, txPrev.vout[prevout.n].nValue, nPrevHeight, fPrevCoinBase);
                                txdb.EraseAddrTx(addr, txhash);
                            }
                        }
                    }
                }
            }

            // Remove outputs created by this tx
            for (unsigned int n = 0; n < tx.vout.size(); n++)
            {
                uint160 addr;
                if (GetScriptAddr(tx.vout[n].scriptPubKey, addr))
                {
                    txdb.EraseAddrOut(addr, txhash, n);
                    txdb.EraseAddrTx(addr, txhash);
                }
            }
        }

        // ATOM layer: reverse ATOM balance changes for this tx.
        if (pindex->nHeight >= ATOM_ACTIVATION_HEIGHT)
        {
            CAtomTransfer atomTransfer;
            bool fAtomFound = GetAtomTransfer(tx, atomTransfer);
            if (fAtomFound)
            {
                if (atomTransfer.nType == ATOM_TYPE_TRANSFER)
                {
                    int64 nBalFrom = 0;
                    uint32_t nNonceFrom = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    nBalFrom += atomTransfer.nAmount;
                    if (nNonceFrom == atomTransfer.nNonce && atomTransfer.nNonce > 0)
                        nNonceFrom = atomTransfer.nNonce - 1;
                    txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    if (fUseIndexer && !fIndexerRebuilding)
                        txdb.EraseAtomAddrTx(atomTransfer.addrFrom, txhash);

                    int64 nBalTo = 0;
                    uint32_t nNonceTo = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    nBalTo -= atomTransfer.nAmount;
                    if (nBalTo < 0) nBalTo = 0;
                    txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    if (fUseIndexer && !fIndexerRebuilding)
                        txdb.EraseAtomAddrTx(atomTransfer.addrTo, txhash);
                }
                else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                {
                    int64 nBalFrom = 0;
                    uint32_t nNonceFrom = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    nBalFrom += atomTransfer.nAmount;
                    if (nNonceFrom == atomTransfer.nNonce && atomTransfer.nNonce > 0)
                        nNonceFrom = atomTransfer.nNonce - 1;
                    txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    if (fUseIndexer && !fIndexerRebuilding)
                        txdb.EraseAtomAddrTx(atomTransfer.addrFrom, txhash);
                }
                else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
                {
                    int64 nBalTo = 0;
                    uint32_t nNonceTo = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    nBalTo -= atomTransfer.nAmount;
                    if (nBalTo < 0) nBalTo = 0;
                    txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    if (fUseIndexer && !fIndexerRebuilding)
                        txdb.EraseAtomAddrTx(atomTransfer.addrTo, txhash);

                    int64 nTotalMinted = 0;
                    txdb.ReadAtomTotalMinted(nTotalMinted);
                    nTotalMinted -= atomTransfer.nAmount;
                    if (nTotalMinted < 0) nTotalMinted = 0;
                    txdb.WriteAtomTotalMinted(nTotalMinted);
                }

                txdb.EraseAtomTx(txhash);
            }
        }

        if (pindex->nHeight >= ATOM_DEX_ACTIVATION_HEIGHT)
        {
            unsigned char nDexType = 0;
            CAtomDexOffer dexOffer;
            CAtomDexCancel dexCancel;
            CAtomDexTake dexTake;

            if (GetDexPayload(tx, nDexType, dexOffer, dexCancel, dexTake))
            {
                if (nDexType == ATOM_TYPE_DEX_OFFER)
                {
                    int64 nBal = 0;
                    uint32_t nNon = 0;
                    txdb.ReadAtomBalance(dexOffer.addrFrom, nBal, nNon);
                    nBal += dexOffer.nAtomAmount;
                    if (nNon == dexOffer.nNonce && dexOffer.nNonce > 0)
                        nNon = dexOffer.nNonce - 1;
                    txdb.WriteAtomBalance(dexOffer.addrFrom, nBal, nNon);
                    txdb.EraseDexOrder(txhash);
                    CRITICAL_BLOCK(cs_mapDexOrders)
                    {
                        mapDexOrders.erase(txhash);
                    }
                }
                else if (nDexType == ATOM_TYPE_DEX_CANCEL)
                {
                    CTransaction txOffer;
                    if (txdb.ReadDiskTx(dexCancel.hashOffer, txOffer))
                    {
                        unsigned char nOfferType = 0;
                        CAtomDexOffer origOffer;
                        CAtomDexCancel ign1;
                        CAtomDexTake ign2;
                        if (GetDexPayload(txOffer, nOfferType, origOffer, ign1, ign2) &&
                            nOfferType == ATOM_TYPE_DEX_OFFER)
                        {
                            int64 nBal = 0;
                            uint32_t nNon = 0;
                            txdb.ReadAtomBalance(origOffer.addrFrom, nBal, nNon);
                            nBal -= origOffer.nAtomAmount;
                            if (nBal < 0) nBal = 0;
                            txdb.WriteAtomBalance(origOffer.addrFrom, nBal, nNon);

                            CDexOrderEntry entry;
                            entry.hashTx      = dexCancel.hashOffer;
                            entry.addrMaker   = origOffer.addrFrom;
                            entry.nSide       = origOffer.nSide;
                            entry.nAtomAmount = origOffer.nAtomAmount;
                            entry.nPrice      = origOffer.nPrice;
                            entry.nNonce      = origOffer.nNonce;
                            entry.nHeight     = 0;
                            CTxIndex txindex;
                            if (txdb.ReadTxIndex(dexCancel.hashOffer, txindex))
                            {
                                CBlock blk;
                                if (blk.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                                {
                                    std::map<uint256, CBlockIndex*>::iterator bi = mapBlockIndex.find(blk.GetHash());
                                    if (bi != mapBlockIndex.end())
                                        entry.nHeight = bi->second->nHeight;
                                }
                            }
                            txdb.WriteDexOrder(dexCancel.hashOffer, entry);
                            CRITICAL_BLOCK(cs_mapDexOrders)
                            {
                                mapDexOrders[dexCancel.hashOffer] = entry;
                            }
                        }
                    }
                }
                else if (nDexType == ATOM_TYPE_DEX_TAKE)
                {
                    CTransaction txOffer;
                    if (txdb.ReadDiskTx(dexTake.hashOffer, txOffer))
                    {
                        unsigned char nOfferType = 0;
                        CAtomDexOffer origOffer;
                        CAtomDexCancel ign1;
                        CAtomDexTake ign2;
                        if (GetDexPayload(txOffer, nOfferType, origOffer, ign1, ign2) &&
                            nOfferType == ATOM_TYPE_DEX_OFFER)
                        {
                            int64 nBalTaker = 0;
                            uint32_t nNonTaker = 0;
                            txdb.ReadAtomBalance(dexTake.addrFrom, nBalTaker, nNonTaker);
                            nBalTaker -= origOffer.nAtomAmount;
                            if (nBalTaker < 0) nBalTaker = 0;
                            txdb.WriteAtomBalance(dexTake.addrFrom, nBalTaker, nNonTaker);

                            CDexOrderEntry entry;
                            entry.hashTx      = dexTake.hashOffer;
                            entry.addrMaker   = origOffer.addrFrom;
                            entry.nSide       = origOffer.nSide;
                            entry.nAtomAmount = origOffer.nAtomAmount;
                            entry.nPrice      = origOffer.nPrice;
                            entry.nNonce      = origOffer.nNonce;
                            entry.nHeight     = 0;
                            CTxIndex txindex;
                            if (txdb.ReadTxIndex(dexTake.hashOffer, txindex))
                            {
                                CBlock blk;
                                if (blk.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                                {
                                    std::map<uint256, CBlockIndex*>::iterator bi = mapBlockIndex.find(blk.GetHash());
                                    if (bi != mapBlockIndex.end())
                                        entry.nHeight = bi->second->nHeight;
                                }
                            }
                            txdb.WriteDexOrder(dexTake.hashOffer, entry);
                            CRITICAL_BLOCK(cs_mapDexOrders)
                            {
                                mapDexOrders[dexTake.hashOffer] = entry;
                            }
                        }
                    }
                }
            }
        }

        if (!vtx[i].DisconnectInputs(txdb))
            return false;
    }

    // ATOM coinbase reward reversal: undo the miner's ATOM credit for this block.
    if (pindex->nHeight >= ATOM_ACTIVATION_HEIGHT)
    {
        uint160 addrMiner;
        if (GetScriptAddr(vtx[0].vout[0].scriptPubKey, addrMiner))
        {
            uint256 coinbaseHash = vtx[0].GetHash();
            int nIgnHeight = 0; unsigned int nIgnTime = 0; unsigned char nIgnType = 0;
            uint160 ignFrom(0), ignTo(0); int64 nStoredReward = 0; uint32_t nIgnNonce = 0;
            if (txdb.ReadAtomTx(coinbaseHash, nIgnHeight, nIgnTime, nIgnType, ignFrom, ignTo, nStoredReward, nIgnNonce) && nStoredReward > 0)
            {
                int64 nMinerBal = 0;
                uint32_t nMinerNonce = 0;
                txdb.ReadAtomBalance(addrMiner, nMinerBal, nMinerNonce);
                nMinerBal -= nStoredReward;
                if (nMinerBal < 0) nMinerBal = 0;
                txdb.WriteAtomBalance(addrMiner, nMinerBal, nMinerNonce);

                int64 nTotalMinted = 0;
                txdb.ReadAtomTotalMinted(nTotalMinted);
                nTotalMinted -= nStoredReward;
                if (nTotalMinted < 0) nTotalMinted = 0;
                txdb.WriteAtomTotalMinted(nTotalMinted);

                txdb.EraseAtomTx(coinbaseHash);
                if (fUseIndexer && !fIndexerRebuilding)
                    txdb.EraseAtomAddrTx(addrMiner, coinbaseHash);
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        txdb.WriteBlockIndex(blockindexPrev);
    }

    if (fUseIndexer && !fIndexerRebuilding)
        txdb.WriteIndexerHeight(pindex->nHeight - 1);

    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    //// issue here: it doesn't know the version
    unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK) - 1 + GetSizeOfCompactSize(vtx.size());

    // Pass 1: validate ATOM balances and update DB state.
    // ATOM processing is only active at and above ATOM_ACTIVATION_HEIGHT.
    if (pindex->nHeight >= ATOM_ACTIVATION_HEIGHT)
    {
    int64 nTotalMinted = 0;
    txdb.ReadAtomTotalMinted(nTotalMinted);

    foreach(const CTransaction& tx, vtx)
    {
        uint256 txhash = tx.GetHash();
        CAtomTransfer atomTransfer;
        if (GetAtomTransfer(tx, atomTransfer))
        {
            if (atomTransfer.nType == ATOM_TYPE_COINBASE)
                return error("ConnectBlock() : ATOM COINBASE type rejected in user tx %s",
                             txhash.ToString().substr(0, 10).c_str());

            if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
            {
                uint160 bridgeAddr(std::vector<unsigned char>(ATOM_BRIDGE_HASH, ATOM_BRIDGE_HASH + 20));
                if (atomTransfer.addrFrom != bridgeAddr)
                    return error("ConnectBlock() : ATOM BRIDGE_TO_BITOK rejected: addrFrom is not bridge address in tx %s",
                                 txhash.ToString().substr(0, 10).c_str());

                if (nTotalMinted + atomTransfer.nAmount > ATOM_MAX_SUPPLY)
                    return error("ConnectBlock() : ATOM supply cap exceeded by bridge_to_bitok tx %s",
                                 txhash.ToString().substr(0, 10).c_str());
            }

            if (atomTransfer.nType == ATOM_TYPE_TRANSFER || atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL)
            {
                if (!VerifyAtomSender(tx, atomTransfer.addrFrom))
                    return error("ConnectBlock() : ATOM sender verification failed: no input signed by addrFrom %s in tx %s",
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 txhash.ToString().substr(0, 10).c_str());
            }

            if (atomTransfer.nType == ATOM_TYPE_TRANSFER)
            {
                int64 nBalFrom = 0;
                uint32_t nNonceFrom = 0;
                txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                if (atomTransfer.nNonce <= nNonceFrom)
                    return error("ConnectBlock() : ATOM nonce too low in tx %s: sender %s nonce %u <= confirmed %u",
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 atomTransfer.nNonce, nNonceFrom);
                if (atomTransfer.nNonce > nNonceFrom + ATOM_MAX_NONCE_GAP)
                    return error("ConnectBlock() : ATOM nonce gap too large in tx %s: sender %s nonce %u > confirmed %u + %u",
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 atomTransfer.nNonce, nNonceFrom, ATOM_MAX_NONCE_GAP);
                if (nBalFrom < atomTransfer.nAmount)
                    return error("ConnectBlock() : ATOM overdraft in tx %s: sender %s has %" PRI64d " but transfer is %" PRI64d,
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 nBalFrom, atomTransfer.nAmount);
                nBalFrom -= atomTransfer.nAmount;
                if (atomTransfer.nNonce > nNonceFrom)
                    nNonceFrom = atomTransfer.nNonce;
                if (!txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom))
                    return error("ConnectBlock() : WriteAtomBalance failed for sender %s tx %s",
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 txhash.ToString().substr(0, 10).c_str());
                if (fUseIndexer && !fIndexerRebuilding)
                    txdb.WriteAtomAddrTx(atomTransfer.addrFrom, txhash, pindex->nHeight);

                int64 nBalTo = 0;
                uint32_t nNonceTo = 0;
                txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                nBalTo += atomTransfer.nAmount;
                if (!txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo))
                    return error("ConnectBlock() : WriteAtomBalance failed for recipient %s tx %s",
                                 atomTransfer.addrTo.ToString().c_str(),
                                 txhash.ToString().substr(0, 10).c_str());
                if (fUseIndexer && !fIndexerRebuilding)
                    txdb.WriteAtomAddrTx(atomTransfer.addrTo, txhash, pindex->nHeight);
            }
            else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL)
            {
                int64 nBalFrom = 0;
                uint32_t nNonceFrom = 0;
                txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                if (atomTransfer.nNonce <= nNonceFrom)
                    return error("ConnectBlock() : ATOM nonce too low in burn tx %s: sender %s nonce %u <= confirmed %u",
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 atomTransfer.nNonce, nNonceFrom);
                if (atomTransfer.nNonce > nNonceFrom + ATOM_MAX_NONCE_GAP)
                    return error("ConnectBlock() : ATOM nonce gap too large in burn tx %s: sender %s nonce %u > confirmed %u + %u",
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 atomTransfer.nNonce, nNonceFrom, ATOM_MAX_NONCE_GAP);
                if (nBalFrom < atomTransfer.nAmount)
                    return error("ConnectBlock() : ATOM burn overdraft in tx %s: sender %s has %" PRI64d " but burn is %" PRI64d,
                                 txhash.ToString().substr(0, 10).c_str(),
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 nBalFrom, atomTransfer.nAmount);
                nBalFrom -= atomTransfer.nAmount;
                if (atomTransfer.nNonce > nNonceFrom)
                    nNonceFrom = atomTransfer.nNonce;
                if (!txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom))
                    return error("ConnectBlock() : WriteAtomBalance failed for burn sender %s tx %s",
                                 atomTransfer.addrFrom.ToString().c_str(),
                                 txhash.ToString().substr(0, 10).c_str());
                if (fUseIndexer && !fIndexerRebuilding)
                    txdb.WriteAtomAddrTx(atomTransfer.addrFrom, txhash, pindex->nHeight);
            }
            else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
            {
                int64 nBalTo = 0;
                uint32_t nNonceTo = 0;
                txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                nBalTo += atomTransfer.nAmount;
                if (!txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo))
                    return error("ConnectBlock() : WriteAtomBalance failed for bridge_to_bitok recipient %s tx %s",
                                 atomTransfer.addrTo.ToString().c_str(),
                                 txhash.ToString().substr(0, 10).c_str());
                if (fUseIndexer && !fIndexerRebuilding)
                    txdb.WriteAtomAddrTx(atomTransfer.addrTo, txhash, pindex->nHeight);

                nTotalMinted += atomTransfer.nAmount;
            }

            if (!txdb.WriteAtomTx(txhash, pindex->nHeight, pindex->nTime, atomTransfer.nType,
                                  atomTransfer.addrFrom, atomTransfer.addrTo,
                                  atomTransfer.nAmount, atomTransfer.nNonce,
                                  atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL ? atomTransfer.solAddrTo : NULL))
                return error("ConnectBlock() : WriteAtomTx failed for tx %s",
                             txhash.ToString().substr(0, 10).c_str());

            AtomMempoolRemove(txhash);
        }

        // DEX processing: handle offer/cancel/take operations
        if (pindex->nHeight >= ATOM_DEX_ACTIVATION_HEIGHT)
        {
            unsigned char nDexType = 0;
            CAtomDexOffer dexOffer;
            CAtomDexCancel dexCancel;
            CAtomDexTake dexTake;

            if (GetDexPayload(tx, nDexType, dexOffer, dexCancel, dexTake))
            {
                if (nDexType == ATOM_TYPE_DEX_OFFER)
                {
                    if (!VerifyAtomSender(tx, dexOffer.addrFrom))
                        return error("ConnectBlock() : DEX_OFFER sender verification failed in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    if (dexOffer.nSide != ATOM_DEX_SIDE_SELL_ATOM)
                        return error("ConnectBlock() : DEX_OFFER only sell-side supported in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    if (DexComputeBitokPayment(dexOffer.nAtomAmount, dexOffer.nPrice) <= 0)
                        return error("ConnectBlock() : DEX_OFFER total BITOK payment is zero in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    {
                        int64 nBalFrom = 0;
                        uint32_t nNonceFrom = 0;
                        txdb.ReadAtomBalance(dexOffer.addrFrom, nBalFrom, nNonceFrom);
                        if (dexOffer.nNonce <= nNonceFrom)
                            return error("ConnectBlock() : DEX_OFFER nonce too low in tx %s",
                                         txhash.ToString().substr(0, 10).c_str());
                        if (dexOffer.nNonce > nNonceFrom + ATOM_MAX_NONCE_GAP)
                            return error("ConnectBlock() : DEX_OFFER nonce gap too large in tx %s",
                                         txhash.ToString().substr(0, 10).c_str());
                        if (nBalFrom < dexOffer.nAtomAmount)
                            return error("ConnectBlock() : DEX_OFFER insufficient ATOM in tx %s",
                                         txhash.ToString().substr(0, 10).c_str());

                        nBalFrom -= dexOffer.nAtomAmount;
                        nNonceFrom = dexOffer.nNonce;
                        txdb.WriteAtomBalance(dexOffer.addrFrom, nBalFrom, nNonceFrom);
                    }

                    CDexOrderEntry entry;
                    entry.hashTx      = txhash;
                    entry.addrMaker   = dexOffer.addrFrom;
                    entry.nSide       = dexOffer.nSide;
                    entry.nAtomAmount = dexOffer.nAtomAmount;
                    entry.nPrice      = dexOffer.nPrice;
                    entry.nNonce      = dexOffer.nNonce;
                    entry.nHeight     = pindex->nHeight;
                    entry.nTime       = pindex->nTime;

                    txdb.WriteDexOrder(txhash, entry);

                    CRITICAL_BLOCK(cs_mapDexOrders)
                    {
                        mapDexOrders[txhash] = entry;
                    }

                    DexMempoolRemove(txhash);
                }
                else if (nDexType == ATOM_TYPE_DEX_CANCEL)
                {
                    if (!VerifyAtomSender(tx, dexCancel.addrFrom))
                        return error("ConnectBlock() : DEX_CANCEL sender verification failed in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    CDexOrderEntry cancelledOrder;
                    bool fFound = false;
                    CRITICAL_BLOCK(cs_mapDexOrders)
                    {
                        std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(dexCancel.hashOffer);
                        if (it != mapDexOrders.end())
                        {
                            if (it->second.addrMaker != dexCancel.addrFrom)
                                return error("ConnectBlock() : DEX_CANCEL maker mismatch in tx %s",
                                             txhash.ToString().substr(0, 10).c_str());
                            cancelledOrder = it->second;
                            fFound = true;
                        }
                    }

                    if (!fFound)
                        return error("ConnectBlock() : DEX_CANCEL references non-existent offer in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    {
                        int64 nBal = 0;
                        uint32_t nNon = 0;
                        txdb.ReadAtomBalance(cancelledOrder.addrMaker, nBal, nNon);
                        nBal += cancelledOrder.nAtomAmount;
                        txdb.WriteAtomBalance(cancelledOrder.addrMaker, nBal, nNon);

                        txdb.EraseDexOrder(dexCancel.hashOffer);

                        CRITICAL_BLOCK(cs_mapDexOrders)
                        {
                            mapDexOrders.erase(dexCancel.hashOffer);
                        }
                    }

                    DexMempoolRemove(txhash);
                }
                else if (nDexType == ATOM_TYPE_DEX_TAKE)
                {
                    if (!VerifyAtomSender(tx, dexTake.addrFrom))
                        return error("ConnectBlock() : DEX_TAKE sender verification failed in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    CDexOrderEntry takenOrder;
                    bool fFound = false;
                    CRITICAL_BLOCK(cs_mapDexOrders)
                    {
                        std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(dexTake.hashOffer);
                        if (it != mapDexOrders.end())
                        {
                            if (it->second.addrMaker == dexTake.addrFrom)
                                return error("ConnectBlock() : DEX_TAKE taker is maker in tx %s",
                                             txhash.ToString().substr(0, 10).c_str());
                            takenOrder = it->second;
                            fFound = true;
                        }
                    }

                    if (!fFound)
                        return error("ConnectBlock() : DEX_TAKE references non-existent offer in tx %s",
                                     txhash.ToString().substr(0, 10).c_str());

                    {
                        int64 nRequiredBitok = DexComputeBitokPayment(takenOrder.nAtomAmount, takenOrder.nPrice);
                        if (nRequiredBitok <= 0)
                            return error("ConnectBlock() : DEX_TAKE computed BITOK payment is zero in tx %s",
                                         txhash.ToString().substr(0, 10).c_str());
                        int64 nPaidToMaker = 0;
                        for (unsigned int vo = 0; vo < tx.vout.size(); vo++)
                        {
                            uint160 addrOut;
                            if (GetScriptAddr(tx.vout[vo].scriptPubKey, addrOut) &&
                                addrOut == takenOrder.addrMaker)
                                nPaidToMaker += tx.vout[vo].nValue;
                        }
                        if (nPaidToMaker < nRequiredBitok)
                            return error("ConnectBlock() : DEX_TAKE insufficient BITOK payment: paid %" PRI64d " need %" PRI64d " in tx %s",
                                         nPaidToMaker, nRequiredBitok,
                                         txhash.ToString().substr(0, 10).c_str());

                        int64 nBalTaker = 0;
                        uint32_t nNonceTaker = 0;
                        txdb.ReadAtomBalance(dexTake.addrFrom, nBalTaker, nNonceTaker);
                        nBalTaker += takenOrder.nAtomAmount;
                        txdb.WriteAtomBalance(dexTake.addrFrom, nBalTaker, nNonceTaker);
                    }

                    txdb.EraseDexOrder(dexTake.hashOffer);

                    CRITICAL_BLOCK(cs_mapDexOrders)
                    {
                        mapDexOrders.erase(dexTake.hashOffer);
                    }

                    DexMempoolRemove(txhash);
                }
            }
        }
    }

    txdb.WriteAtomTotalMinted(nTotalMinted);
    } // end ATOM_ACTIVATION_HEIGHT guard

    // Pass 2: script validation (ConnectInputs) and UTXO indexer.
    map<uint256, CTxIndex> mapUnused;
    int64 nFees = 0;
    foreach(CTransaction& tx, vtx)
    {
        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        nTxPos += ::GetSerializeSize(tx, SER_DISK);

        if (!tx.ConnectInputs(txdb, mapUnused, posThisTx, pindex->nHeight, nFees, true, false))
            return false;

        uint256 txhash = tx.GetHash();
        bool fCoinBase = tx.IsCoinBase();

        if (fUseIndexer && !fIndexerRebuilding)
        {
            // Index each output as a UTXO
            for (unsigned int n = 0; n < tx.vout.size(); n++)
            {
                const CTxOut& txout = tx.vout[n];
                uint160 addr;
                if (GetScriptAddr(txout.scriptPubKey, addr))
                {
                    txdb.WriteAddrOut(addr, txhash, n, txout.nValue, pindex->nHeight, fCoinBase);
                    txdb.WriteAddrTx(addr, txhash, pindex->nHeight);
                }
            }

            // Spend inputs: erase their addrout entries (they are no longer UTXOs)
            // and link the spending tx to the sender's address
            if (!fCoinBase)
            {
                for (unsigned int k = 0; k < tx.vin.size(); k++)
                {
                    const COutPoint& prevout = tx.vin[k].prevout;

                    CTransaction txPrev;
                    if (txdb.ReadDiskTx(prevout.hash, txPrev))
                    {
                        if (prevout.n < txPrev.vout.size())
                        {
                            uint160 addrSender;
                            if (GetScriptAddr(txPrev.vout[prevout.n].scriptPubKey, addrSender))
                            {
                                txdb.EraseAddrOut(addrSender, prevout.hash, prevout.n);
                                txdb.WriteAddrTx(addrSender, txhash, pindex->nHeight);
                            }
                        }
                    }
                }
            }
        }
    }

    if (vtx[0].GetValueOut() > GetBlockValue(nFees))
        return false;

    // ATOM coinbase reward: credit 20 ATOM per 1 BITOK of block subsidy to the miner.
    if (pindex->nHeight >= ATOM_ACTIVATION_HEIGHT)
    {
        uint160 addrMiner;
        if (GetScriptAddr(vtx[0].vout[0].scriptPubKey, addrMiner))
        {
            int64 nSubsidy = (50 * COIN) >> (pindex->nHeight / 210000);
            int64 nAtomReward = (nSubsidy / COIN) * ATOM_PER_BITOK;
            if (nAtomReward > 0)
            {
                int64 nTotalMinted = 0;
                txdb.ReadAtomTotalMinted(nTotalMinted);
                if (nTotalMinted + nAtomReward > ATOM_MAX_SUPPLY)
                    nAtomReward = ATOM_MAX_SUPPLY - nTotalMinted;

                if (nAtomReward > 0)
                {
                    int64 nMinerBal = 0;
                    uint32_t nMinerNonce = 0;
                    txdb.ReadAtomBalance(addrMiner, nMinerBal, nMinerNonce);
                    nMinerBal += nAtomReward;

                    uint256 coinbaseHash = vtx[0].GetHash();
                    if (!txdb.WriteAtomBalance(addrMiner, nMinerBal, nMinerNonce))
                        return error("ConnectBlock() : WriteAtomBalance failed for ATOM coinbase reward at height %d", pindex->nHeight);
                    if (!txdb.WriteAtomTx(coinbaseHash, pindex->nHeight, pindex->nTime, ATOM_TYPE_COINBASE,
                                          uint160(0), addrMiner, nAtomReward, 0))
                        return error("ConnectBlock() : WriteAtomTx failed for ATOM coinbase at height %d", pindex->nHeight);
                    if (fUseIndexer && !fIndexerRebuilding)
                        txdb.WriteAtomAddrTx(addrMiner, coinbaseHash, pindex->nHeight);

                    nTotalMinted += nAtomReward;
                    txdb.WriteAtomTotalMinted(nTotalMinted);
                }
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        txdb.WriteBlockIndex(blockindexPrev);
    }

    if (fUseIndexer && !fIndexerRebuilding)
        txdb.WriteIndexerHeight(pindex->nHeight);

    // Watch for transactions paying to me
    foreach(CTransaction& tx, vtx)
        AddToWalletIfMine(tx, this);

    return true;
}



bool ReindexUTXOs()
{
    printf("ReindexUTXOs: starting full reindex of UTXO and address indexes\n");

    if (pindexGenesisBlock == NULL)
    {
        printf("ReindexUTXOs: no genesis block, nothing to index\n");
        fIndexerRebuilding = false;
        return true;
    }

    CTxDB txdb;

    txdb.TxnBegin();
    if (!txdb.EraseAllIndexerData())
    {
        fIndexerRebuilding = false;
        return error("ReindexUTXOs: EraseAllIndexerData failed");
    }
    txdb.TxnCommit();

    CBlockIndex* pindex = pindexGenesisBlock;
    int nProcessed = 0;
    const int nBatchSize = 500;

    txdb.TxnBegin();

    while (pindex)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos))
        {
            txdb.TxnAbort();
            fIndexerRebuilding = false;
            printf("ReindexUTXOs: ReadFromDisk failed at height %d\n", pindex->nHeight);
            return false;
        }

        foreach(const CTransaction& tx, block.vtx)
        {
            uint256 txhash = tx.GetHash();
            bool fCoinBase = tx.IsCoinBase();

            for (unsigned int n = 0; n < tx.vout.size(); n++)
            {
                const CTxOut& txout = tx.vout[n];
                uint160 addr;
                if (GetScriptAddr(txout.scriptPubKey, addr))
                {
                    txdb.WriteAddrOut(addr, txhash, n, txout.nValue, pindex->nHeight, fCoinBase);
                    txdb.WriteAddrTx(addr, txhash, pindex->nHeight);
                }
            }

            if (!fCoinBase)
            {
                for (unsigned int k = 0; k < tx.vin.size(); k++)
                {
                    const COutPoint& prevout = tx.vin[k].prevout;

                    CTransaction txPrev;
                    if (txdb.ReadDiskTx(prevout.hash, txPrev))
                    {
                        if (prevout.n < txPrev.vout.size())
                        {
                            uint160 addrSender;
                            if (GetScriptAddr(txPrev.vout[prevout.n].scriptPubKey, addrSender))
                            {
                                txdb.EraseAddrOut(addrSender, prevout.hash, prevout.n);
                                txdb.WriteAddrTx(addrSender, txhash, pindex->nHeight);
                            }
                        }
                    }
                }
            }
        }

        txdb.WriteIndexerHeight(pindex->nHeight);
        nProcessed++;

        if (nProcessed % nBatchSize == 0)
        {
            txdb.TxnCommit();
            printf("ReindexUTXOs: processed %d blocks (height %d)\n", nProcessed, pindex->nHeight);
            txdb.TxnBegin();
        }

        pindex = pindex->pnext;
    }

    txdb.TxnCommit();
    txdb.Close();

    fIndexerRebuilding = false;
    printf("ReindexUTXOs: completed, indexed %d blocks\n", nProcessed);

    printf("ReindexUTXOs: starting automatic ATOM rescan\n");
    if (!RescanAtom())
        return error("ReindexUTXOs: RescanAtom failed");

    return true;
}

bool RescanAtom(boost::function<bool (int, int)> progressCallback)
{
    printf("RescanAtom: starting full rescan of ATOM balances and transaction history\n");
    fflush(stdout);

    if (pindexGenesisBlock == NULL)
    {
        printf("[ATOM] RescanAtom: ERROR - no genesis block found, chain not loaded yet!\n");
        fflush(stdout);
        return true;
    }
    printf("[ATOM] RescanAtom: genesis block found, starting scan...\n");
    fflush(stdout);

    CTxDB txdb;

    printf("[ATOM] RescanAtom: erasing old ATOM data...\n");
    fflush(stdout);
    txdb.TxnBegin();
    if (!txdb.EraseAllAtomData())
    {
        txdb.TxnAbort();
        printf("[ATOM] RescanAtom: ERROR - EraseAllAtomData failed!\n");
        fflush(stdout);
        return error("RescanAtom: EraseAllAtomData failed");
    }
    txdb.TxnCommit();
    printf("[ATOM] RescanAtom: old data erased OK\n");
    fflush(stdout);

    int nTotalBlocks = 0;
    for (CBlockIndex* p = pindexGenesisBlock; p; p = p->pnext)
        nTotalBlocks++;
    printf("[ATOM] RescanAtom: total blocks to scan: %d\n", nTotalBlocks);
    fflush(stdout);

    // On a regular node, atomaddrtx is only written for wallet addresses.
    // On an indexer node, atomaddrtx is written for every address in every block.
    std::set<uint160> setWalletAddrs;
    if (!fUseIndexer)
    {
        CRITICAL_BLOCK(cs_mapKeys)
        {
            for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
                 it != mapKeys.end(); ++it)
                setWalletAddrs.insert(Hash160(it->first));
        }
    }

    CBlockIndex* pindex = pindexGenesisBlock;
    int nProcessed = 0;
    int nLastPct = -1;
    const int nBatchSize = 500;
    int64 nTotalMinted = 0;

    txdb.TxnBegin();

    while (pindex)
    {
        if (pindex->nHeight < ATOM_ACTIVATION_HEIGHT)
        {
            nProcessed++;
            pindex = pindex->pnext;
            continue;
        }

        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos))
        {
            txdb.TxnAbort();
            printf("RescanAtom: ReadFromDisk failed at height %d\n", pindex->nHeight);
            return false;
        }

        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            const CTransaction& tx = block.vtx[i];
            uint256 txhash = tx.GetHash();

            CAtomTransfer atomTransfer;
            if (GetAtomTransfer(tx, atomTransfer))
            {
                if (atomTransfer.nType == ATOM_TYPE_COINBASE)
                {
                    printf("[ATOM] RescanAtom: skipping invalid COINBASE user tx %s at height %d\n",
                           txhash.ToString().substr(0, 10).c_str(), pindex->nHeight);
                    continue;
                }
                if (atomTransfer.nType == ATOM_TYPE_TRANSFER)
                {
                    int64 nBalFrom = 0;
                    uint32_t nNonceFrom = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    nBalFrom -= atomTransfer.nAmount;
                    if (nBalFrom < 0) nBalFrom = 0;
                    if (atomTransfer.nNonce > nNonceFrom)
                        nNonceFrom = atomTransfer.nNonce;
                    txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    if (fUseIndexer || setWalletAddrs.count(atomTransfer.addrFrom))
                        txdb.WriteAtomAddrTx(atomTransfer.addrFrom, txhash, pindex->nHeight);

                    int64 nBalTo = 0;
                    uint32_t nNonceTo = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    nBalTo += atomTransfer.nAmount;
                    txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    if (fUseIndexer || setWalletAddrs.count(atomTransfer.addrTo))
                        txdb.WriteAtomAddrTx(atomTransfer.addrTo, txhash, pindex->nHeight);
                }
                else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                {
                    int64 nBalFrom = 0;
                    uint32_t nNonceFrom = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    nBalFrom -= atomTransfer.nAmount;
                    if (nBalFrom < 0) nBalFrom = 0;
                    if (atomTransfer.nNonce > nNonceFrom)
                        nNonceFrom = atomTransfer.nNonce;
                    txdb.WriteAtomBalance(atomTransfer.addrFrom, nBalFrom, nNonceFrom);
                    if (fUseIndexer || setWalletAddrs.count(atomTransfer.addrFrom))
                        txdb.WriteAtomAddrTx(atomTransfer.addrFrom, txhash, pindex->nHeight);
                }
                else if (atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
                {
                    int64 nBalTo = 0;
                    uint32_t nNonceTo = 0;
                    txdb.ReadAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    nBalTo += atomTransfer.nAmount;
                    txdb.WriteAtomBalance(atomTransfer.addrTo, nBalTo, nNonceTo);
                    if (fUseIndexer || setWalletAddrs.count(atomTransfer.addrTo))
                        txdb.WriteAtomAddrTx(atomTransfer.addrTo, txhash, pindex->nHeight);

                    nTotalMinted += atomTransfer.nAmount;
                }

                txdb.WriteAtomTx(txhash, pindex->nHeight, pindex->nTime, atomTransfer.nType,
                                 atomTransfer.addrFrom, atomTransfer.addrTo,
                                 atomTransfer.nAmount, atomTransfer.nNonce,
                                 atomTransfer.nType == ATOM_TYPE_BRIDGE_TO_SOL ? atomTransfer.solAddrTo : NULL);
            }
        }

        // Coinbase ATOM reward
        {
            uint160 addrMiner;
            if (GetScriptAddr(block.vtx[0].vout[0].scriptPubKey, addrMiner))
            {
                int64 nSubsidy = (50 * COIN) >> (pindex->nHeight / 210000);
                int64 nAtomReward = (nSubsidy / COIN) * ATOM_PER_BITOK;
                if (nAtomReward > 0)
                {
                    if (nTotalMinted + nAtomReward > ATOM_MAX_SUPPLY)
                        nAtomReward = ATOM_MAX_SUPPLY - nTotalMinted;

                    if (nAtomReward > 0)
                    {
                    int64 nMinerBal = 0;
                    uint32_t nMinerNonce = 0;
                    txdb.ReadAtomBalance(addrMiner, nMinerBal, nMinerNonce);
                    nMinerBal += nAtomReward;
                    txdb.WriteAtomBalance(addrMiner, nMinerBal, nMinerNonce);

                    uint256 coinbaseHash = block.vtx[0].GetHash();
                    txdb.WriteAtomTx(coinbaseHash, pindex->nHeight, pindex->nTime, ATOM_TYPE_COINBASE,
                                     uint160(0), addrMiner, nAtomReward, 0);
                    if (fUseIndexer || setWalletAddrs.count(addrMiner))
                        txdb.WriteAtomAddrTx(addrMiner, coinbaseHash, pindex->nHeight);

                    nTotalMinted += nAtomReward;
                    }
                }
            }
        }

        nProcessed++;

        if (nProcessed % nBatchSize == 0)
        {
            txdb.TxnCommit();
            txdb.TxnBegin();

            int nPct = (nProcessed * 100) / nTotalBlocks;
            if (nPct / 10 > nLastPct / 10 && nPct <= 100)
            {
                nLastPct = nPct;
                printf("[ATOM] RescanAtom progress: %d%% (%d/%d blocks, height %d)\n",
                       (nPct / 10) * 10, nProcessed, nTotalBlocks, pindex->nHeight);
                fflush(stdout);
            }
            if (progressCallback)
                progressCallback(nProcessed, nTotalBlocks);
        }

        pindex = pindex->pnext;
    }

    txdb.WriteAtomTotalMinted(nTotalMinted);
    txdb.TxnCommit();
    txdb.Close();

    printf("[ATOM] RescanAtom: COMPLETED, processed %d blocks, totalMinted=%" PRI64d "\n", nProcessed, nTotalMinted);
    fflush(stdout);
    return true;
}


bool Reorganize(CTxDB& txdb, CBlockIndex* pindexNew)
{
    if (fDebug)
        printf("[BLOCK] REORGANIZE\n");

    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
    {
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
    }

    // Reorganize is costly in terms of db load, as it works in a single db transaction.
    // Try to limit how much needs to be done inside
    CBlockIndex* pindexLastCommon = pfork;
    if (pindexLastCommon)
    {
        CBlockIndex* pindex = Checkpoints::GetLastCheckpoint(mapBlockIndex);
        if (pindex && pindexLastCommon->nHeight < pindex->nHeight)
            return error("Reorganize() : reorganize past last checkpoint %d", pindex->nHeight);
    }

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);

    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    // Disconnect shorter branch
    vector<CTransaction> vResurrect;
    foreach(CBlockIndex* pindex, vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock failed");

        // Queue memory transactions to resurrect
        foreach(const CTransaction& tx, block.vtx)
            if (!tx.IsCoinBase())
                vResurrect.push_back(tx);
    }

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (int i = 0; i < vConnect.size(); i++)
    {
        CBlockIndex* pindex = vConnect[i];
        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex))
        {
            // Invalid block, delete the rest of this branch
            txdb.TxnAbort();
            for (int j = i; j < vConnect.size(); j++)
            {
                CBlockIndex* pindex = vConnect[j];
                pindex->EraseBlockFromDisk();
                txdb.EraseBlockIndex(pindex->GetBlockHash());
                mapBlockIndex.erase(pindex->GetBlockHash());
                delete pindex;
            }
            return error("Reorganize() : ConnectBlock failed");
        }

        // Queue memory transactions to delete
        foreach(const CTransaction& tx, block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Commit now because resurrecting could take some time
    txdb.TxnCommit();

    // Disconnect shorter branch
    foreach(CBlockIndex* pindex, vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = NULL;

    // Connect longer branch
    foreach(CBlockIndex* pindex, vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    foreach(CTransaction& tx, vResurrect)
        tx.AcceptTransaction(txdb, false);

    // Delete redundant memory transactions that are in the connected branch
    foreach(CTransaction& tx, vDelete)
        tx.RemoveFromMemoryPool();

    return true;
}


bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos)
{
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,16).c_str());

    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");

    auto mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    auto miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // Check against checkpoints
    if (!Checkpoints::CheckBlock(pindexNew->nHeight, hash))
        return error("AddToBlockIndex() : rejected by checkpoint lockin at %d", pindexNew->nHeight);

    CTxDB txdb;
    txdb.TxnBegin();
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

    if (pindexNew->nHeight > nBestHeight)
    {
        if (pindexGenesisBlock == NULL && hash == hashGenesisBlock)
        {
            pindexGenesisBlock = pindexNew;
            txdb.WriteHashBestChain(hash);
        }
        else if (hashPrevBlock == hashBestChain)
        {
            if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
            {
                txdb.TxnAbort();
                pindexNew->EraseBlockFromDisk();
                mapBlockIndex.erase(pindexNew->GetBlockHash());
                delete pindexNew;
                return error("AddToBlockIndex() : ConnectBlock failed");
            }
            txdb.TxnCommit();

            if (pindexNew->pprev)
                pindexNew->pprev->pnext = pindexNew;
            else
                pindexGenesisBlock = pindexNew;

            foreach(CTransaction& tx, vtx)
                tx.RemoveFromMemoryPool();
        }
        else
        {
            if (!Reorganize(txdb, pindexNew))
            {
                txdb.TxnAbort();
                return error("AddToBlockIndex() : Reorganize failed");
            }
        }

        hashBestChain = hash;
        pindexBest = pindexNew;
        nBestHeight = pindexBest->nHeight;
        nTimeBestReceived = GetTime();
        nTransactionsUpdated++;
        if (fDebug)
            printf("[BLOCK] AddToBlockIndex: new best=%s height=%d\n", hashBestChain.ToString().substr(0,16).c_str(), nBestHeight);
    }

    txdb.TxnCommit();
    txdb.Close();

    if (pindexNew == pindexBest)
    {
        static uint256 hashPrevBestCoinBase;
        CRITICAL_BLOCK(cs_mapWallet)
            vWalletUpdated.push_back(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    MainFrameRepaint();
    return true;
}




bool CBlock::CheckBlock() const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("CheckBlock() : size limits failed");

    // Check timestamp
    if (nTime > GetAdjustedTime() + 2 * 60 * 60)
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return error("CheckBlock() : first tx is not coinbase");
    for (int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return error("CheckBlock() : more than one coinbase");

    // Check transactions
    unsigned int nSigOps = 0;
    foreach(const CTransaction& tx, vtx)
    {
        if (!tx.CheckTransaction())
            return error("CheckBlock() : CheckTransaction failed");
        foreach(const CTxIn& txin, tx.vin)
            nSigOps += GetSigOpCount(txin.scriptSig);
        foreach(const CTxOut& txout, tx.vout)
            nSigOps += GetSigOpCount(txout.scriptPubKey);
    }
    if (nSigOps > MAX_SIGOPS_PER_BLOCK)
        return error("CheckBlock() : too many sigops");

    // Check proof of work matches claimed amount
    if (CBigNum().SetCompact(nBits) > bnProofOfWorkLimit)
        return error("CheckBlock() : nBits below minimum work");
    if (GetPoWHash() > CBigNum().SetCompact(nBits).getuint256())
        return error("CheckBlock() : hash doesn't match nBits");

    // Check merkleroot
    if (hashMerkleRoot != BuildMerkleTree())
        return error("CheckBlock() : hashMerkleRoot mismatch");

    return true;
}

bool CBlock::AcceptBlock()
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    auto mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return error("AcceptBlock() : prev block not found");
    CBlockIndex* pindexPrev = (*mi).second;

    // Check timestamp against prev
    if (nTime <= pindexPrev->GetMedianTimePast())
        return error("AcceptBlock() : block's timestamp is too early");

    // Time warp attack mitigation (soft-fork)
    // Activates at block TIMEWARP_ACTIVATION_HEIGHT
    // Before activation: warnings only
    // After activation: strict enforcement (rejects invalid blocks)
    const unsigned int nTargetSpacing = 10 * 60;
    const unsigned int nMaxTimestampDrift = 7200;
    int nNextHeight = pindexPrev->nHeight + 1;
    bool fEnforceTimewarp = (nNextHeight >= TIMEWARP_ACTIVATION_HEIGHT);

    if (nTime > pindexPrev->nTime + nMaxTimestampDrift + nTargetSpacing * 6)
    {
        printf("WARNING: Block timestamp %u is far ahead of parent %u (diff=%u)\n",
               nTime, pindexPrev->nTime, nTime - pindexPrev->nTime);
        if (fEnforceTimewarp)
            return error("AcceptBlock() : block timestamp too far ahead (time warp protection)");
    }

    // At difficulty adjustment boundaries, apply stricter validation
    const unsigned int nTargetTimespan = 14 * 24 * 60 * 60;
    const unsigned int nInterval = nTargetTimespan / nTargetSpacing;
    if (nNextHeight % nInterval == 0)
    {
        const CBlockIndex* pindexFirst = pindexPrev;
        for (int i = 0; pindexFirst && i < (int)(nInterval - 1); i++)
            pindexFirst = pindexFirst->pprev;

        if (pindexFirst)
        {
            int64 nActualTimespan = (int64)nTime - (int64)pindexFirst->nTime;
            int64 nMinReasonableTimespan = (int64)nTargetTimespan / 8;

            if (nActualTimespan < nMinReasonableTimespan)
            {
                printf("WARNING: Difficulty adjustment timespan %lld is suspiciously short (min=%lld)\n",
                       nActualTimespan, nMinReasonableTimespan);
                if (fEnforceTimewarp)
                    return error("AcceptBlock() : timespan too short (time warp protection)");
            }
        }
    }

    // Check that all transactions are finalized
    foreach(const CTransaction& tx, vtx)
        if (!tx.IsFinal(nTime))
            return error("AcceptBlock() : contains a non-final transaction");

    // Check proof of work
    if (nBits != GetNextWorkRequired(pindexPrev))
        return error("AcceptBlock() : incorrect proof of work");

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile;
    unsigned int nBlockPos;
    if (!WriteToDisk(!fClient, nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    if (hashBestChain == hash)
    {
        if (fDebug)
            printf("[BLOCK] New best block %s at height %d\n", hash.ToString().substr(0,16).c_str(), nBestHeight);
        CRITICAL_BLOCK(cs_vNodes)
            foreach(CNode* pnode, vNodes)
                if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : 55000))
                    pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    return true;
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().substr(0,16).c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().substr(0,16).c_str());

    // Preliminary checks
    if (!pblock->CheckBlock())
    {
        delete pblock;
        return error("ProcessBlock() : CheckBlock FAILED");
    }

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        if (fDebug)
            printf("[BLOCK] Orphan block, prev=%s\n", pblock->hashPrevBlock.ToString().substr(0,16).c_str());
        mapOrphanBlocks.insert(make_pair(hash, pblock));
        mapOrphanBlocksByPrev.insert(make_pair(pblock->hashPrevBlock, pblock));

        // Ask this guy to fill in what we're missing
        if (pfrom)
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock));
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
    {
        delete pblock;
        return error("ProcessBlock() : AcceptBlock FAILED");
    }
    delete pblock;

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock())
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    if (fDebug)
        printf("[BLOCK] ProcessBlock: ACCEPTED\n");
    return true;
}








template<typename Stream>
bool ScanMessageStart(Stream& s)
{
    // Scan ahead to the next pchMessageStart, which should normally be immediately
    // at the file pointer.  Leaves file pointer at end of pchMessageStart.
    s.clear(0);
    short prevmask = s.exceptions(0);
    const char* p = BEGIN(pchMessageStart);
    try
    {
        loop
        {
            char c;
            s.read(&c, 1);
            if (s.fail())
            {
                s.clear(0);
                s.exceptions(prevmask);
                return false;
            }
            if (*p != c)
                p = BEGIN(pchMessageStart);
            if (*p == c)
            {
                if (++p == END(pchMessageStart))
                {
                    s.clear(0);
                    s.exceptions(prevmask);
                    return true;
                }
            }
        }
    }
    catch (...)
    {
        s.clear(0);
        s.exceptions(prevmask);
        return false;
    }
}

bool CheckDiskSpace(int64 nAdditionalBytes)
{
    uint64 nFreeBytesAvailable = 0;

#if defined(_WIN32) || defined(__MINGW32__)
    ULARGE_INTEGER freeBytesAvailable;
    if (GetDiskFreeSpaceExA(GetDataDir().c_str(), &freeBytesAvailable, NULL, NULL))
        nFreeBytesAvailable = freeBytesAvailable.QuadPart;
#else
    nFreeBytesAvailable = boost::filesystem::space(GetDataDir()).available;
#endif

    // Check for 15MB because database could create another 10MB log file at any time
    if (nFreeBytesAvailable < (int64)15000000 + nAdditionalBytes)
    {
        fShutdown = true;
        ThreadSafeMessageBox(_STR("Warning: Disk space is low  "), "Bitcoin", wxOK | wxICON_EXCLAMATION);
        CreateThread(Shutdown, NULL);
        return false;
    }
    return true;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if (nFile == -1)
        return NULL;
    FILE* file = fopen(strprintf("%s/blk%04d.dat", GetDataDir().c_str(), nFile).c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    loop
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 filesize max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < 0x7F000000 - MAX_SIZE)
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}

//
// Multi-threaded Genesis Mining
//

struct GenesisData {
    CBlock block;
    uint256 hashTarget;
    volatile bool fFound;
    volatile unsigned int nSolution;
    volatile unsigned int nTimeSolution;
    volatile int nActiveThreads;
    CCriticalSection cs;
};

static GenesisData* pGenesisData = NULL;

void InitSHA256();
void ThreadGenesisMiner(void* parg);


bool LoadBlockIndex(bool fAllowNew)
{
    //
    // Load block index
    //
    CTxDB txdb("cr");
    if (!txdb.LoadBlockIndex())
        return false;
    txdb.Close();

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;


        // Genesis block
        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        CBigNum bnPubKey;
        bnPubKey.SetHex("0x5F1DF16B2B704C8A578D0BBAF74D385CDE12C11EE50455F3C438EF4C3FBCF649B6DE611FEAE06279A60939E028A8D65C10B73071A6F16719274855FEB0FD8A6704");
        txNew.vout[0].scriptPubKey = CScript() << bnPubKey << OP_CHECKSIG;
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
        block.nTime    = 1231006505;
        block.nBits    = 0x1effffff; // Match bnProofOfWorkLimit (~uint256(0) >> 17)
        block.nNonce   = 37137;

        // Mine the genesis block with multi-threading
        if (hashGenesisBlock == 0)
        {
            printf("Mining genesis block...\n");

            // Initialize optimized SHA-256
            InitSHA256();

            // Determine number of threads to use
#if wxUSE_GUI
            int nThreads = wxThread::GetCPUCount();
#elif defined(_WIN32) || defined(__MINGW32__)
            SYSTEM_INFO sysinfo;
            GetSystemInfo(&sysinfo);
            int nThreads = sysinfo.dwNumberOfProcessors;
#else
            int nThreads = sysconf(_SC_NPROCESSORS_ONLN);
#endif
            if (nThreads < 1)
                nThreads = 1;
            if (fLimitProcessors && nThreads > nLimitProcessors)
                nThreads = nLimitProcessors;

            printf("Using %d mining threads", nThreads);
            if (fLimitProcessors)
                printf(" (-genproclimit=%d)\n", nLimitProcessors);
            else
                printf(" (all CPUs)\n");
            fflush(stdout);

            // Setup shared genesis data
            GenesisData genesis;
            genesis.block = block;
            genesis.hashTarget = CBigNum().SetCompact(block.nBits).getuint256();
            genesis.fFound = false;
            genesis.nSolution = 0;
            genesis.nTimeSolution = block.nTime;
            genesis.nActiveThreads = 0;
            pGenesisData = &genesis;

            // Start mining threads
            int64 nStartTime = GetTimeMillis();
            for (int i = 0; i < nThreads; i++)
            {
                if (!CreateThread(ThreadGenesisMiner, (void*)(long)i))
                    printf("Error: CreateThread(ThreadGenesisMiner) failed for thread %d\n", i);
                Sleep(10);
            }

            // Wait for solution
            while (!genesis.fFound)
                Sleep(100);

            while (genesis.nActiveThreads > 0)
                Sleep(100);

            block.nNonce = genesis.nSolution;
            block.nTime = genesis.nTimeSolution;

            int64 nElapsed = GetTimeMillis() - nStartTime;
            printf("\nGenesis block mined in %d ms!\n", (int)nElapsed);
            printf("nNonce = %u\n", block.nNonce);
            printf("nTime = %u\n", block.nTime);
            printf("Hash = %s\n", block.GetHash().ToString().c_str());
            printf("Merkle = %s\n", block.hashMerkleRoot.ToString().c_str());

            pGenesisData = NULL;
        }

        printf("%s\n", block.GetHash().ToString().c_str());
        printf("%s\n", block.hashMerkleRoot.ToString().c_str());
        printf("%s\n", hashGenesisBlock.ToString().c_str());
        txNew.vout[0].scriptPubKey.print();
        block.print();

        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(!fClient, nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");

        if (!block.AddToBlockIndex(nFile, nBlockPos))
            return error("LoadBlockIndex() : genesis block not accepted");
    }

    return true;
}



void PrintBlockTree()
{
    // precompute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
        // test
        //while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex);
        printf("%d (%u,%u) %s  %s  tx %d",
            pindex->nHeight,
            pindex->nFile,
            pindex->nBlockPos,
            block.GetHash().ToString().substr(0,16).c_str(),
            DateTimeStrFormat("%x %H:%M:%S", block.nTime).c_str(),
            block.vtx.size());

        CRITICAL_BLOCK(cs_mapWallet)
        {
            if (mapWallet.count(block.vtx[0].GetHash()))
            {
                CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
                printf("    mine:  %d  %d  %d", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
            }
        }
        printf("\n");


        // put the main timechain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}










//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool AlreadyHave(CTxDB& txdb, const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:             return mapTransactions.count(inv.hash) || mapOrphanTransactions.count(inv.hash) || txdb.ContainsTx(inv.hash);
    case MSG_BLOCK:
    case MSG_FILTERED_BLOCK: return mapBlockIndex.count(inv.hash) || mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}







bool ProcessMessages(CNode* pfrom)
{
    CDataStream& vRecv = pfrom->vRecv;
    if (vRecv.empty())
        return true;
    //if (fDebug)
    //    printf("ProcessMessages(%d bytes)\n", vRecv.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //

    loop
    {
        // Scan for message start
        CDataStream::iterator pstart = search(vRecv.begin(), vRecv.end(), BEGIN(pchMessageStart), END(pchMessageStart));
        int nHeaderSize = vRecv.GetSerializeSize(CMessageHeader());
        if (vRecv.end() - pstart < nHeaderSize)
        {
            if (vRecv.size() > nHeaderSize)
            {
                printf("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
                vRecv.erase(vRecv.begin(), vRecv.end() - nHeaderSize);
            }
            break;
        }
        if (pstart - vRecv.begin() > 0)
            printf("\n\nPROCESSMESSAGE SKIPPED %d BYTES\n\n", pstart - vRecv.begin());
        vRecv.erase(vRecv.begin(), pstart);

        // Read header
        vector<char> vHeaderSave(vRecv.begin(), vRecv.begin() + nHeaderSize);
        CMessageHeader hdr;
        vRecv >> hdr;
        if (!hdr.IsValid())
        {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;
        if (nMessageSize > vRecv.size())
        {
            // Rewind and wait for rest of message
            ///// need a mechanism to give up waiting for overlong message size error
            vRecv.insert(vRecv.begin(), &vHeaderSave[0], &vHeaderSave[0] + vHeaderSave.size());
            break;
        }

        // Copy message to its own buffer
        CDataStream vMsg(vRecv.begin(), vRecv.begin() + nMessageSize, vRecv.nType, vRecv.nVersion);
        vRecv.ignore(nMessageSize);

        // Checksum
        if (vRecv.GetVersion() >= 209)
        {
            uint256 hash = Hash(vMsg.begin(), vMsg.end());
            unsigned int nChecksum = 0;
            memcpy(&nChecksum, &hash, sizeof(nChecksum));
            if (nChecksum != hdr.nChecksum)
            {
                printf("ProcessMessage(%s, %d bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
                       strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
                continue;
            }
        }

        // Process message
        bool fRet = false;
        try
        {
            CRITICAL_BLOCK(cs_main)
                fRet = ProcessMessage(pfrom, strCommand, vMsg);
            if (fShutdown)
                return true;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "CDataStream::read() : end of data"))
            {
                // Allow exceptions from underlength message on vRecv
                printf("ProcessMessage(%s, %d bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else if (strstr(e.what(), ": size too large"))
            {
                // Allow exceptions from overlong size
                printf("ProcessMessage(%s, %d bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else
            {
                PrintException(&e, "ProcessMessage()");
            }
        }
        catch (std::exception& e) {
            PrintException(&e, "ProcessMessage()");
        } catch (...) {
            PrintException(NULL, "ProcessMessage()");
        }

        if (!fRet)
            printf("ProcessMessage(%s, %d bytes) FAILED\n", strCommand.c_str(), nMessageSize);
    }

    vRecv.Compact();
    return true;
}




bool ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    static map<unsigned int, vector<unsigned char> > mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        printf("[MSG] %s from %s: %s (%d bytes)\n",
            DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str(),
            pfrom->addr.ToStringLog().c_str(),
            strCommand.c_str(),
            (int)vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }





    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
            return false;

        int64 nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64 nNonce = 1;
        string strSubVer;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (pfrom->nVersion >= 106 && !vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (pfrom->nVersion >= 106 && !vRecv.empty())
            vRecv >> strSubVer;
        if (pfrom->nVersion >= 209 && !vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

        if (pfrom->nVersion == 0)
            return false;

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            pfrom->fDisconnect = true;
            return true;
        }

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);
        if (pfrom->fClient)
        {
            pfrom->vSend.nType |= SER_BLOCKHEADERONLY;
            pfrom->vRecv.nType |= SER_BLOCKHEADERONLY;
        }

        AddTimeData(pfrom->addr.ip, nTime);

        // Change version
        if (pfrom->nVersion >= 209)
            pfrom->PushMessage("verack");
        pfrom->vSend.SetVersion(min(pfrom->nVersion, VERSION));
        if (pfrom->nVersion < 209)
            pfrom->vRecv.SetVersion(min(pfrom->nVersion, VERSION));

        // Ask the first connected node for block updates
        static int nAskedForBlocks;
        if (!pfrom->fClient && (nAskedForBlocks < 1 || vNodes.size() <= 1))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256(0));
        }

        pfrom->fSuccessfullyConnected = true;

        if (fDebug)
            printf("[NET] Peer version: %d, blocks=%d\n", pfrom->nVersion, pfrom->nStartingHeight);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->vRecv.SetVersion(min(pfrom->nVersion, VERSION));
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;
        if (pfrom->nVersion < 200) // don't want addresses from 0.1.5
            return true;
        if (vAddr.size() > 1000)
            return error("message addr size() = %d", vAddr.size());

        // Store the new addresses
        foreach(CAddress& addr, vAddr)
        {
            if (fShutdown)
                return true;
            addr.nTime = GetAdjustedTime() - 2 * 60 * 60;
            if (pfrom->fGetAddr || vAddr.size() > 10)
                addr.nTime -= 5 * 24 * 60 * 60;
            AddAddress(addr);
            pfrom->AddAddressKnown(addr);
            if (!pfrom->fGetAddr && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                CRITICAL_BLOCK(cs_vNodes)
                {
                    // Use deterministic randomness to send to
                    // the same places for an hour at a time
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                    uint256 hashRand = addr.ip ^ (GetTime()/3600) ^ hashSalt;
                    multimap<uint256, CNode*> mapMix;
                    foreach(CNode* pnode, vNodes)
                        mapMix.insert(make_pair(hashRand = Hash(BEGIN(hashRand), END(hashRand)), pnode));
                    int nRelayNodes = 10; // reduce this to 5 when the network is large
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
        }
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
    }


    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > 50000)
            return error("message inv size() = %d", vInv.size());

        CTxDB txdb("r");
        foreach(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            printf("  got inventory: %s  %s\n", inv.ToString().c_str(), fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash))
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));

            // Track requests for our stuff
            CRITICAL_BLOCK(cs_mapRequestCount)
            {
                map<uint256, int>::iterator mi = mapRequestCount.find(inv.hash);
                if (mi != mapRequestCount.end())
                    (*mi).second++;
            }
        }
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());

        foreach(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            if (fDebug)
                printf("[MSG] received getdata for: %s\n", inv.ToString().c_str());

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
            {
                auto mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    block.ReadFromDisk((*mi).second, true);

                    if (inv.type == MSG_FILTERED_BLOCK)
                    {
                        CRITICAL_BLOCK(pfrom->cs_filter)
                        {
                            if (pfrom->pfilter)
                            {
                                CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                                pfrom->PushMessage("merkleblock", merkleBlock);
                                vector<uint256> vMatchedTxn;
                                merkleBlock.txn.ExtractMatches(vMatchedTxn);

                                for (unsigned int i = 0; i < block.vtx.size(); i++)
                                {
                                    uint256 txHash = block.vtx[i].GetHash();
                                    for (unsigned int j = 0; j < vMatchedTxn.size(); j++)
                                    {
                                        if (vMatchedTxn[j] == txHash)
                                        {
                                            pfrom->PushMessage("tx", block.vtx[i]);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        pfrom->PushMessage("block", block);
                    }

                    if (inv.hash == pfrom->hashContinue)
                    {
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, hashBestChain));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                CRITICAL_BLOCK(cs_mapRelay)
                {
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end())
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                }
            }

            // Track requests for our stuff
            CRITICAL_BLOCK(cs_mapRequestCount)
            {
                map<uint256, int>::iterator mi = mapRequestCount.find(inv.hash);
                if (mi != mapRequestCount.end())
                    (*mi).second++;
            }
        }
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        // Find the first block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 500 + locator.GetDistanceBack();
        printf("getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,16).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                printf("  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,16).c_str());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                printf("  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,16).c_str());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        CDataStream vMsg(vRecv);
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;
        if (tx.AcceptTransaction(true, &fMissingInputs))
        {
            if (fDebug)
                printf("[TX] Accepted transaction %s (%d inputs, %d outputs)\n",
                    tx.GetHash().ToString().substr(0,16).c_str(),
                    (int)tx.vin.size(), (int)tx.vout.size());
            AddToWalletIfMine(tx, NULL);
            RelayMessage(inv, vMsg);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(hashPrev);
                     mi != mapOrphanTransactionsByPrev.upper_bound(hashPrev);
                     ++mi)
                {
                    const CDataStream& vMsg = *((*mi).second);
                    CTransaction tx;
                    CDataStream(vMsg) >> tx;
                    CInv inv(MSG_TX, tx.GetHash());

                    if (tx.AcceptTransaction(true))
                    {
                        if (fDebug)
                            printf("[TX] Accepted orphan tx %s\n", inv.hash.ToString().substr(0,6).c_str());
                        AddToWalletIfMine(tx, NULL);
                        RelayMessage(inv, vMsg);
                        mapAlreadyAskedFor.erase(inv);
                        vWorkQueue.push_back(inv.hash);
                    }
                }
            }

            foreach(uint256 hash, vWorkQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            if (fDebug)
                printf("[TX] Storing orphan tx %s\n", inv.hash.ToString().substr(0,6).c_str());
            AddOrphanTx(vMsg);
        }
    }


    else if (strCommand == "block")
    {
        auto_ptr<CBlock> pblock(new CBlock);
        vRecv >> *pblock;

        if (fDebug)
            printf("[BLOCK] Received block %s (prev=%s, %d txs)\n",
                pblock->GetHash().ToString().substr(0,16).c_str(),
                pblock->hashPrevBlock.ToString().substr(0,16).c_str(),
                (int)pblock->vtx.size());

        CInv inv(MSG_BLOCK, pblock->GetHash());
        pfrom->AddInventoryKnown(inv);

        if (ProcessBlock(pfrom, pblock.release()))
            mapAlreadyAskedFor.erase(inv);
    }


    else if (strCommand == "getaddr")
    {
        // This includes all nodes that are currently online,
        // since they rebroadcast an addr every 24 hours
        pfrom->vAddrToSend.clear();
        int64 nSince = GetAdjustedTime() - 24 * 60 * 60; // in the last 24 hours
        CRITICAL_BLOCK(cs_mapAddresses)
        {
            unsigned int nSize = mapAddresses.size();
            foreach(const PAIRTYPE(vector<unsigned char>, CAddress)& item, mapAddresses)
            {
                if (fShutdown)
                    return true;
                const CAddress& addr = item.second;
                if (addr.nTime > nSince)
                    pfrom->PushAddress(addr);
            }
        }
    }


    else if (strCommand == "checkorder")
    {
        uint256 hashReply;
        CWalletTx order;
        vRecv >> hashReply >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr.ip))
            mapReuseKey[pfrom->addr.ip] = GenerateNewKey();

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr.ip] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }


    else if (strCommand == "submitorder")
    {
        uint256 hashReply;
        CWalletTx wtxNew;
        vRecv >> hashReply >> wtxNew;
        wtxNew.fFromMe = false;

        // Broadcast
        if (!wtxNew.AcceptWalletTransaction())
        {
            pfrom->PushMessage("reply", hashReply, (int)1);
            return error("submitorder AcceptWalletTransaction() failed, returning error 1");
        }
        wtxNew.fTimeReceivedIsTxTime = true;
        AddToWallet(wtxNew);
        wtxNew.RelayWalletTransaction();
        mapReuseKey.erase(pfrom->addr.ip);

        // Send back confirmation
        pfrom->PushMessage("reply", hashReply, (int)0);
    }


    else if (strCommand == "reply")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        CRITICAL_BLOCK(pfrom->cs_mapRequests)
        {
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end())
            {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
    }


    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            auto mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = pindex->pnext;
        }

        vector<CBlock> vHeaders;
        int nLimit = 2000;
        if (fDebug)
            printf("[SPV] getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,16).c_str());
        for (; pindex; pindex = pindex->pnext)
        {
            CBlock header;
            header.nVersion       = pindex->nVersion;
            header.hashPrevBlock  = (pindex->pprev ? pindex->pprev->GetBlockHash() : uint256(0));
            header.hashMerkleRoot = pindex->hashMerkleRoot;
            header.nTime          = pindex->nTime;
            header.nBits          = pindex->nBits;
            header.nNonce         = pindex->nNonce;
            vHeaders.push_back(header);
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "headers")
    {
    }


    else if (strCommand == "filterload")
    {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
        {
            pfrom->fDisconnect = true;
            return false;
        }

        CRITICAL_BLOCK(pfrom->cs_filter)
        {
            if (pfrom->pfilter)
                delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->fClient = true;
        }
        if (fDebug)
            printf("[SPV] Loaded bloom filter from %s\n", pfrom->addr.ToStringLog().c_str());
    }


    else if (strCommand == "filteradd")
    {
        vector<unsigned char> vData;
        vRecv >> vData;

        if (vData.size() > 520)
        {
            pfrom->fDisconnect = true;
            return false;
        }

        CRITICAL_BLOCK(pfrom->cs_filter)
        {
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
        }
    }


    else if (strCommand == "filterclear")
    {
        CRITICAL_BLOCK(pfrom->cs_filter)
        {
            if (pfrom->pfilter)
            {
                delete pfrom->pfilter;
                pfrom->pfilter = NULL;
            }
        }
        pfrom->fClient = false;
    }


    else if (strCommand == "mempool")
    {
        vector<CInv> vInv;
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            vInv.reserve(mapTransactions.size());
            for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin();
                 mi != mapTransactions.end(); ++mi)
            {
                uint256 hash = (*mi).first;
                bool fSend = true;
                CRITICAL_BLOCK(pfrom->cs_filter)
                {
                    if (pfrom->pfilter)
                        fSend = pfrom->pfilter->IsRelevantAndUpdate((*mi).second, hash);
                }
                if (fSend)
                {
                    CInv inv(MSG_TX, hash);
                    vInv.push_back(inv);
                }
            }
        }
        pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "ping")
    {
    }


    else
    {
        // Ignore unknown commands for extensibility
    }


    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);


    return true;
}









bool SendMessages(CNode* pto, bool fSendTrickle)
{
    CRITICAL_BLOCK(cs_main)
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        // Keep-alive ping
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSend.empty())
            pto->PushMessage("ping");

        // Address refresh broadcast
        static int64 nLastRebroadcast;
        if (GetTime() - nLastRebroadcast > 24 * 60 * 60) // every 24 hours
        {
            nLastRebroadcast = GetTime();
            CRITICAL_BLOCK(cs_vNodes)
            {
                foreach(CNode* pnode, vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (addrLocalHost.IsRoutable() && !fUseProxy)
                        pnode->PushAddress(addrLocalHost);
                }
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();


        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            foreach(const CAddress& addr, pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }


        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        CRITICAL_BLOCK(pto->cs_inventory)
        {
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            foreach(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait)
                    {
                        TRY_CRITICAL_BLOCK(cs_mapWallet)
                        {
                            map<uint256, CWalletTx>::iterator mi = mapWallet.find(inv.hash);
                            if (mi != mapWallet.end())
                            {
                                CWalletTx& wtx = (*mi).second;
                                if (wtx.fFromMe)
                                    fTrickleWait = true;
                            }
                        }
                    }

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        //
        // Message: getdata
        //
        vector<CInv> vGetData;
        int64 nNow = GetTime() * 1000000;
        CTxDB txdb("r");
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv))
            {
                printf("sending getdata: %s\n", inv.ToString().c_str());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);

    }
    return true;
}














//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

void GenerateBitcoins(bool fGenerate)
{
    if (fGenerateBitcoins != fGenerate)
    {
        fGenerateBitcoins = fGenerate;
        CWalletDB().WriteSetting("fGenerateBitcoins", fGenerateBitcoins);
        MainFrameRepaint();
    }
    if (fGenerateBitcoins)
    {
#if wxUSE_GUI
        int nProcessors = wxThread::GetCPUCount();
#elif defined(_WIN32) || defined(__MINGW32__)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int nProcessors = sysinfo.dwNumberOfProcessors;
#else
        int nProcessors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
        printf("%d processors\n", nProcessors);
        if (nProcessors < 1)
            nProcessors = 1;
        if (fLimitProcessors && nProcessors > nLimitProcessors)
            nProcessors = nLimitProcessors;
        int nAddThreads = nProcessors - vnThreadsRunning[3];
        printf("Starting %d BitcoinMiner threads\n", nAddThreads);
        if (fLimitProcessors)
            printf("Thread limit: %d processors\n", nLimitProcessors);
        else
            printf("Thread limit: unlimited (using all %d processors)\n", nProcessors);
        for (int i = 0; i < nAddThreads; i++)
        {
            if (!CreateThread(ThreadBitcoinMiner, NULL))
                printf("Error: CreateThread(ThreadBitcoinMiner) failed\n");
            Sleep(10);
        }
        printf("Total active mining threads: %d\n", vnThreadsRunning[3]);
    }
}

void ThreadBitcoinMiner(void* parg)
{
    try
    {
        vnThreadsRunning[3]++;
        BitcoinMiner();
        vnThreadsRunning[3]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[3]--;
        PrintException(&e, "ThreadBitcoinMiner()");
    } catch (...) {
        vnThreadsRunning[3]--;
        PrintException(NULL, "ThreadBitcoinMiner()");
    }
    UIThreadCall(bind(CalledSetStatusBar, "", 0));
    printf("ThreadBitcoinMiner exiting, %d threads remaining\n", vnThreadsRunning[3]);
}

int FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

using CryptoPP::ByteReverse;
static int detectlittleendian = 1;
static bool g_sha256_initialized = false;

void InitSHA256()
{
    if (!g_sha256_initialized) {
        std::string impl = SHA256AutoDetect();
        printf("SHA256 implementation: %s\n", impl.c_str());
        fflush(stdout);
        g_sha256_initialized = true;
    }
}

void BlockSHA256(const void* pin, unsigned int nBlocks, void* pout)
{
    uint32_t* pstate = (uint32_t*)pout;
    const unsigned char* pdata = (const unsigned char*)pin;

    pstate[0] = 0x6a09e667;
    pstate[1] = 0xbb67ae85;
    pstate[2] = 0x3c6ef372;
    pstate[3] = 0xa54ff53a;
    pstate[4] = 0x510e527f;
    pstate[5] = 0x9b05688c;
    pstate[6] = 0x1f83d9ab;
    pstate[7] = 0x5be0cd19;

    sha256::Transform(pstate, pdata, nBlocks);

    if (*(char*)&detectlittleendian != 0) {
        for (int i = 0; i < 8; i++)
            pstate[i] = ByteReverse(pstate[i]);
    }
}


void ThreadGenesisMiner(void* parg)
{
    int nThreadID = (int)(intptr_t)parg;

    if (!pGenesisData)
        return;

    GenesisData& genesis = *pGenesisData;

    // Increment active thread counter
    CRITICAL_BLOCK(genesis.cs)
    {
        genesis.nActiveThreads++;
    }

    // Setup optimized mining structure
    struct {
        struct {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        } block_header;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    } tmp;

    tmp.block_header.nVersion = genesis.block.nVersion;
    tmp.block_header.hashPrevBlock = genesis.block.hashPrevBlock;
    tmp.block_header.hashMerkleRoot = genesis.block.hashMerkleRoot;
    tmp.block_header.nTime = genesis.block.nTime;
    tmp.block_header.nBits = genesis.block.nBits;
    tmp.block_header.nNonce = nThreadID;  // Start each thread at different nonce

    unsigned int nBlocks0 = FormatHashBlocks(&tmp.block_header, sizeof(tmp.block_header));
    unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    uint256 hash;
    int64 nHashCount = 0;
    int64 nStartTime = GetTimeMillis();

#if wxUSE_GUI
    int nThreads = wxThread::GetCPUCount();
#elif defined(_WIN32) || defined(__MINGW32__)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int nThreads = sysinfo.dwNumberOfProcessors;
#else
    int nThreads = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (nThreads < 1) nThreads = 1;
    if (fLimitProcessors && nThreads > nLimitProcessors)
        nThreads = nLimitProcessors;

    printf("Genesis thread %d started (Yespower)\n", nThreadID);

    while (!genesis.fFound)
    {
        genesis.block.nNonce = tmp.block_header.nNonce;
        genesis.block.nTime = tmp.block_header.nTime;
        hash = genesis.block.GetPoWHash();

        nHashCount++;

        if (hash <= genesis.hashTarget)
        {
            CRITICAL_BLOCK(genesis.cs)
            {
                if (!genesis.fFound)
                {
                    genesis.fFound = true;
                    genesis.nSolution = tmp.block_header.nNonce;
                    genesis.nTimeSolution = tmp.block_header.nTime;
                    printf("\nGenesis solution found by thread %d (Yespower)!\n", nThreadID);
                    printf("nNonce = %u\n", genesis.nSolution);
                    printf("PoW Hash = %s\n", hash.ToString().c_str());
                }
            }
            break;
        }

        tmp.block_header.nNonce += nThreads;

        if (tmp.block_header.nNonce < nThreads)
        {
            tmp.block_header.nTime++;
            printf("Thread %d: Nonce wrapped, incrementing time to %u\n", nThreadID, tmp.block_header.nTime);
        }

        if ((nHashCount & 0xFF) == 0)
        {
            int64 nElapsed = GetTimeMillis() - nStartTime;
            if (nElapsed > 0)
            {
                double dHashRate = (1000.0 * nHashCount) / nElapsed;
                printf("Thread %d: %.1f h/s, nNonce %08X, hash %s\n",
                       nThreadID, dHashRate, tmp.block_header.nNonce,
                       hash.ToString().substr(0, 16).c_str());
            }
        }
    }

    int64 nElapsed = GetTimeMillis() - nStartTime;
    if (nElapsed > 0)
    {
        double dHashRate = (1000.0 * nHashCount) / nElapsed;
        printf("Thread %d finished: %.1f h/s total (Yespower)\n", nThreadID, dHashRate);
    }

    CRITICAL_BLOCK(genesis.cs)
    {
        genesis.nActiveThreads--;
    }
}


void BitcoinMiner()
{
    if (vnThreadsRunning[3] == 1)
    {
        printf("\n");
        printf("========================================\n");
        printf("   YESPOWER MINER STARTED\n");
        printf("   (CPU-friendly, ASIC-resistant)\n");
        printf("========================================\n");
        printf("Algorithm: Yespower 1.0 (N=2048, r=32)\n");
        printf("Threads:   %d\n", fLimitProcessors ? nLimitProcessors : vnThreadsRunning[3]);
        printf("Height:    %d\n", nBestHeight);
        printf("========================================\n");
        printf("\n");
    }

    yespower_local_t local;
    if (yespower_init_local(&local) != 0) {
        printf("ERROR: Failed to initialize yespower local context\n");
        return;
    }

    static int thread_id = 0;
    int tid;
    {
        static CCriticalSection cs_thread_id;
        CRITICAL_BLOCK(cs_thread_id)
            tid = thread_id++;
    }

#ifdef _WIN32
    DWORD_PTR numCpus = 0;
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    numCpus = sysinfo.dwNumberOfProcessors;
    if (numCpus > 0) {
        DWORD_PTR mask = (DWORD_PTR)1 << (tid % numCpus);
        SetThreadAffinityMask(GetCurrentThread(), mask);
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#elif defined(__APPLE__)
    thread_affinity_policy_data_t policy = { tid };
    thread_policy_set(pthread_mach_thread_np(pthread_self()),
                      THREAD_AFFINITY_POLICY,
                      (thread_policy_t)&policy, 1);
    SetThreadPriority(THREAD_PRIORITY_NORMAL);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(tid % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    SetThreadPriority(THREAD_PRIORITY_NORMAL);
#endif

    CKey key;
    key.MakeNewKey();
    CBigNum bnExtraNonce = 0;
    while (fGenerateBitcoins)
    {
        if (fShutdown) {
            yespower_free_local(&local);
            return;
        }
        if (!fTestMode)
        {
            while (vNodes.empty())
            {
                Sleep(1000);
                if (fShutdown) {
                    yespower_free_local(&local);
                    return;
                }
                if (!fGenerateBitcoins) {
                    yespower_free_local(&local);
                    return;
                }
            }
        }

        unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
        CBlockIndex* pindexPrev = pindexBest;
        unsigned int nBits = GetNextWorkRequired(pindexPrev);


        //
        // Create coinbase tx
        //
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vin[0].scriptSig << nBits << ++bnExtraNonce;
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;


        //
        // Create new block
        //
        auto_ptr<CBlock> pblock(new CBlock());
        if (!pblock.get()) {
            yespower_free_local(&local);
            return;
        }

        // Add our coinbase tx as first transaction
        pblock->vtx.push_back(txNew);

        // Collect the latest transactions into the block
        int64 nFees = 0;
        int nTxsAdded = 0;
        CRITICAL_BLOCK(cs_main)
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            CTxDB txdb("r");
            map<uint256, CTxIndex> mapTestPool;
            int nTargetHeight = (pindexPrev ? pindexPrev->nHeight + 1 : 0);
            bool fUsePriority = (nTargetHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT);

            if (fUsePriority)
            {
                struct CTxCand
                {
                    uint256 hash;
                    CTransaction tx;
                    double dPriority;
                    double dFeePerByte;
                    unsigned int nTxSize;
                    int64 nTxFee;
                };

                vector<CTxCand> vCand;
                vCand.reserve(mapTransactions.size());

                for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi)
                {
                    CTransaction& tx = (*mi).second;
                    if (tx.IsCoinBase() || !tx.IsFinal())
                        continue;

                    CTxCand c;
                    c.hash = (*mi).first;
                    c.tx = tx;
                    c.nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
                    c.dPriority = ComputePriority(tx, txdb, nTargetHeight);

                    int64 nValueIn = 0;
                    bool fOk = true;
                    for (int i = 0; i < tx.vin.size(); i++)
                    {
                        COutPoint prevout = tx.vin[i].prevout;
                        CTxIndex txindex;
                        CTransaction txPrev;
                        bool fFoundIndex = false;

                        if (mapTestPool.count(prevout.hash))
                        {
                            txindex = mapTestPool[prevout.hash];
                            fFoundIndex = true;
                        }
                        else
                        {
                            fFoundIndex = txdb.ReadTxIndex(prevout.hash, txindex);
                        }

                        if (fFoundIndex && txindex.pos != CDiskTxPos(1,1,1))
                        {
                            if (!txPrev.ReadFromDisk(txindex.pos))
                            {
                                fOk = false;
                                break;
                            }
                        }
                        else if (mapTransactions.count(prevout.hash))
                        {
                            txPrev = mapTransactions[prevout.hash];
                        }
                        else
                        {
                            fOk = false;
                            break;
                        }

                        if (prevout.n < txPrev.vout.size())
                            nValueIn += txPrev.vout[prevout.n].nValue;
                    }
                    if (!fOk)
                        continue;

                    c.nTxFee = nValueIn - tx.GetValueOut();
                    if (c.nTxFee < 0)
                        continue;
                    c.dFeePerByte = (c.nTxSize > 0) ? (double)c.nTxFee / c.nTxSize : 0.0;
                    vCand.push_back(c);
                }

                sort(vCand.begin(), vCand.end(),
                    [](const CTxCand& a, const CTxCand& b) {
                        return a.dPriority > b.dPriority;
                    });

                unsigned int nBlockSize = 0;
                unsigned int nBlockPrioritySize = 0;
                set<uint256> setAdded;

                for (unsigned int i = 0; i < vCand.size(); i++)
                {
                    CTxCand& c = vCand[i];
                    if (nBlockPrioritySize + c.nTxSize > DEFAULT_BLOCK_PRIORITY_SIZE)
                        continue;
                    if (c.dPriority < FREE_PRIORITY_THRESHOLD && c.nTxFee == 0)
                        continue;

                    bool fHasDust = false;
                    foreach(const CTxOut& txout, c.tx.vout)
                        if (txout.nValue < DUST_THRESHOLD)
                            fHasDust = true;
                    if (fHasDust && c.nTxFee < CENT)
                        continue;

                    bool fAllowFree = (c.dPriority >= FREE_PRIORITY_THRESHOLD);
                    int64 nMinFee = c.tx.GetMinFee(nBlockSize, fAllowFree);
                    if (c.nTxFee < nMinFee && !fAllowFree)
                        continue;

                    map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                    if (!c.tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, fAllowFree ? 0 : nMinFee))
                        continue;
                    swap(mapTestPool, mapTestPoolTmp);

                    pblock->vtx.push_back(c.tx);
                    nBlockSize += c.nTxSize;
                    nBlockPrioritySize += c.nTxSize;
                    setAdded.insert(c.hash);
                    nTxsAdded++;
                }

                vector<CTxCand> vFeeSorted;
                for (unsigned int i = 0; i < vCand.size(); i++)
                    if (!setAdded.count(vCand[i].hash))
                        vFeeSorted.push_back(vCand[i]);

                sort(vFeeSorted.begin(), vFeeSorted.end(),
                    [](const CTxCand& a, const CTxCand& b) {
                        return a.dFeePerByte > b.dFeePerByte;
                    });

                for (unsigned int i = 0; i < vFeeSorted.size(); i++)
                {
                    CTxCand& c = vFeeSorted[i];
                    if (nBlockSize + c.nTxSize > MAX_SIZE/2)
                        break;

                    int64 nMinFee = c.tx.GetMinFee(nBlockSize);
                    if (c.nTxFee < nMinFee)
                        continue;

                    bool fHasDust = false;
                    foreach(const CTxOut& txout, c.tx.vout)
                        if (txout.nValue < DUST_THRESHOLD)
                            fHasDust = true;
                    if (fHasDust && c.nTxFee < CENT)
                        continue;

                    map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                    if (!c.tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, nMinFee))
                        continue;
                    swap(mapTestPool, mapTestPoolTmp);

                    pblock->vtx.push_back(c.tx);
                    nBlockSize += c.nTxSize;
                    nTxsAdded++;
                }
            }
            else
            {
                vector<char> vfAlreadyAdded(mapTransactions.size());
                bool fFoundSomething = true;
                unsigned int nBlockSize = 0;
                while (fFoundSomething && nBlockSize < MAX_SIZE/2)
                {
                    fFoundSomething = false;
                    unsigned int n = 0;
                    for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n)
                    {
                        if (vfAlreadyAdded[n])
                            continue;
                        CTransaction& tx = (*mi).second;
                        if (tx.IsCoinBase() || !tx.IsFinal())
                            continue;

                        int64 nMinFee = tx.GetMinFee(nBlockSize);

                        map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                        if (!tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, nMinFee))
                            continue;
                        swap(mapTestPool, mapTestPoolTmp);

                        pblock->vtx.push_back(tx);
                        nBlockSize += ::GetSerializeSize(tx, SER_NETWORK);
                        vfAlreadyAdded[n] = true;
                        fFoundSomething = true;
                        nTxsAdded++;
                    }
                }
            }
        }
        pblock->nBits = nBits;
        pblock->vtx[0].vout[0].nValue = pblock->GetBlockValue(nFees);


        //
        // Prebuild hash buffer
        //
        struct unnamed1
        {
            struct unnamed2
            {
                int nVersion;
                uint256 hashPrevBlock;
                uint256 hashMerkleRoot;
                unsigned int nTime;
                unsigned int nBits;
                unsigned int nNonce;
            }
            block;
            unsigned char pchPadding0[64];
            uint256 hash1;
            unsigned char pchPadding1[64];
        }
        tmp;

        tmp.block.nVersion       = pblock->nVersion;
        tmp.block.hashPrevBlock  = pblock->hashPrevBlock  = (pindexPrev ? pindexPrev->GetBlockHash() : 0);
        tmp.block.hashMerkleRoot = pblock->hashMerkleRoot = pblock->BuildMerkleTree();
        tmp.block.nTime          = pblock->nTime          = max((pindexPrev ? pindexPrev->GetMedianTimePast()+1 : 0), GetAdjustedTime());
        tmp.block.nBits          = pblock->nBits          = nBits;
        tmp.block.nNonce         = pblock->nNonce         = 1;

        unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
        unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));


        //
        // Search using Yespower (CPU-friendly, ASIC-resistant)
        //
        int64 nStart = GetTime();
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
        uint256 hash;
        loop
        {
            if (fShutdown || !fGenerateBitcoins) {
                yespower_free_local(&local);
                return;
            }

            pblock->nNonce = tmp.block.nNonce;
            hash = pblock->GetPoWHash(&local);

            if (hash <= hashTarget)
            {
                    printf("\n");
                    printf("========================================\n");
                    printf(">>> BLOCK MINED (Yespower)! <<<\n");
                    printf("========================================\n");
                    printf("Height:  %d\n", nBestHeight + 1);
                    printf("PoW:     %s\n", hash.GetHex().c_str());
                    printf("Block:   %s\n", pblock->GetHash().GetHex().c_str());
                    printf("Target:  %s\n", hashTarget.GetHex().c_str());
                    printf("Reward:  %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());
                    printf("Txs:     %d\n", pblock->vtx.size());
                    printf("Time:    %s\n", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
                    printf("========================================\n");
                    printf("\n");

                SetThreadPriority(THREAD_PRIORITY_NORMAL);
                CRITICAL_BLOCK(cs_main)
                {
                    if (pindexPrev == pindexBest)
                    {
                        if (!AddKey(key)) {
                            yespower_free_local(&local);
                            return;
                        }
                        key.MakeNewKey();

                        CRITICAL_BLOCK(cs_mapRequestCount)
                            mapRequestCount[pblock->GetHash()] = 0;

                        if (!ProcessBlock(NULL, pblock.release()))
                            printf("ERROR in BitcoinMiner, ProcessBlock, block not accepted\n");
                    }
                }
                SetThreadPriority(THREAD_PRIORITY_LOWEST);

                Sleep(500);
                break;
            }

            const unsigned int nMask = 0xff;
            if ((++tmp.block.nNonce & nMask) == 0)
            {
                static CCriticalSection cs_hashrate;
                static int64 nHashCounter;
                static int64 nLastTick;
                static double dSmoothedHashRate = 0.0;
                const double dSmoothingFactor = 0.3;

                CRITICAL_BLOCK(cs_hashrate)
                {
                    if (nLastTick == 0)
                        nLastTick = GetTimeMillis();
                    else
                        nHashCounter += nMask + 1;

                    int64 nNow = GetTimeMillis();
                    if (nNow - nLastTick > 4000)
                    {
                        double dHashesPerSec = 1000.0 * nHashCounter / (nNow - nLastTick);
                        nLastTick = nNow;
                        nHashCounter = 0;

                        if (dSmoothedHashRate == 0.0)
                            dSmoothedHashRate = dHashesPerSec;
                        else
                            dSmoothedHashRate = dSmoothingFactor * dHashesPerSec + (1.0 - dSmoothingFactor) * dSmoothedHashRate;

                        string strHashRate;
                        if (dSmoothedHashRate >= 1000000000.0)
                            strHashRate = strprintf(" %.2f GH/s", dSmoothedHashRate / 1000000000.0);
                        else if (dSmoothedHashRate >= 1000000.0)
                            strHashRate = strprintf(" %.2f MH/s", dSmoothedHashRate / 1000000.0);
                        else if (dSmoothedHashRate >= 1000.0)
                            strHashRate = strprintf(" %.2f KH/s", dSmoothedHashRate / 1000.0);
                        else
                            strHashRate = strprintf(" %.1f H/s", dSmoothedHashRate);

                        UIThreadCall(bind(CalledSetStatusBar, strHashRate, 0));
                        static int64 nLogTime;
                        if (GetTime() - nLogTime > 30)
                        {
                            nLogTime = GetTime();
                            if (dSmoothedHashRate >= 1000000000.0)
                                printf("Hashrate: %.2f GH/s | Threads: %d | Height: %d | Txs: %d\n",
                                       dSmoothedHashRate / 1000000000.0, vnThreadsRunning[3], nBestHeight, pblock->vtx.size());
                            else if (dSmoothedHashRate >= 1000000.0)
                                printf("Hashrate: %.2f MH/s | Threads: %d | Height: %d | Txs: %d\n",
                                       dSmoothedHashRate / 1000000.0, vnThreadsRunning[3], nBestHeight, pblock->vtx.size());
                            else if (dSmoothedHashRate >= 1000.0)
                                printf("Hashrate: %.2f KH/s | Threads: %d | Height: %d | Txs: %d\n",
                                       dSmoothedHashRate / 1000.0, vnThreadsRunning[3], nBestHeight, pblock->vtx.size());
                            else
                                printf("Hashrate: %.1f H/s | Threads: %d | Height: %d | Txs: %d\n",
                                       dSmoothedHashRate, vnThreadsRunning[3], nBestHeight, pblock->vtx.size());
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                if (fShutdown)
                    return;
                if (!fGenerateBitcoins)
                    return;
                if (fLimitProcessors && vnThreadsRunning[3] > nLimitProcessors)
                {
                    printf("Stopping thread: exceeded limit (%d > %d)\n", vnThreadsRunning[3], nLimitProcessors);
                    return;
                }
                if (vNodes.empty())
                    break;
                if (tmp.block.nNonce == 0)
                    break;
                if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != pindexBest)
                {
                    // Pause generating during initial download
                    if (GetTime() - nStart < 20)
                    {
                        CBlockIndex* pindexTmp;
                        do
                        {
                            pindexTmp = pindexBest;
                            for (int i = 0; i < 10; i++)
                            {
                                Sleep(1000);
                                if (fShutdown) {
                                    yespower_free_local(&local);
                                    return;
                                }
                            }
                        }
                        while (pindexTmp != pindexBest);
                    }
                    break;
                }

                tmp.block.nTime = pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
            }
        }
    }

    yespower_free_local(&local);
}


CBlock* CreateNewBlock(CKey& key)
{
    CBlockIndex* pindexPrev = pindexBest;

    auto_ptr<CBlock> pblock(new CBlock());
    if (!pblock.get())
        return NULL;

    unsigned int nBits = GetNextWorkRequired(pindexPrev);

    static unsigned int nExtraNonce = 0;
    nExtraNonce++;

    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vin[0].scriptSig << nBits << nExtraNonce;
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

    pblock->vtx.push_back(txNew);

    int64 nFees = 0;
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        CTxDB txdb("r");
        map<uint256, CTxIndex> mapTestPool;
        int nTargetHeight = (pindexPrev ? pindexPrev->nHeight + 1 : 0);
        bool fUsePriority = (nTargetHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT);

        if (fUsePriority)
        {
            struct CTxCandidate
            {
                uint256 hash;
                CTransaction tx;
                double dPriority;
                double dFeePerByte;
                unsigned int nTxSize;
                int64 nTxFee;
            };

            vector<CTxCandidate> vCandidates;
            vCandidates.reserve(mapTransactions.size());

            for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi)
            {
                CTransaction& tx = (*mi).second;
                if (tx.IsCoinBase() || !tx.IsFinal())
                    continue;

                CTxCandidate cand;
                cand.hash = (*mi).first;
                cand.tx = tx;
                cand.nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
                cand.dPriority = ComputePriority(tx, txdb, nTargetHeight);

                int64 nValueIn = 0;
                bool fInputsOk = true;
                for (int i = 0; i < tx.vin.size(); i++)
                {
                    COutPoint prevout = tx.vin[i].prevout;
                    CTxIndex txindex;
                    CTransaction txPrev;
                    bool fFoundIndex = false;

                    if (mapTestPool.count(prevout.hash))
                    {
                        txindex = mapTestPool[prevout.hash];
                        fFoundIndex = true;
                    }
                    else
                    {
                        fFoundIndex = txdb.ReadTxIndex(prevout.hash, txindex);
                    }

                    if (fFoundIndex && txindex.pos != CDiskTxPos(1,1,1))
                    {
                        if (!txPrev.ReadFromDisk(txindex.pos))
                        {
                            fInputsOk = false;
                            break;
                        }
                    }
                    else if (mapTransactions.count(prevout.hash))
                    {
                        txPrev = mapTransactions[prevout.hash];
                    }
                    else
                    {
                        fInputsOk = false;
                        break;
                    }

                    if (prevout.n < txPrev.vout.size())
                        nValueIn += txPrev.vout[prevout.n].nValue;
                }
                if (!fInputsOk)
                    continue;

                cand.nTxFee = nValueIn - tx.GetValueOut();
                if (cand.nTxFee < 0)
                    continue;
                cand.dFeePerByte = (cand.nTxSize > 0) ? (double)cand.nTxFee / cand.nTxSize : 0.0;

                vCandidates.push_back(cand);
            }

            sort(vCandidates.begin(), vCandidates.end(),
                [](const CTxCandidate& a, const CTxCandidate& b) {
                    return a.dPriority > b.dPriority;
                });

            unsigned int nBlockSize = 0;
            unsigned int nBlockPrioritySize = 0;
            set<uint256> setAdded;

            for (unsigned int i = 0; i < vCandidates.size(); i++)
            {
                CTxCandidate& cand = vCandidates[i];
                if (nBlockPrioritySize + cand.nTxSize > DEFAULT_BLOCK_PRIORITY_SIZE)
                    continue;
                if (cand.dPriority < FREE_PRIORITY_THRESHOLD && cand.nTxFee == 0)
                    continue;

                bool fHasDust = false;
                foreach(const CTxOut& txout, cand.tx.vout)
                    if (txout.nValue < DUST_THRESHOLD)
                        fHasDust = true;
                if (fHasDust && cand.nTxFee < CENT)
                    continue;

                bool fAllowFree = (cand.dPriority >= FREE_PRIORITY_THRESHOLD);
                int64 nMinFee = cand.tx.GetMinFee(nBlockSize, fAllowFree);
                if (cand.nTxFee < nMinFee && !fAllowFree)
                    continue;

                map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                if (!cand.tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, fAllowFree ? 0 : nMinFee))
                    continue;
                swap(mapTestPool, mapTestPoolTmp);

                pblock->vtx.push_back(cand.tx);
                nBlockSize += cand.nTxSize;
                nBlockPrioritySize += cand.nTxSize;
                setAdded.insert(cand.hash);
            }

            vector<CTxCandidate> vFeeSorted;
            for (unsigned int i = 0; i < vCandidates.size(); i++)
            {
                if (setAdded.count(vCandidates[i].hash))
                    continue;
                vFeeSorted.push_back(vCandidates[i]);
            }

            sort(vFeeSorted.begin(), vFeeSorted.end(),
                [](const CTxCandidate& a, const CTxCandidate& b) {
                    return a.dFeePerByte > b.dFeePerByte;
                });

            for (unsigned int i = 0; i < vFeeSorted.size(); i++)
            {
                CTxCandidate& cand = vFeeSorted[i];
                if (nBlockSize + cand.nTxSize > MAX_SIZE/2)
                    break;

                int64 nMinFee = cand.tx.GetMinFee(nBlockSize);
                if (cand.nTxFee < nMinFee)
                    continue;

                bool fHasDust = false;
                foreach(const CTxOut& txout, cand.tx.vout)
                    if (txout.nValue < DUST_THRESHOLD)
                        fHasDust = true;
                if (fHasDust && cand.nTxFee < CENT)
                    continue;

                map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                if (!cand.tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, nMinFee))
                    continue;
                swap(mapTestPool, mapTestPoolTmp);

                pblock->vtx.push_back(cand.tx);
                nBlockSize += cand.nTxSize;
            }
        }
        else
        {
            vector<char> vfAlreadyAdded(mapTransactions.size());
            bool fFoundSomething = true;
            unsigned int nBlockSize = 0;

            while (fFoundSomething && nBlockSize < MAX_SIZE/2)
            {
                fFoundSomething = false;
                unsigned int n = 0;
                for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n)
                {
                    if (vfAlreadyAdded[n])
                        continue;
                    CTransaction& tx = (*mi).second;
                    if (tx.IsCoinBase() || !tx.IsFinal())
                        continue;

                    int64 nMinFee = tx.GetMinFee(nBlockSize);

                    map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                    if (!tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), nTargetHeight, nFees, false, true, nMinFee))
                        continue;
                    swap(mapTestPool, mapTestPoolTmp);

                    pblock->vtx.push_back(tx);
                    nBlockSize += ::GetSerializeSize(tx, SER_NETWORK);
                    vfAlreadyAdded[n] = true;
                    fFoundSomething = true;
                }
            }
        }
    }

    pblock->vtx[0].vout[0].nValue = pblock->GetBlockValue(nFees);

    pblock->hashPrevBlock = pindexPrev ? pindexPrev->GetBlockHash() : 0;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
    pblock->nTime = max(pindexPrev ? pindexPrev->GetMedianTimePast()+1 : 0, GetAdjustedTime());
    pblock->nBits = nBits;
    pblock->nNonce = 0;

    return pblock.release();
}


int64 GetNetworkHashPS(int lookup)
{
    CBlockIndex *pb = pindexBest;

    if (pb == NULL || nBestHeight < 2)
        return 0;

    // Adjust lookup to available blocks
    int actualLookup = (nBestHeight < lookup) ? (int)nBestHeight : lookup;

    // Store the last block
    CBlockIndex *pbLast = pb;
    int64 timeLast = pbLast->nTime;

    // Go back actualLookup blocks
    for (int i = 0; i < actualLookup && pb->pprev != NULL; i++)
    {
        pb = pb->pprev;
    }

    int64 timeFirst = pb->nTime;

    // Calculate actual time span from real block timestamps
    int64 timeSpan = timeLast - timeFirst;

    if (timeSpan <= 0)
        return 0;

    // Number of block intervals
    int numBlocks = actualLookup;

    // Get current difficulty from the best block
    int nShift = (pbLast->nBits >> 24) & 0xff;
    int nMantissa = pbLast->nBits & 0x007fffff;
    double difficulty = (double)0x007fffff / (double)nMantissa;
    while (nShift < 0x1e) { difficulty *= 256.0; nShift++; }
    while (nShift > 0x1e) { difficulty /= 256.0; nShift--; }

    // Hashrate = (num_blocks * difficulty * 2^17) / actual_time_span
    // Bitok uses 17 leading zero bits
    double hashesPerSec = ((double)numBlocks * difficulty * pow(2.0, 17)) / (double)timeSpan;

    return (int64)hashesPerSec;
}




















//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64 GetBalance()
{
    int64 nStart = GetTimeMillis();

    int64 nTotal = 0;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || pcoin->fSpent)
                continue;
            nTotal += pcoin->GetCredit(true);
        }
    }

    //printf("GetBalance() %"PRI64d"ms\n", GetTimeMillis() - nStart);
    return nTotal;
}



bool SelectCoins(int64 nTargetValue, set<CWalletTx*>& setCoinsRet)
{
    setCoinsRet.clear();

    // List of values less than target
    int64 nLowestLarger = INT64_MAX;
    CWalletTx* pcoinLowestLarger = NULL;
    vector<pair<int64, CWalletTx*> > vValue;
    int64 nTotalLower = 0;

    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || pcoin->fSpent)
                continue;
            int64 n = pcoin->GetCredit();
            if (n <= 0)
                continue;
            if (n < nTargetValue)
            {
                vValue.push_back(make_pair(n, pcoin));
                nTotalLower += n;
            }
            else if (n == nTargetValue)
            {
                setCoinsRet.insert(pcoin);
                return true;
            }
            else if (n < nLowestLarger)
            {
                nLowestLarger = n;
                pcoinLowestLarger = pcoin;
            }
        }
    }

    if (nTotalLower < nTargetValue)
    {
        if (pcoinLowestLarger == NULL)
            return false;
        setCoinsRet.insert(pcoinLowestLarger);
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend());
    vector<char> vfIncluded;
    vector<char> vfBest(vValue.size(), true);
    int64 nBest = nTotalLower;

    for (int nRep = 0; nRep < 1000 && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64 nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (int i = 0; i < vValue.size(); i++)
            {
                if (nPass == 0 ? rand() % 2 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }

    // If the next larger is still closer, return it
    if (pcoinLowestLarger && nLowestLarger - nTargetValue <= nBest - nTargetValue)
        setCoinsRet.insert(pcoinLowestLarger);
    else
    {
        for (int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                setCoinsRet.insert(vValue[i].second);

        //// debug print
        printf("SelectCoins() best subset: ");
        for (int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                printf("%s ", FormatMoney(vValue[i].first).c_str());
        printf("total %s\n", FormatMoney(nBest).c_str());
    }

    return true;
}


static bool GetStealthChangeKeyForInputs(const set<CWalletTx*>& setCoins, CKey& changeKeyOut)
{
    CRITICAL_BLOCK(cs_stealthAddresses)
    CRITICAL_BLOCK(cs_mapKeys)
    {
        foreach(CWalletTx* pcoin, setCoins)
        {
            for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
            {
                if (!pcoin->vout[nOut].IsMine())
                    continue;

                uint160 hash160 = pcoin->vout[nOut].scriptPubKey.GetBitcoinAddressHash160();
                if (hash160 == 0)
                    continue;

                if (!mapPubKeys.count(hash160))
                    continue;

                vector<unsigned char> vchPubKey = mapPubKeys[hash160];

                if (!mapStealthDestToScan.count(vchPubKey))
                    continue;

                vector<unsigned char> vchScanPub = mapStealthDestToScan[vchPubKey].first;

                foreach(const CStealthAddress& sxAddr, vStealthAddresses)
                {
                    if (sxAddr.scan_pubkey != vchScanPub)
                        continue;

                    CWalletDB walletdb;
                    CPrivKey vchSpendPrivKey;
                    if (!walletdb.ReadStealthSpendKey(sxAddr.spend_pubkey, vchSpendPrivKey))
                        continue;

                    CKey spendKey;
                    if (!spendKey.SetPrivKey(vchSpendPrivKey))
                        continue;

                    vector<unsigned char> vchSpendSecret = spendKey.GetSecret();

                    uint32_t nIndex = 0;
                    if (mapStealthChangeIndex.count(sxAddr.spend_pubkey))
                        nIndex = mapStealthChangeIndex[sxAddr.spend_pubkey];

                    vector<unsigned char> vchChangePriv, vchChangePub;
                    if (!StealthDeriveChangeKey(vchSpendSecret, nIndex, vchChangePriv, vchChangePub))
                        continue;

                    if (!changeKeyOut.SetSecret(vchChangePriv))
                        continue;

                    mapStealthChangeIndex[sxAddr.spend_pubkey] = nIndex + 1;
                    walletdb.WriteStealthChangeIndex(sxAddr.spend_pubkey, nIndex + 1);

                    printf("stealth: using deterministic change key index %u for stealth address %s\n",
                           nIndex, sxAddr.Encoded().substr(0, 16).c_str());
                    return true;
                }
            }
        }
    }
    return false;
}


bool CreateTransaction(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, CKey& keyRet, int64& nFeeRequiredRet)
{
    nFeeRequiredRet = 0;
    CRITICAL_BLOCK(cs_main)
    {
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapWallet)
        {
            int64 nFee = nTransactionFee;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;
                if (nValue < 0)
                    return false;
                int64 nValueOut = nValue;
                int64 nTotalValue = nValue + nFee;

                // Choose coins to use
                set<CWalletTx*> setCoins;
                if (!SelectCoins(nTotalValue, setCoins))
                    return false;
                int64 nValueIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    nValueIn += pcoin->GetCredit();

                // Fill a vout to the payee
                bool fChangeFirst = GetRand(2);
                if (!fChangeFirst)
                    wtxNew.vout.push_back(CTxOut(nValueOut, scriptPubKey));

                // Fill a vout back to self with any change
                if (nValueIn > nTotalValue)
                {
                    if (keyRet.IsNull())
                    {
                        if (!GetStealthChangeKeyForInputs(setCoins, keyRet))
                            keyRet.MakeNewKey();
                    }

                    CScript scriptChange;
                    if (scriptPubKey.GetBitcoinAddressHash160() != 0)
                        scriptChange.SetBitcoinAddress(keyRet.GetPubKey());
                    else
                        scriptChange << keyRet.GetPubKey() << OP_CHECKSIG;
                    wtxNew.vout.push_back(CTxOut(nValueIn - nTotalValue, scriptChange));
                }

                // Fill a vout to the payee
                if (fChangeFirst)
                    wtxNew.vout.push_back(CTxOut(nValueOut, scriptPubKey));

                // Fill vin
                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            wtxNew.vin.push_back(CTxIn(pcoin->GetHash(), nOut));

                // Sign
                int nIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            SignSignature(*pcoin, wtxNew, nIn++);

                // Check that enough fee is included
                bool fAllowFree = false;
                if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                {
                    double dPriority = ComputePriority(wtxNew, txdb, nBestHeight + 1);
                    fAllowFree = (dPriority >= FREE_PRIORITY_THRESHOLD);
                }
                if (nFee < wtxNew.GetMinFee(1, fAllowFree))
                {
                    nFee = nFeeRequiredRet = wtxNew.GetMinFee(1, fAllowFree);
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CreateStealthTransaction(CScript scriptPubKey, const CScript& scriptOpReturn,
                              int64 nValue, CWalletTx& wtxNew, CKey& keyRet,
                              int64& nFeeRequiredRet)
{
    nFeeRequiredRet = 0;
    CRITICAL_BLOCK(cs_main)
    {
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapWallet)
        {
            int64 nFee = nTransactionFee;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;
                if (nValue < 0)
                    return false;
                int64 nValueOut = nValue;
                int64 nTotalValue = nValue + nFee;

                set<CWalletTx*> setCoins;
                if (!SelectCoins(nTotalValue, setCoins))
                    return false;
                int64 nValueIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    nValueIn += pcoin->GetCredit();

                bool fChangeFirst = GetRand(2);
                if (!fChangeFirst)
                {
                    wtxNew.vout.push_back(CTxOut(nValueOut, scriptPubKey));
                    wtxNew.vout.push_back(CTxOut(0, scriptOpReturn));
                }

                if (nValueIn > nTotalValue)
                {
                    if (keyRet.IsNull())
                    {
                        if (!GetStealthChangeKeyForInputs(setCoins, keyRet))
                            keyRet.MakeNewKey();
                    }

                    CScript scriptChange;
                    scriptChange.SetBitcoinAddress(keyRet.GetPubKey());
                    wtxNew.vout.push_back(CTxOut(nValueIn - nTotalValue, scriptChange));
                }

                if (fChangeFirst)
                {
                    wtxNew.vout.push_back(CTxOut(nValueOut, scriptPubKey));
                    wtxNew.vout.push_back(CTxOut(0, scriptOpReturn));
                }

                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            wtxNew.vin.push_back(CTxIn(pcoin->GetHash(), nOut));

                int nIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            SignSignature(*pcoin, wtxNew, nIn++);

                bool fAllowFree = false;
                if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                {
                    double dPriority = ComputePriority(wtxNew, txdb, nBestHeight + 1);
                    fAllowFree = (dPriority >= FREE_PRIORITY_THRESHOLD);
                }
                if (nFee < wtxNew.GetMinFee(1, fAllowFree))
                {
                    nFee = nFeeRequiredRet = wtxNew.GetMinFee(1, fAllowFree);
                    continue;
                }

                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}


// Call after CreateTransaction unless you want to abort
bool CommitTransaction(CWalletTx& wtxNew, const CKey& key)
{
    CRITICAL_BLOCK(cs_main)
    {
        printf("CommitTransaction:\n%s", wtxNew.ToString().c_str());
        CRITICAL_BLOCK(cs_mapWallet)
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB walletdb("r");

            // Add the change's private key to wallet
            if (!key.IsNull() && !AddKey(key))
                throw runtime_error("CommitTransaction() : AddKey failed\n");

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            foreach(const CTxIn& txin, wtxNew.vin)
                setCoins.insert(&mapWallet[txin.prevout.hash]);
            foreach(CWalletTx* pcoin, setCoins)
            {
                pcoin->fSpent = true;
                pcoin->WriteToDisk();
                vWalletUpdated.push_back(pcoin->GetHash());
            }
        }

        // Track how many getdata requests our transaction gets
        CRITICAL_BLOCK(cs_mapRequestCount)
            mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptTransaction())
        {
            // This must not fail. The transaction has already been signed and recorded.
            printf("CommitTransaction() : Error: Transaction not valid");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    MainFrameRepaint();
    return true;
}




string SendMoney(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, bool fAskFee)
{
    CRITICAL_BLOCK(cs_main)
    {
        CKey key;
        int64 nFeeRequired;
        if (!CreateTransaction(scriptPubKey, nValue, wtxNew, key, nFeeRequired))
        {
            string strError;
            if (nValue + nFeeRequired > GetBalance())
                strError = strprintf(_STR("Error: This is an oversized transaction that requires a transaction fee of %s  ").c_str(), FormatMoney(nFeeRequired).c_str());
            else
                strError = _STR("Error: Transaction creation failed  ");
            printf("SendMoney() : %s", strError.c_str());
            return strError;
        }

        if (fAskFee && !ThreadSafeAskFee(nFeeRequired, _STR("Sending..."), NULL))
            return "ABORTED";

        if (!CommitTransaction(wtxNew, key))
            return _STR("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
    }
    MainFrameRepaint();
    return "";
}



string SendMoneyToBitcoinAddress(string strAddress, int64 nValue, CWalletTx& wtxNew, bool fAskFee)
{
    // Check amount
    if (nValue <= 0)
        return _STR("Invalid amount");
    if (nValue + nTransactionFee > GetBalance())
        return _STR("Insufficient funds");

    // Parse bitcoin address
    CScript scriptPubKey;
    if (!scriptPubKey.SetBitcoinAddress(strAddress))
        return _STR("Invalid bitcoin address");

    return SendMoney(scriptPubKey, nValue, wtxNew, fAskFee);
}
