// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "atom.h"
#undef printf
#undef snprintf
#ifdef _WIN32
#define BOOST_ASIO_DISABLE_IOCP
#endif
#include <boost/asio.hpp>
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#define printf OutputDebugStringF
static string SolAddrToBase58(const unsigned char* p);
// MinGW 3.4.5 gets "fatal error: had to relocate PCH" if the json headers are
// precompiled in headers.h.  The problem might be when the pch file goes over
// a certain size around 145MB.  If we need access to json_spirit outside this
// file, we could use the compiled json_spirit option.

using boost::asio::ip::tcp;
using namespace json_spirit;

void ThreadRPCServer2(void* parg);
typedef Value(*rpcfn_type)(const Array& params, bool fHelp);
extern map<string, rpcfn_type> mapCallTable;

bool ExtractAddress(const CScript& scriptPubKey, string& addressRet)
{
    addressRet.clear();

    uint160 hash160;
    if (ExtractHash160(scriptPubKey, hash160))
    {
        addressRet = Hash160ToAddress(hash160);
        return true;
    }

    // P2PK: <pubkey> OP_CHECKSIG
    // Compressed pubkey (33 bytes): 0x21 <33 bytes> 0xac
    // Uncompressed pubkey (65 bytes): 0x41 <65 bytes> 0xac
    if (scriptPubKey.size() == 35 && scriptPubKey[0] == 33 && scriptPubKey[34] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 34);
        addressRet = Hash160ToAddress(Hash160(vchPubKey));
        return true;
    }
    if (scriptPubKey.size() == 67 && scriptPubKey[0] == 65 && scriptPubKey[66] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 66);
        addressRet = Hash160ToAddress(Hash160(vchPubKey));
        return true;
    }

    return false;
}





///
/// Note: This interface may still be subject to change.
///



Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "help\n"
            "List commands.");

    string strRet;
    set<rpcfn_type> setSeen;
    for (map<string, rpcfn_type>::iterator mi = mapCallTable.begin(); mi != mapCallTable.end(); ++mi)
    {
        if (setSeen.count((*mi).second))
            continue;
        setSeen.insert((*mi).second);
        try
        {
            Array params;
            (*(*mi).second)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strHelp.find('\n') != -1)
                strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + "\n";
        }
    }
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}


Value stop(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "stop\n"
            "Stop Bitok server.");

    // Shutdown will take long enough that the response should get back
    CreateThread(Shutdown, NULL);
    return "Bitok server stopping";
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the height of the most recent block in the longest block chain.");

    return nBestHeight;
}


Value getblocknumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblocknumber\n"
            "Returns the block number of the latest block in the longest block chain.");

    return nBestHeight;
}


Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = pindexBest;
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;

    return pblockindex->GetBlockHash().ToString();
}


Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblock <hash>\n"
            "Returns information about the block with the given hash.");

    string strHash = params[0].get_str();
    uint256 hash;
    hash.SetHex(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw runtime_error("Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];
    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    Object result;
    result.push_back(Pair("hash", block.GetHash().ToString()));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("previousblockhash", block.hashPrevBlock.ToString()));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.ToString()));
    result.push_back(Pair("time", (boost::int64_t)block.nTime));
    result.push_back(Pair("bits", (boost::int64_t)block.nBits));
    result.push_back(Pair("nonce", (boost::int64_t)block.nNonce));
    result.push_back(Pair("height", pblockindex->nHeight));

    Array txhashes;
    foreach(const CTransaction& tx, block.vtx)
        txhashes.push_back(tx.GetHash().ToString());
    result.push_back(Pair("tx", txhashes));

    if (pblockindex->pnext)
        result.push_back(Pair("nextblockhash", pblockindex->pnext->GetBlockHash().ToString()));

    return result;
}


Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "gettransaction <txid>\n"
            "Get detailed information about <txid>");

    string strTxid = params[0].get_str();
    uint256 hash;
    hash.SetHex(strTxid);

    Object result;

    CRITICAL_BLOCK(cs_mapWallet)
    {
        if (mapWallet.count(hash))
        {
            const CWalletTx& wtx = mapWallet[hash];

            result.push_back(Pair("txid", hash.ToString()));
            result.push_back(Pair("version", wtx.nVersion));
            result.push_back(Pair("time", (boost::int64_t)wtx.nTimeReceived));

            int nDepth = wtx.GetDepthInMainChain();
            result.push_back(Pair("confirmations", nDepth));

            if (wtx.hashBlock != 0)
                result.push_back(Pair("blockhash", wtx.hashBlock.ToString()));

            int64 nCredit = wtx.GetCredit(true);
            int64 nDebit = wtx.GetDebit();
            int64 nNet = nCredit - nDebit;

            result.push_back(Pair("amount", (double)nNet / (double)COIN));

            if (nDebit > 0)
                result.push_back(Pair("fee", (double)(nDebit - wtx.GetValueOut()) / (double)COIN));

            Array details;
            if (nDebit > 0)
            {
                for (int i = 0; i < wtx.vout.size(); i++)
                {
                    const CTxOut& txout = wtx.vout[i];
                    if (txout.IsMine())
                        continue;

                    Object entry;
                    entry.push_back(Pair("category", "send"));

                    string strAddress;
                    if (ExtractAddress(txout.scriptPubKey, strAddress))
                        entry.push_back(Pair("address", strAddress));

                    entry.push_back(Pair("amount", (double)(-txout.nValue) / (double)COIN));
                    details.push_back(entry);
                }
            }

            if (nCredit > 0)
            {
                for (int i = 0; i < wtx.vout.size(); i++)
                {
                    const CTxOut& txout = wtx.vout[i];
                    if (!txout.IsMine())
                        continue;

                    Object entry;
                    entry.push_back(Pair("category", wtx.IsCoinBase() ? "generate" : "receive"));

                    string strAddress;
                    if (ExtractAddress(txout.scriptPubKey, strAddress))
                        entry.push_back(Pair("address", strAddress));

                    entry.push_back(Pair("amount", (double)txout.nValue / (double)COIN));
                    details.push_back(entry);
                }
            }

            result.push_back(Pair("details", details));

            return result;
        }
    }

    CTxDB txdb("r");
    CTransaction tx;
    if (txdb.ReadDiskTx(hash, tx))
    {
        result.push_back(Pair("txid", hash.ToString()));
        result.push_back(Pair("version", tx.nVersion));

        Array vin;
        foreach(const CTxIn& txin, tx.vin)
        {
            Object entry;
            if (txin.prevout.IsNull())
            {
                entry.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end(), false)));
            }
            else
            {
                entry.push_back(Pair("txid", txin.prevout.hash.ToString()));
                entry.push_back(Pair("vout", (int)txin.prevout.n));
            }
            vin.push_back(entry);
        }
        result.push_back(Pair("vin", vin));

        Array vout;
        for (int i = 0; i < tx.vout.size(); i++)
        {
            const CTxOut& txout = tx.vout[i];
            Object entry;
            entry.push_back(Pair("value", (double)txout.nValue / (double)COIN));
            entry.push_back(Pair("n", i));

            string strAddress;
            if (ExtractAddress(txout.scriptPubKey, strAddress))
                entry.push_back(Pair("address", strAddress));

            vout.push_back(entry);
        }
        result.push_back(Pair("vout", vout));

        return result;
    }

    throw runtime_error("Transaction not found");
}


Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress <bitokaddress>\n"
            "Return information about <bitokaddress>.");

    string strAddress = params[0].get_str();

    Object result;
    result.push_back(Pair("address", strAddress));

    uint160 hash160;
    bool isValid = AddressToHash160(strAddress, hash160);
    result.push_back(Pair("isvalid", isValid));

    if (isValid)
    {
        bool isMine = mapPubKeys.count(hash160) > 0;
        result.push_back(Pair("ismine", isMine));

        if (isMine)
        {
            vector<unsigned char> vchPubKey = mapPubKeys[hash160];
            result.push_back(Pair("pubkey", HexStr(vchPubKey, false)));
        }

        CRITICAL_BLOCK(cs_mapAddressBook)
        {
            if (mapAddressBook.count(strAddress))
                result.push_back(Pair("label", mapAddressBook[strAddress]));
        }
    }

    return result;
}


Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "Returns the number of connections to other nodes.");

    return (int)vNodes.size();
}


double GetDifficulty()
{
    if (pindexBest == NULL)
        return 1.0;

    int nShift = (pindexBest->nBits >> 24) & 0xff;
    int nMantissa = pindexBest->nBits & 0x007fffff;

    if (nMantissa == 0)
        return 1.0;

    double dDiff = (double)0x007fffff / (double)nMantissa;

    while (nShift < 0x1e)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 0x1e)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the proof-of-work difficulty as a multiple of the minimum difficulty.");

    return GetDifficulty();
}


Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbalance\n"
            "Returns the server's available balance.");

    return ((double)GetBalance() / (double)COIN);
}


Value getgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "Returns true or false.");

    return (bool)fGenerateBitcoins;
}


Value setgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate <generate> [genproclimit]\n"
            "<generate> is true or false to turn generation on or off.\n"
            "Generation is limited to [genproclimit] processors, -1 is unlimited.");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    if (params.size() > 1)
    {
        int nGenProcLimit = params[1].get_int();
        fLimitProcessors = (nGenProcLimit != -1);
        CWalletDB().WriteSetting("fLimitProcessors", fLimitProcessors);
        if (nGenProcLimit != -1)
            CWalletDB().WriteSetting("nLimitProcessors", nLimitProcessors = nGenProcLimit);
    }

    GenerateBitcoins(fGenerate);
    return Value::null;
}


Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "Returns an object containing mining-related information.");

    Object obj;
    obj.push_back(Pair("blocks",            (int)nBestHeight));
    obj.push_back(Pair("currentblocksize",  (uint64_t)0));
    obj.push_back(Pair("currentblocktx",    (uint64_t)0));
    obj.push_back(Pair("difficulty",        (double)GetDifficulty()));
    obj.push_back(Pair("networkhashps",     (int64_t)GetNetworkHashPS()));
    obj.push_back(Pair("pooledtx",          (uint64_t)mapTransactions.size()));
    obj.push_back(Pair("chain",             string("main")));
    obj.push_back(Pair("generate",          (bool)fGenerateBitcoins));
    obj.push_back(Pair("genproclimit",      (int)(fLimitProcessors ? nLimitProcessors : -1)));
    return obj;
}


Value getblocktemplate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate [params]\n"
            "Returns data needed to construct a block to work on.\n"
            "See BIP 22 for full specification.");

    CKey key;
    key.MakeNewKey();

    CBlock* pblock = CreateNewBlock(key);
    if (!pblock)
        throw runtime_error("Out of memory");

    auto_ptr<CBlock> pblockAuto(pblock);

    Object result;
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));

    Array transactions;
    {
        CTxDB txdb("r");
        int nTargetHeight = nBestHeight + 1;
        for (unsigned int i = 1; i < pblock->vtx.size(); i++)
        {
            const CTransaction& tx = pblock->vtx[i];
            CDataStream ssTx(SER_NETWORK);
            ssTx << tx;

            Object entry;
            entry.push_back(Pair("data", HexStr(ssTx.begin(), ssTx.end(), false)));
            entry.push_back(Pair("txid", tx.GetHash().GetHex()));
            entry.push_back(Pair("hash", tx.GetHash().GetHex()));
            unsigned int nTxSigOps = 0;
            foreach(const CTxIn& txin, tx.vin)
                nTxSigOps += GetSigOpCount(txin.scriptSig);
            foreach(const CTxOut& txout, tx.vout)
                nTxSigOps += GetSigOpCount(txout.scriptPubKey);

            int64 nTxFee = 0;
            int64 nValueIn = 0;
            for (int j = 0; j < tx.vin.size(); j++)
            {
                COutPoint prevout = tx.vin[j].prevout;
                CTxIndex txindex;
                CTransaction txPrev;
                if (txdb.ReadTxIndex(prevout.hash, txindex) && txPrev.ReadFromDisk(txindex.pos))
                {
                    if (prevout.n < txPrev.vout.size())
                        nValueIn += txPrev.vout[prevout.n].nValue;
                }
            }
            nTxFee = nValueIn - tx.GetValueOut();
            if (nTxFee < 0)
                nTxFee = 0;

            double dPriority = ComputePriority(tx, txdb, nTargetHeight);

            entry.push_back(Pair("depends", Array()));
            entry.push_back(Pair("fee", (int64_t)nTxFee));
            entry.push_back(Pair("sigops", (int64_t)nTxSigOps));

            if (nTargetHeight >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                entry.push_back(Pair("priority", dPriority));

            transactions.push_back(entry);
        }
    }
    result.push_back(Pair("transactions", transactions));

    Object coinbaseaux;
    coinbaseaux.push_back(Pair("flags", string("")));
    result.push_back(Pair("coinbaseaux", coinbaseaux));

    result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));

    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
    result.push_back(Pair("target", hashTarget.GetHex()));

    result.push_back(Pair("mintime", (int64_t)pblock->nTime));

    Array mutable_arr;
    mutable_arr.push_back("time");
    mutable_arr.push_back("transactions");
    mutable_arr.push_back("prevblock");
    result.push_back(Pair("mutable", mutable_arr));

    result.push_back(Pair("noncerange", string("00000000ffffffff")));
    result.push_back(Pair("sigoplimit", (int64_t)20000));
    result.push_back(Pair("sizelimit", (int64_t)MAX_BLOCK_SIZE));
    result.push_back(Pair("curtime", (int64_t)GetTime()));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", (int64_t)(nBestHeight + 1)));

    return result;
}


Value submitblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "submitblock <hex data> [optional-params-obj]\n"
            "Attempts to submit new block to network.\n"
            "Returns null on success, error string on failure.");

    vector<unsigned char> blockData = ParseHex(params[0].get_str());
    CDataStream ssBlock(blockData, SER_NETWORK);
    CBlock* pblock = new CBlock();

    try {
        ssBlock >> *pblock;
    }
    catch (std::exception &e) {
        delete pblock;
        throw runtime_error("Block decode failed");
    }

    bool fAccepted = ProcessBlock(NULL, pblock);
    if (!fAccepted)
        return string("rejected");

    return Value::null;
}


static map<uint256, CBlock*> mapGetworkBlocks;
static map<uint256, CKey> mapGetworkKeys;
static CCriticalSection cs_getwork;

Value getwork(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwork [data]\n"
            "If [data] is not specified, returns work data.\n"
            "If [data] is specified, tries to solve the block.");

    if (params.size() == 0)
    {
        CKey key;
        key.MakeNewKey();

        CBlock* pblock = CreateNewBlock(key);
        if (!pblock)
            throw runtime_error("Out of memory");

        unsigned char pdata[128];
        memset(pdata, 0, sizeof(pdata));

        memcpy(pdata, &pblock->nVersion, 4);
        memcpy(pdata + 4, pblock->hashPrevBlock.begin(), 32);
        memcpy(pdata + 36, pblock->hashMerkleRoot.begin(), 32);
        memcpy(pdata + 68, &pblock->nTime, 4);
        memcpy(pdata + 72, &pblock->nBits, 4);
        unsigned int nNonce = 0;
        memcpy(pdata + 76, &nNonce, 4);

        for (int i = 0; i < 32; i++)
        {
            unsigned char tmp;
            tmp = pdata[i*4];
            pdata[i*4] = pdata[i*4+3];
            pdata[i*4+3] = tmp;
            tmp = pdata[i*4+1];
            pdata[i*4+1] = pdata[i*4+2];
            pdata[i*4+2] = tmp;
        }

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        uint256 hashMerkle = pblock->hashMerkleRoot;
        CRITICAL_BLOCK(cs_getwork)
        {
            mapGetworkBlocks[hashMerkle] = pblock;
            mapGetworkKeys[hashMerkle] = key;
            if (mapGetworkBlocks.size() > 100)
            {
                map<uint256, CBlock*>::iterator it = mapGetworkBlocks.begin();
                mapGetworkKeys.erase(it->first);
                delete it->second;
                mapGetworkBlocks.erase(it);
            }
        }

        string strTarget = HexStr(BEGIN(hashTarget), END(hashTarget), false);

        if (fDebug)
            printf("[getwork] bits=%08x target=%s height=%d\n",
                   pblock->nBits, strTarget.c_str(), nBestHeight + 1);

        Object result;
        result.push_back(Pair("data", HexStr(pdata, pdata + 128, false)));
        result.push_back(Pair("target", strTarget));
        result.push_back(Pair("algorithm", string("yespower")));

        return result;
    }
    else
    {
        if (fDebug)
            printf("[getwork] received submission, data length=%d\n", (int)params[0].get_str().size());

        vector<unsigned char> vchData = ParseHex(params[0].get_str());

        if (fDebug)
            printf("[getwork] parsed %d bytes\n", (int)vchData.size());

        if (vchData.size() < 80)
            throw runtime_error("Invalid parameter - data too short");

        for (size_t i = 0; i < vchData.size() / 4; i++)
        {
            unsigned char tmp;
            tmp = vchData[i*4];
            vchData[i*4] = vchData[i*4+3];
            vchData[i*4+3] = tmp;
            tmp = vchData[i*4+1];
            vchData[i*4+1] = vchData[i*4+2];
            vchData[i*4+2] = tmp;
        }

        unsigned int nVersion;
        uint256 hashPrevBlock;
        uint256 hashMerkleRoot;
        unsigned int nTime;
        unsigned int nBits;
        unsigned int nNonce;

        memcpy(&nVersion, &vchData[0], 4);
        memcpy(hashPrevBlock.begin(), &vchData[4], 32);
        memcpy(hashMerkleRoot.begin(), &vchData[36], 32);
        memcpy(&nTime, &vchData[68], 4);
        memcpy(&nBits, &vchData[72], 4);
        memcpy(&nNonce, &vchData[76], 4);

        if (fDebug)
            printf("[getwork] submit data: version=%u time=%u bits=%08x nonce=%u merkle=%s\n",
                   nVersion, nTime, nBits, nNonce, hashMerkleRoot.ToString().substr(0,16).c_str());

        CBlock* pblock = NULL;
        CKey key;
        CRITICAL_BLOCK(cs_getwork)
        {
            if (fDebug)
                printf("[getwork] mapGetworkBlocks has %d entries\n", (int)mapGetworkBlocks.size());
            if (mapGetworkBlocks.count(hashMerkleRoot))
            {
                pblock = mapGetworkBlocks[hashMerkleRoot];
                key = mapGetworkKeys[hashMerkleRoot];
            }
        }

        if (!pblock)
        {
            if (fDebug)
                printf("[getwork] ERROR: merkle root not found in map!\n");
            throw runtime_error("Stale work - block not found");
        }

        pblock->nNonce = nNonce;
        pblock->nTime = nTime;

        uint256 hash = pblock->GetPoWHash();
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        if (fDebug)
            printf("[getwork] submit: nonce=%u hash=%s target=%s\n",
                   nNonce, hash.ToString().c_str(), hashTarget.ToString().c_str());

        if (hash > hashTarget)
        {
            if (fDebug)
                printf("[getwork] hash does not meet target\n");
            return false;
        }

        printf("getwork: block solved! hash=%s\n", hash.ToString().c_str());

        CRITICAL_BLOCK(cs_main)
        {
            if (pblock->hashPrevBlock != hashBestChain)
                return false;

            if (!AddKey(key))
                throw runtime_error("Failed to add key to wallet");
        }

        if (!ProcessBlock(NULL, pblock))
            throw runtime_error("Block rejected");

        CRITICAL_BLOCK(cs_getwork)
        {
            mapGetworkBlocks.erase(hashMerkleRoot);
            mapGetworkKeys.erase(hashMerkleRoot);
        }

        return true;
    }
}


Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo");

    Object obj;
    obj.push_back(Pair("balance",       (double)GetBalance() / (double)COIN));
    obj.push_back(Pair("blocks",        (int)nBestHeight));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (fUseProxy ? addrProxy.ToStringIPPort() : string())));
    obj.push_back(Pair("generate",      (bool)fGenerateBitcoins));
    obj.push_back(Pair("genproclimit",  (int)(fLimitProcessors ? nLimitProcessors : -1)));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    return obj;
}


Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress [label]\n"
            "Returns a new Bitok address for receiving payments.  "
            "If [label] is specified (recommended), it is added to the address book "
            "so payments received with the address will be labeled.");

    // Parse the label first so we don't generate a key if there's an error
    string strLabel;
    if (params.size() > 0)
        strLabel = params[0].get_str();

    // Generate a new key that is added to wallet
    string strAddress = PubKeyToAddress(GenerateNewKey());

    SetAddressBookName(strAddress, strLabel);
    return strAddress;
}


Value setlabel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setlabel <bitokaddress> <label>\n"
            "Sets the label associated with the given address.");

    string strAddress = params[0].get_str();
    string strLabel;
    if (params.size() > 1)
        strLabel = params[1].get_str();

    SetAddressBookName(strAddress, strLabel);
    return Value::null;
}


Value getlabel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getlabel <bitokaddress>\n"
            "Returns the label associated with the given address.");

    string strAddress = params[0].get_str();

    string strLabel;
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        map<string, string>::iterator mi = mapAddressBook.find(strAddress);
        if (mi != mapAddressBook.end() && !(*mi).second.empty())
            strLabel = (*mi).second;
    }
    return strLabel;
}


Value getaddressesbylabel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbylabel <label>\n"
            "Returns the list of addresses with the given label.");

    string strLabel = params[0].get_str();

    // Find all addresses that have the given label
    Array ret;
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        foreach(const PAIRTYPE(string, string)& item, mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strName = item.second;
            if (strName == strLabel)
            {
                // We're only adding valid Bitok addresses and not ip addresses
                CScript scriptPubKey;
                if (scriptPubKey.SetBitcoinAddress(strAddress))
                    ret.push_back(strAddress);
            }
        }
    }
    return ret;
}


Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoaddress <address> <amount> [comment] [comment-to]\n"
            "<address> can be a regular address or an ok-address (stealth).\n"
            "<amount> is a real and is rounded to the nearest 0.01");

    string strAddress = params[0].get_str();

    if (params[1].get_real() <= 0.0 || params[1].get_real() > 21000000.0)
        throw runtime_error("Invalid amount");
    int64 nAmount = roundint64(params[1].get_real() * 100.00) * CENT;

    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["message"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    CStealthAddress sxAddr;
    if (sxAddr.SetEncoded(strAddress))
    {
        vector<unsigned char> vchEphemPub, vchDestPubKey, vchSharedSecret;
        if (!StealthEphemeral(sxAddr, vchEphemPub, vchDestPubKey, vchSharedSecret))
            throw runtime_error("Failed to derive stealth destination");

        CScript scriptDestPubKey;
        scriptDestPubKey.SetBitcoinAddress(vchDestPubKey);

        vector<unsigned char> vchOpReturnData = BuildStealthOpReturn(vchEphemPub);
        CScript scriptOpReturn;
        scriptOpReturn << OP_RETURN << vchOpReturnData;

        wtx.mapValue["stealth_address"] = strAddress;

        CKey changeKey;
        int64 nFeeRequired;

        if (!CreateStealthTransaction(scriptDestPubKey, scriptOpReturn, nAmount, wtx, changeKey, nFeeRequired))
        {
            string strError;
            if (nAmount + nFeeRequired > GetBalance())
                strError = strprintf("Error: This transaction requires a fee of %s", FormatMoney(nFeeRequired).c_str());
            else
                strError = "Error: Transaction creation failed";
            throw runtime_error(strError);
        }

        if (!CommitTransaction(wtx, changeKey))
            throw runtime_error("Error: The transaction was rejected");

        Object result;
        result.push_back(Pair("txid", wtx.GetHash().ToString()));
        result.push_back(Pair("dest_address", PubKeyToAddress(vchDestPubKey)));
        result.push_back(Pair("ephemeral_pubkey", HexStr(vchEphemPub.begin(), vchEphemPub.end(), false)));
        return result;
    }

    string strError = SendMoneyToBitcoinAddress(strAddress, nAmount, wtx);
    if (strError != "")
        throw runtime_error(strError);
    return wtx.GetHash().ToString();
}


Value listtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listtransactions [count=10] [includegenerated=true]\n"
            "Returns up to [count] most recent transactions.\n"
            "Returns array of objects with: txid, category, amount, confirmations, time, address");

    int64 nCount = 10;
    if (params.size() > 0)
        nCount = params[0].get_int64();
    bool fIncludeGenerated = true;
    if (params.size() > 1)
        fIncludeGenerated = params[1].get_bool();

    vector<pair<int64, uint256> > vSorted;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            vSorted.push_back(make_pair(wtx.nTimeReceived, wtx.GetHash()));
        }
    }
    sort(vSorted.rbegin(), vSorted.rend());

    Array ret;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (vector<pair<int64, uint256> >::iterator it = vSorted.begin(); it != vSorted.end() && (int64)ret.size() < nCount; ++it)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find((*it).second);
            if (mi == mapWallet.end())
                continue;
            const CWalletTx& wtx = (*mi).second;

            bool fGenerated = wtx.IsCoinBase();
            if (fGenerated && !fIncludeGenerated)
                continue;

            int nDepth = wtx.GetDepthInMainChain();
            int64 nTime = wtx.nTimeReceived;
            string strTxid = wtx.GetHash().ToString();

            if (fGenerated)
            {
                if (nDepth < GetCoinbaseMaturity())
                    continue;
                int64 nCredit = wtx.GetCredit(true);
                Object entry;
                entry.push_back(Pair("txid", strTxid));
                entry.push_back(Pair("category", "generate"));
                entry.push_back(Pair("amount", (double)nCredit / (double)COIN));
                entry.push_back(Pair("confirmations", nDepth));
                entry.push_back(Pair("time", (boost::int64_t)nTime));
                ret.push_back(entry);
            }
            else
            {
                int64 nDebit = wtx.GetDebit();
                int64 nCredit = wtx.GetCredit(true);

                if (nDebit > 0)
                {
                    int64 nValueOut = wtx.GetValueOut();
                    int64 nFee = nDebit - nValueOut;

                    for (int i = 0; i < wtx.vout.size(); i++)
                    {
                        const CTxOut& txout = wtx.vout[i];
                        if (txout.IsMine())
                            continue;

                        if (txout.nValue == 0 && txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN)
                            continue;

                        string strAddress;
                        ExtractAddress(txout.scriptPubKey, strAddress);

                        Object entry;
                        entry.push_back(Pair("txid", strTxid));
                        entry.push_back(Pair("category", "send"));
                        entry.push_back(Pair("amount", (double)(-txout.nValue) / (double)COIN));
                        entry.push_back(Pair("fee", (double)(-nFee) / (double)COIN));
                        if (!strAddress.empty())
                            entry.push_back(Pair("address", strAddress));
                        if (wtx.mapValue.count("stealth_address") && !wtx.mapValue.find("stealth_address")->second.empty())
                            entry.push_back(Pair("stealth_address", wtx.mapValue.find("stealth_address")->second));
                        entry.push_back(Pair("confirmations", nDepth));
                        entry.push_back(Pair("time", (boost::int64_t)nTime));
                        ret.push_back(entry);
                        nFee = 0;
                    }
                }

                if (nCredit > 0)
                {
                    for (int i = 0; i < wtx.vout.size(); i++)
                    {
                        const CTxOut& txout = wtx.vout[i];
                        if (!txout.IsMine())
                            continue;

                        string strAddress;
                        ExtractAddress(txout.scriptPubKey, strAddress);

                        Object entry;
                        entry.push_back(Pair("txid", strTxid));
                        entry.push_back(Pair("category", "receive"));
                        entry.push_back(Pair("amount", (double)txout.nValue / (double)COIN));
                        if (!strAddress.empty())
                            entry.push_back(Pair("address", strAddress));
                        entry.push_back(Pair("confirmations", nDepth));
                        entry.push_back(Pair("time", (boost::int64_t)nTime));
                        ret.push_back(entry);
                    }
                }
            }
        }
    }

    return ret;
}


Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaddress <bitokaddress> [minconf=1]\n"
            "Returns the total amount received by <bitokaddress> in transactions with at least [minconf] confirmations.");

    // Bitok address
    string strAddress = params[0].get_str();
    CScript scriptPubKey;
    if (!scriptPubKey.SetBitcoinAddress(strAddress))
        throw runtime_error("Invalid Bitok address");
    if (!IsMine(scriptPubKey))
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    int64 nAmount = 0;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            foreach(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    if (wtx.GetDepthInMainChain() >= nMinDepth)
                        nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}


Value getreceivedbylabel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbylabel <label> [minconf=1]\n"
            "Returns the total amount received by addresses with <label> in transactions with at least [minconf] confirmations.");

    // Get the set of pub keys that have the label
    string strLabel = params[0].get_str();
    set<CScript> setPubKey;
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        foreach(const PAIRTYPE(string, string)& item, mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strName = item.second;
            if (strName == strLabel)
            {
                // We're only counting our own valid Bitok addresses and not ip addresses
                CScript scriptPubKey;
                if (scriptPubKey.SetBitcoinAddress(strAddress))
                    if (IsMine(scriptPubKey))
                        setPubKey.insert(scriptPubKey);
            }
        }
    }

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    int64 nAmount = 0;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            foreach(const CTxOut& txout, wtx.vout)
                if (setPubKey.count(txout.scriptPubKey))
                    if (wtx.GetDepthInMainChain() >= nMinDepth)
                        nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}


struct tallyitem
{
    int64 nAmount;
    int nConf;
    tallyitem()
    {
        nAmount = 0;
        nConf = INT_MAX;
    }
};

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "Returns the hash of the best (tip) block in the longest block chain.");

    return hashBestChain.ToString();
}


Value getrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getrawtransaction <txid> [verbose=0]\n"
            "If verbose=0, returns a string that is serialized, hex-encoded data for <txid>.\n"
            "If verbose is non-zero, returns an Object with information about <txid>.");

    string strTxid = params[0].get_str();
    uint256 hash;
    hash.SetHex(strTxid);

    bool fVerbose = false;
    if (params.size() > 1)
        fVerbose = (params[1].get_int() != 0);

    CTransaction tx;
    uint256 hashBlock = 0;
    CTxIndex txindex;

    CRITICAL_BLOCK(cs_mapTransactions)
    {
        if (mapTransactions.count(hash))
        {
            tx = mapTransactions[hash];
        }
    }

    if (tx.IsNull())
    {
        CTxDB txdb("r");
        if (!txdb.ReadDiskTx(hash, tx, txindex))
            throw runtime_error("No information available about transaction");
    }

    CDataStream ssTx;
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end(), false);

    if (!fVerbose)
        return strHex;

    Object result;
    result.push_back(Pair("hex", strHex));
    result.push_back(Pair("txid", hash.ToString()));
    result.push_back(Pair("version", tx.nVersion));
    result.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));

    if (!txindex.pos.IsNull())
    {
        CRITICAL_BLOCK(cs_main)
        {
            for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
            {
                CBlockIndex* pindex = mi->second;
                if (pindex->nFile == txindex.pos.nFile && pindex->nBlockPos == txindex.pos.nBlockPos)
                {
                    result.push_back(Pair("blockhash", pindex->GetBlockHash().ToString()));
                    result.push_back(Pair("blockheight", pindex->nHeight));
                    result.push_back(Pair("blocktime", (boost::int64_t)pindex->nTime));
                    if (pindex->IsInMainChain())
                        result.push_back(Pair("confirmations", nBestHeight - pindex->nHeight + 1));
                    else
                        result.push_back(Pair("confirmations", 0));
                    break;
                }
            }
        }
    }

    Array vin;
    foreach(const CTxIn& txin, tx.vin)
    {
        Object entry;
        if (txin.prevout.IsNull())
        {
            entry.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end(), false)));
        }
        else
        {
            entry.push_back(Pair("txid", txin.prevout.hash.ToString()));
            entry.push_back(Pair("vout", (int)txin.prevout.n));
            entry.push_back(Pair("scriptSig", HexStr(txin.scriptSig.begin(), txin.scriptSig.end(), false)));
        }
        entry.push_back(Pair("sequence", (boost::int64_t)txin.nSequence));
        vin.push_back(entry);
    }
    result.push_back(Pair("vin", vin));

    Array vout;
    for (int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        Object entry;
        entry.push_back(Pair("value", (double)txout.nValue / (double)COIN));
        entry.push_back(Pair("n", i));
        entry.push_back(Pair("scriptPubKey", HexStr(txout.scriptPubKey.begin(), txout.scriptPubKey.end(), false)));

        string strAddress;
        if (ExtractAddress(txout.scriptPubKey, strAddress))
            entry.push_back(Pair("address", strAddress));

        vout.push_back(entry);
    }
    result.push_back(Pair("vout", vout));

    return result;
}


Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    Array ret;
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin();
             mi != mapTransactions.end(); ++mi)
        {
            ret.push_back((*mi).first.ToString());
        }
    }
    return ret;
}


Value gettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout <txid> <n> [includemempool=true]\n"
            "Returns details about an unspent transaction output.");

    string strTxid = params[0].get_str();
    uint256 hash;
    hash.SetHex(strTxid);
    int nOutput = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CTransaction tx;
    CTxIndex txindex;
    bool fFoundInMempool = false;

    if (fMempool)
    {
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            if (mapTransactions.count(hash))
            {
                tx = mapTransactions[hash];
                fFoundInMempool = true;
            }
        }
    }

    if (!fFoundInMempool)
    {
        CTxDB txdb("r");
        if (!txdb.ReadDiskTx(hash, tx, txindex))
            return Value::null;

        if (nOutput < 0 || nOutput >= (int)txindex.vSpent.size())
            return Value::null;

        if (!txindex.vSpent[nOutput].IsNull())
            return Value::null;
    }

    if (nOutput < 0 || nOutput >= (int)tx.vout.size())
        return Value::null;

    const CTxOut& txout = tx.vout[nOutput];

    Object result;
    result.push_back(Pair("bestblock", hashBestChain.ToString()));
    result.push_back(Pair("value", (double)txout.nValue / (double)COIN));
    result.push_back(Pair("scriptPubKey", HexStr(txout.scriptPubKey.begin(), txout.scriptPubKey.end(), false)));

    string strAddress;
    if (ExtractAddress(txout.scriptPubKey, strAddress))
        result.push_back(Pair("address", strAddress));

    if (fFoundInMempool)
    {
        result.push_back(Pair("confirmations", 0));
    }
    else if (!txindex.pos.IsNull())
    {
        CRITICAL_BLOCK(cs_main)
        {
            for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
            {
                CBlockIndex* pindex = mi->second;
                if (pindex->nFile == txindex.pos.nFile && pindex->nBlockPos == txindex.pos.nBlockPos)
                {
                    result.push_back(Pair("height", pindex->nHeight));
                    if (pindex->IsInMainChain())
                        result.push_back(Pair("confirmations", nBestHeight - pindex->nHeight + 1));
                    else
                        result.push_back(Pair("confirmations", 0));
                    break;
                }
            }
        }
    }

    result.push_back(Pair("coinbase", tx.IsCoinBase()));

    return result;
}


Value listunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listunspent [minconf=1] [maxconf=9999999]\n"
            "Returns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.");

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    Array results;
    CTxDB txdb("r");

    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;

            if (wtx.IsCoinBase() && wtx.GetBlocksToMaturity() > 0)
                continue;

            int nDepth = wtx.GetDepthInMainChain();
            if (nDepth < nMinDepth || nDepth > nMaxDepth)
                continue;

            for (int i = 0; i < wtx.vout.size(); i++)
            {
                const CTxOut& txout = wtx.vout[i];

                if (!txout.IsMine())
                    continue;

                CTxIndex txindex;
                if (!txdb.ReadTxIndex(wtx.GetHash(), txindex))
                    continue;

                if (i < txindex.vSpent.size() && !txindex.vSpent[i].IsNull())
                    continue;

                Object entry;
                entry.push_back(Pair("txid", wtx.GetHash().ToString()));
                entry.push_back(Pair("vout", i));

                string strAddress;
                if (ExtractAddress(txout.scriptPubKey, strAddress))
                    entry.push_back(Pair("address", strAddress));

                entry.push_back(Pair("scriptPubKey", HexStr(txout.scriptPubKey.begin(), txout.scriptPubKey.end(), false)));
                entry.push_back(Pair("amount", (double)txout.nValue / (double)COIN));
                entry.push_back(Pair("confirmations", nDepth));

                results.push_back(entry);
            }
        }
    }

    return results;
}


Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "Returns data about each connected network node.");

    Array ret;
    CRITICAL_BLOCK(cs_vNodes)
    {
        foreach(CNode* pnode, vNodes)
        {
            Object obj;
            obj.push_back(Pair("addr", pnode->addr.ToString()));
            obj.push_back(Pair("services", strprintf("%08" PRI64x, pnode->nServices)));
            obj.push_back(Pair("lastsend", (boost::int64_t)pnode->nLastSend));
            obj.push_back(Pair("lastrecv", (boost::int64_t)pnode->nLastRecv));
            obj.push_back(Pair("conntime", (boost::int64_t)pnode->nTimeConnected));
            obj.push_back(Pair("version", pnode->nVersion));
            obj.push_back(Pair("inbound", pnode->fInbound));
            obj.push_back(Pair("startingheight", pnode->nStartingHeight));
            ret.push_back(obj);
        }
    }
    return ret;
}


Value ListReceived(const Array& params, bool fByLabels)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    // Tally
    map<uint160, tallyitem> mapTally;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            int nDepth = wtx.GetDepthInMainChain();
            if (nDepth < nMinDepth)
                continue;

            foreach(const CTxOut& txout, wtx.vout)
            {
                // Only counting our own Bitok addresses and not ip addresses
                uint160 hash160 = txout.scriptPubKey.GetBitcoinAddressHash160();
                if (hash160 == 0 || !mapPubKeys.count(hash160)) // IsMine
                    continue;

                tallyitem& item = mapTally[hash160];
                item.nAmount += txout.nValue;
                item.nConf = min(item.nConf, nDepth);
            }
        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapLabelTally;
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        foreach(const PAIRTYPE(string, string)& item, mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strLabel = item.second;
            uint160 hash160;
            if (!AddressToHash160(strAddress, hash160))
                continue;
            map<uint160, tallyitem>::iterator it = mapTally.find(hash160);
            if (it == mapTally.end() && !fIncludeEmpty)
                continue;

            int64 nAmount = 0;
            int nConf = INT_MAX;
            if (it != mapTally.end())
            {
                nAmount = (*it).second.nAmount;
                nConf = (*it).second.nConf;
            }

            if (fByLabels)
            {
                tallyitem& item = mapLabelTally[strLabel];
                item.nAmount += nAmount;
                item.nConf = min(item.nConf, nConf);
            }
            else
            {
                Object obj;
                obj.push_back(Pair("address",       strAddress));
                obj.push_back(Pair("label",         strLabel));
                obj.push_back(Pair("amount",        (double)nAmount / (double)COIN));
                obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
                ret.push_back(obj);
            }
        }
    }

    if (fByLabels)
    {
        for (map<string, tallyitem>::iterator it = mapLabelTally.begin(); it != mapLabelTally.end(); ++it)
        {
            int64 nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            obj.push_back(Pair("label",         (*it).first));
            obj.push_back(Pair("amount",        (double)nAmount / (double)COIN));
            obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaddress [minconf=1] [includeempty=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include addresses that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"address\" : receiving address\n"
            "  \"label\" : the label of the receiving address\n"
            "  \"amount\" : total amount received by the address\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    return ListReceived(params, false);
}

Value listreceivedbylabel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbylabel [minconf=1] [includeempty=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include labels that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"label\" : the label of the receiving addresses\n"
            "  \"amount\" : total amount received by addresses with this label\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    return ListReceived(params, true);
}













Value getblockheader(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockheader <hash>\n"
            "Returns header information for the block with the given hash.");

    string strHash = params[0].get_str();
    uint256 hash;
    hash.SetHex(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw runtime_error("Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];

    Object result;
    result.push_back(Pair("hash", pblockindex->GetBlockHash().ToString()));
    result.push_back(Pair("version", pblockindex->nVersion));
    uint256 hashPrevBlock = (pblockindex->pprev ? pblockindex->pprev->GetBlockHash() : uint256(0));
    result.push_back(Pair("previousblockhash", hashPrevBlock.ToString()));
    result.push_back(Pair("merkleroot", pblockindex->hashMerkleRoot.ToString()));
    result.push_back(Pair("time", (boost::int64_t)pblockindex->nTime));
    result.push_back(Pair("bits", (boost::int64_t)pblockindex->nBits));
    result.push_back(Pair("nonce", (boost::int64_t)pblockindex->nNonce));
    result.push_back(Pair("height", pblockindex->nHeight));
    result.push_back(Pair("confirmations", 1 + nBestHeight - pblockindex->nHeight));
    if (pblockindex->pnext)
        result.push_back(Pair("nextblockhash", pblockindex->pnext->GetBlockHash().ToString()));

    CDataStream ssHeader(SER_NETWORK | SER_BLOCKHEADERONLY);
    CBlock header;
    header.nVersion       = pblockindex->nVersion;
    header.hashPrevBlock  = hashPrevBlock;
    header.hashMerkleRoot = pblockindex->hashMerkleRoot;
    header.nTime          = pblockindex->nTime;
    header.nBits          = pblockindex->nBits;
    header.nNonce         = pblockindex->nNonce;
    ssHeader << header;
    result.push_back(Pair("hex", HexStr(ssHeader.begin(), ssHeader.end(), false)));

    return result;
}


Value sendrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "sendrawtransaction <hex string>\n"
            "Submits raw transaction (serialized, hex-encoded) to local node and network.\n"
            "Returns transaction hash.");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction tx;

    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    uint256 hashTx = tx.GetHash();

    CRITICAL_BLOCK(cs_mapTransactions)
    {
        if (mapTransactions.count(hashTx))
            return hashTx.ToString();
    }

    CTxDB txdb("r");
    CTxIndex txindex;
    if (txdb.ReadTxIndex(hashTx, txindex))
        return hashTx.ToString();

    bool fMissingInputs = false;
    if (!tx.AcceptTransaction(true, &fMissingInputs))
    {
        if (fMissingInputs)
            throw runtime_error("Missing inputs");
        throw runtime_error("Transaction rejected");
    }

    CInv inv(MSG_TX, hashTx);
    CDataStream ss(SER_NETWORK);
    ss << tx;
    RelayMessage(inv, ss);

    return hashTx.ToString();
}


Value gettxoutproof(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettxoutproof <txid> [blockhash]\n"
            "Returns a hex-encoded merkle proof that the transaction was included in a block.\n"
            "Optionally specify the block hash to look in.");

    string strTxid = params[0].get_str();
    uint256 hashTx;
    hashTx.SetHex(strTxid);

    uint256 hashBlock = 0;
    if (params.size() > 1)
    {
        string strBlock = params[1].get_str();
        hashBlock.SetHex(strBlock);
    }

    if (hashBlock == 0)
    {
        CRITICAL_BLOCK(cs_mapWallet)
        {
            if (mapWallet.count(hashTx))
                hashBlock = mapWallet[hashTx].hashBlock;
        }

        if (hashBlock == 0)
        {
            CTxDB txdb("r");
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(hashTx, txindex))
                throw runtime_error("Transaction not yet in a block");

            CBlock block;
            if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, true))
                throw runtime_error("Block not found on disk");

            hashBlock = block.GetHash();
        }
    }

    if (hashBlock == 0)
        throw runtime_error("Transaction not found in any block");

    if (mapBlockIndex.count(hashBlock) == 0)
        throw runtime_error("Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    bool fFound = false;
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        if (block.vtx[i].GetHash() == hashTx)
        {
            fFound = true;
            break;
        }
    }
    if (!fFound)
        throw runtime_error("Transaction not found in specified block");

    set<uint256> txids;
    txids.insert(hashTx);
    CMerkleBlock merkleBlock(block, txids);

    CDataStream ssMB(SER_NETWORK);
    ssMB << merkleBlock;

    return HexStr(ssMB.begin(), ssMB.end(), false);
}


Value verifytxoutproof(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "verifytxoutproof <hex proof>\n"
            "Verifies a merkle proof and returns the transaction ids it commits to.\n"
            "Returns array of transaction ids the proof commits to, or empty if invalid.");

    vector<unsigned char> proofData = ParseHex(params[0].get_str());
    CDataStream ssMB(proofData, SER_NETWORK);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    vector<uint256> vMatch;
    uint256 hashMerkleRoot = merkleBlock.txn.ExtractMatches(vMatch);

    if (hashMerkleRoot == 0)
        return Array();

    uint256 hashBlock = merkleBlock.header.GetHash();
    if (mapBlockIndex.count(hashBlock) == 0)
        return Array();

    CBlockIndex* pindex = mapBlockIndex[hashBlock];
    if (!pindex->IsInMainChain())
        return Array();

    if (pindex->hashMerkleRoot != hashMerkleRoot)
        return Array();

    Array result;
    for (unsigned int i = 0; i < vMatch.size(); i++)
        result.push_back(vMatch[i].ToString());

    return result;
}


Value dumpprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey <address>\n"
            "Reveals the private key corresponding to <address>.\n"
            "For regular addresses, returns a WIF-encoded private key.\n"
            "For ok-addresses (stealth), returns an SK... stealth secret.");

    string strAddress = params[0].get_str();

    CStealthAddress sxAddr;
    if (sxAddr.SetEncoded(strAddress))
    {
        bool fFound = false;
        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            foreach(const CStealthAddress& sx, vStealthAddresses)
            {
                if (sx.scan_pubkey == sxAddr.scan_pubkey && sx.spend_pubkey == sxAddr.spend_pubkey)
                {
                    sxAddr.label = sx.label;
                    fFound = true;
                    break;
                }
            }
        }

        if (!fFound)
            throw runtime_error("Stealth address not found in wallet");

        CWalletDB walletdb;

        CPrivKey vchScanPrivKey;
        if (!walletdb.ReadStealthScanKey(sxAddr.scan_pubkey, vchScanPrivKey))
            throw runtime_error("Failed to read scan private key");

        CPrivKey vchSpendPrivKey;
        if (!walletdb.ReadStealthSpendKey(sxAddr.spend_pubkey, vchSpendPrivKey))
            throw runtime_error("Failed to read spend private key");

        CKey scanKey;
        if (!scanKey.SetPrivKey(vchScanPrivKey))
            throw runtime_error("Failed to decode scan private key");

        CKey spendKey;
        if (!spendKey.SetPrivKey(vchSpendPrivKey))
            throw runtime_error("Failed to decode spend private key");

        vector<unsigned char> vchScanSecret = scanKey.GetSecret();
        vector<unsigned char> vchSpendSecret = spendKey.GetSecret();

        string strCombinedKey = EncodeStealthSecret(vchScanSecret, vchSpendSecret);
        if (strCombinedKey.empty())
            throw runtime_error("Failed to encode stealth secret");

        Object result;
        result.push_back(Pair("stealthaddress", sxAddr.Encoded()));
        result.push_back(Pair("label", sxAddr.label));
        result.push_back(Pair("stealth_secret", strCombinedKey));
        return result;
    }

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid address");

    CKey key;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!mapPubKeys.count(hash160))
            throw runtime_error("Private key for address " + strAddress + " is not known");
        vector<unsigned char> vchPubKey = mapPubKeys[hash160];
        if (!mapKeys.count(vchPubKey))
            throw runtime_error("Private key for address " + strAddress + " is not known");
        if (!key.SetPrivKey(mapKeys[vchPubKey]))
            throw runtime_error("Failed to read private key");
    }

    vector<unsigned char> vchSecret = key.GetSecret();
    vector<unsigned char> vchWIF;
    vchWIF.push_back(128);
    vchWIF.insert(vchWIF.end(), vchSecret.begin(), vchSecret.end());
    return EncodeBase58Check(vchWIF);
}


Value importprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importprivkey <privkey> [label] [rescan=true]\n"
            "Adds a private key (as returned by dumpprivkey) to your wallet.\n"
            "Accepts WIF keys for regular addresses or SK... keys for stealth addresses.\n"
            "If rescan is true (default), the blockchain will be rescanned for transactions.");

    string strSecret = params[0].get_str();
    string strLabel;
    if (params.size() > 1)
        strLabel = params[1].get_str();
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    vector<unsigned char> vchScanSecret, vchSpendSecret;
    if (DecodeStealthSecret(strSecret, vchScanSecret, vchSpendSecret))
    {
        CKey scanKey;
        if (!scanKey.SetSecret(vchScanSecret))
            throw runtime_error("Invalid scan secret key");

        CKey spendKey;
        if (!spendKey.SetSecret(vchSpendSecret))
            throw runtime_error("Invalid spend secret key");

        CStealthAddress sxAddr;
        sxAddr.scan_pubkey = scanKey.GetCompressedPubKey();
        sxAddr.spend_pubkey = spendKey.GetCompressedPubKey();
        sxAddr.label = strLabel;

        if (!sxAddr.IsValid())
            throw runtime_error("Derived stealth address is invalid");

        bool fExists = false;
        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            foreach(const CStealthAddress& sx, vStealthAddresses)
            {
                if (sx.scan_pubkey == sxAddr.scan_pubkey && sx.spend_pubkey == sxAddr.spend_pubkey)
                {
                    fExists = true;
                    break;
                }
            }
        }

        if (fExists)
            throw runtime_error("Stealth address already exists in wallet");

        CWalletDB walletdb;
        if (!walletdb.WriteStealthAddress(sxAddr))
            throw runtime_error("Failed to write stealth address to wallet");
        if (!walletdb.WriteStealthScanKey(sxAddr.scan_pubkey, scanKey.GetPrivKey()))
            throw runtime_error("Failed to write stealth scan key");
        if (!walletdb.WriteStealthSpendKey(sxAddr.spend_pubkey, spendKey.GetPrivKey()))
            throw runtime_error("Failed to write stealth spend key");

        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            vStealthAddresses.push_back(sxAddr);
        }

        printf("[STEALTH] Imported stealth address %s\n", sxAddr.Encoded().substr(0, 16).c_str());

        if (fRescan)
        {
            set<uint160> setOnChainAddresses;
            printf("[STEALTH] Rescanning blockchain for stealth payments (collecting address index)\n");
            ScanWalletTransactions(pindexGenesisBlock, 0, &setOnChainAddresses);

            printf("[STEALTH] Scanning for deterministic change keys\n");
            ScanStealthChangeKeys(vchSpendSecret, setOnChainAddresses);

            printf("[STEALTH] Re-scanning to pick up change transactions\n");
            ScanWalletTransactions(pindexGenesisBlock);
            RescanAtom();
        }

        Object result;
        result.push_back(Pair("stealthaddress", sxAddr.Encoded()));
        result.push_back(Pair("label", strLabel));
        result.push_back(Pair("scan_pubkey", HexStr(sxAddr.scan_pubkey.begin(), sxAddr.scan_pubkey.end(), false)));
        result.push_back(Pair("spend_pubkey", HexStr(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end(), false)));
        result.push_back(Pair("rescanned", fRescan));
        return result;
    }

    vector<unsigned char> vchWIF;
    if (!DecodeBase58Check(strSecret, vchWIF))
        throw runtime_error("Invalid private key encoding");
    if (vchWIF.size() < 33 || vchWIF[0] != 128)
        throw runtime_error("Invalid private key version");

    vector<unsigned char> vchSecret(vchWIF.begin() + 1, vchWIF.begin() + 33);

    CKey key;
    if (!key.SetSecret(vchSecret))
        throw runtime_error("Invalid private key");

    if (!AddKey(key))
        throw runtime_error("Error adding key to wallet");

    string strAddress = PubKeyToAddress(key.GetPubKey());
    if (!strLabel.empty())
        SetAddressBookName(strAddress, strLabel);

    if (fRescan)
    {
        printf("[WALLET] Rescanning blockchain for imported key %s\n", strAddress.c_str());
        ScanWalletTransactions(pindexGenesisBlock);
        RescanAtom();
    }

    return strAddress;
}


Value rescanwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "rescanwallet\n"
            "Rescans the blockchain for wallet transactions and rebuilds all ATOM state.\n"
            "This finds any transactions belonging to wallet keys that may be missing\n"
            "from the wallet, then erases and rebuilds all ATOM balances and transaction\n"
            "history by replaying every block from genesis.");

    printf("[WALLET] Rescanning blockchain for all wallet transactions\n");
    int nFound = ScanWalletTransactions(pindexGenesisBlock);
    RescanAtom();

    Object result;
    result.push_back(Pair("found", nFound));
    return result;
}


static string ClassifyScript(const CScript& scriptPubKey, int& nRequiredRet, Array& addressesRet)
{
    nRequiredRet = 0;
    addressesRet = Array();

    uint160 hash160;
    if (ExtractHash160(scriptPubKey, hash160))
    {
        nRequiredRet = 1;
        addressesRet.push_back(Hash160ToAddress(hash160));
        return "pubkeyhash";
    }

    if (scriptPubKey.size() == 35 && scriptPubKey[0] == 33 && scriptPubKey[34] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 34);
        nRequiredRet = 1;
        addressesRet.push_back(Hash160ToAddress(Hash160(vchPubKey)));
        return "pubkey";
    }

    if (scriptPubKey.size() == 67 && scriptPubKey[0] == 65 && scriptPubKey[66] == OP_CHECKSIG)
    {
        vector<unsigned char> vchPubKey(scriptPubKey.begin() + 1, scriptPubKey.begin() + 66);
        nRequiredRet = 1;
        addressesRet.push_back(Hash160ToAddress(Hash160(vchPubKey)));
        return "pubkey";
    }

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        if (!scriptPubKey.GetOp(pc, opcode) || opcode < OP_1 || opcode > OP_16)
            break;
        int nRequired = (int)opcode - (int)(OP_1 - 1);

        vector<vector<unsigned char> > vPubKeys;
        while (scriptPubKey.GetOp(pc, opcode, vchData))
        {
            if (opcode >= OP_1 && opcode <= OP_16)
            {
                int nKeys = (int)opcode - (int)(OP_1 - 1);
                opcodetype opcodeCheck;
                if (scriptPubKey.GetOp(pc, opcodeCheck) &&
                    opcodeCheck == OP_CHECKMULTISIG &&
                    pc == scriptPubKey.end())
                {
                    if ((int)vPubKeys.size() == nKeys && nRequired <= nKeys)
                    {
                        nRequiredRet = nRequired;
                        for (int i = 0; i < (int)vPubKeys.size(); i++)
                            addressesRet.push_back(Hash160ToAddress(Hash160(vPubKeys[i])));
                        return "multisig";
                    }
                }
                break;
            }
            if (vchData.size() == 33 || vchData.size() == 65)
                vPubKeys.push_back(vchData);
            else
                break;
        }
    } while (false);

    if (scriptPubKey.size() > 0 && scriptPubKey[0] == OP_RETURN)
        return "nulldata";

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_HASH160)
            break;
        if (!scriptPubKey.GetOp(pc, opcode, vchData) || vchData.size() != 20)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_EQUALVERIFY)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_DUP)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_HASH160)
            break;
        if (!scriptPubKey.GetOp(pc, opcode, vchData) || vchData.size() != 20)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_EQUALVERIFY)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_CHECKSIG)
            break;
        if (pc != scriptPubKey.end())
            break;

        nRequiredRet = 1;
        addressesRet.push_back(Hash160ToAddress(uint160(vchData)));
        return "hashlock";
    } while (false);

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_SHA256)
            break;
        if (!scriptPubKey.GetOp(pc, opcode, vchData) || vchData.size() != 32)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_EQUALVERIFY)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_DUP)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_HASH160)
            break;
        if (!scriptPubKey.GetOp(pc, opcode, vchData) || vchData.size() != 20)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_EQUALVERIFY)
            break;
        if (!scriptPubKey.GetOp(pc, opcode) || opcode != OP_CHECKSIG)
            break;
        if (pc != scriptPubKey.end())
            break;

        nRequiredRet = 1;
        addressesRet.push_back(Hash160ToAddress(uint160(vchData)));
        return "hashlock-sha256";
    } while (false);

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        bool hasArith = false;
        int depth = 0;
        while (scriptPubKey.GetOp(pc, opcode, vchData))
        {
            depth++;
            if (opcode == OP_ADD || opcode == OP_SUB || opcode == OP_MUL ||
                opcode == OP_DIV || opcode == OP_MOD ||
                opcode == OP_LSHIFT || opcode == OP_RSHIFT ||
                opcode == OP_2MUL || opcode == OP_2DIV ||
                opcode == OP_1ADD || opcode == OP_1SUB)
                hasArith = true;
        }
        if (!hasArith || depth < 2)
            break;

        return "arithmetic";
    } while (false);

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        bool hasBitwise = false;
        bool hasChecksig = false;
        while (scriptPubKey.GetOp(pc, opcode, vchData))
        {
            if (opcode == OP_AND || opcode == OP_OR || opcode == OP_XOR || opcode == OP_INVERT)
                hasBitwise = true;
            if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
                hasChecksig = true;
        }
        if (!hasBitwise)
            break;

        if (hasChecksig)
            return "bitwise-sig";
        return "bitwise";
    } while (false);

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        bool hasCat = false;
        bool hasHash = false;
        bool hasChecksig = false;
        while (scriptPubKey.GetOp(pc, opcode, vchData))
        {
            if (opcode == OP_CAT)
                hasCat = true;
            if (opcode == OP_HASH160 || opcode == OP_HASH256 || opcode == OP_SHA256)
                hasHash = true;
            if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
                hasChecksig = true;
        }
        if (!hasCat)
            break;

        if (hasHash && hasChecksig)
            return "cat-covenant";
        if (hasHash)
            return "cat-hash";
        return "cat-script";
    } while (false);

    do {
        CScript::const_iterator pc = scriptPubKey.begin();
        opcodetype opcode;
        vector<unsigned char> vchData;

        bool hasSplice = false;
        while (scriptPubKey.GetOp(pc, opcode, vchData))
        {
            if (opcode == OP_SUBSTR || opcode == OP_LEFT || opcode == OP_RIGHT)
                hasSplice = true;
        }
        if (!hasSplice)
            break;
        return "splice";
    } while (false);

    return "nonstandard";
}

static Object ScriptPubKeyToJSON(const CScript& scriptPubKey)
{
    Object o;
    o.push_back(Pair("asm", scriptPubKey.ToString()));
    o.push_back(Pair("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end(), false)));

    int nRequired;
    Array addresses;
    string strType = ClassifyScript(scriptPubKey, nRequired, addresses);
    o.push_back(Pair("type", strType));
    if (nRequired > 0)
        o.push_back(Pair("reqSigs", nRequired));
    if (addresses.size() > 0)
        o.push_back(Pair("addresses", addresses));

    return o;
}


Value decoderawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decoderawtransaction <hex string>\n"
            "Return a JSON object representing the serialized, hex-encoded transaction.");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction tx;
    try {
        ssData >> tx;
    } catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    Object result;
    result.push_back(Pair("txid", tx.GetHash().ToString()));
    result.push_back(Pair("version", tx.nVersion));
    result.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));

    Array vin;
    foreach(const CTxIn& txin, tx.vin)
    {
        Object in;
        if (txin.prevout.IsNull())
        {
            in.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end(), false)));
        }
        else
        {
            in.push_back(Pair("txid", txin.prevout.hash.ToString()));
            in.push_back(Pair("vout", (int)txin.prevout.n));
            Object scriptSigObj;
            scriptSigObj.push_back(Pair("asm", txin.scriptSig.ToString()));
            scriptSigObj.push_back(Pair("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end(), false)));
            in.push_back(Pair("scriptSig", scriptSigObj));
        }
        in.push_back(Pair("sequence", (boost::int64_t)txin.nSequence));
        vin.push_back(in);
    }
    result.push_back(Pair("vin", vin));

    Array vout;
    for (int i = 0; i < (int)tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        Object out;
        out.push_back(Pair("value", (double)txout.nValue / (double)COIN));
        out.push_back(Pair("n", i));
        out.push_back(Pair("scriptPubKey", ScriptPubKeyToJSON(txout.scriptPubKey)));
        vout.push_back(out);
    }
    result.push_back(Pair("vout", vout));

    return result;
}


Value decodescript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decodescript <hex string>\n"
            "Decode a hex-encoded script.");

    vector<unsigned char> scriptData = ParseHex(params[0].get_str());
    CScript script(scriptData.begin(), scriptData.end());

    Object result;
    result.push_back(Pair("asm", script.ToString()));
    result.push_back(Pair("hex", HexStr(script.begin(), script.end(), false)));

    int nRequired;
    Array addresses;
    string strType = ClassifyScript(script, nRequired, addresses);
    result.push_back(Pair("type", strType));
    if (nRequired > 0)
        result.push_back(Pair("reqSigs", nRequired));
    if (addresses.size() > 0)
        result.push_back(Pair("addresses", addresses));

    return result;
}


static bool IsValidPubKey(const vector<unsigned char>& vchPubKey)
{
    if (vchPubKey.size() == 33 && (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03))
        return true;
    if (vchPubKey.size() == 65 && vchPubKey[0] == 0x04)
        return true;
    return false;
}

Value createmultisig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "createmultisig <nrequired> [\"pubkey\",...]\n"
            "Creates a multi-signature script with nrequired keys.\n"
            "Returns JSON object with script hex and derived address.\n"
            "Each pubkey must be a hex-encoded compressed (33-byte) or uncompressed (65-byte) public key.\n"
            "Max 20 keys. nrequired must be between 1 and the number of keys.");

    int nRequired = params[0].get_int();
    const Array& keys = params[1].get_array();

    if (keys.size() < 1)
        throw runtime_error("A multisig address must require at least one key");
    if ((int)keys.size() > (int)MAX_PUBKEYS_PER_MULTISIG)
        throw runtime_error(strprintf("Too many keys: max %u", MAX_PUBKEYS_PER_MULTISIG));
    if (nRequired < 1)
        throw runtime_error("nrequired must be at least 1");
    if (nRequired > (int)keys.size())
        throw runtime_error("nrequired cannot exceed the number of keys");

    vector<vector<unsigned char> > vPubKeys;
    for (int i = 0; i < (int)keys.size(); i++)
    {
        vector<unsigned char> vchPubKey = ParseHex(keys[i].get_str());
        if (!IsValidPubKey(vchPubKey))
            throw runtime_error(strprintf("Invalid public key: %s", keys[i].get_str().c_str()));
        vPubKeys.push_back(vchPubKey);
    }

    CScript script;
    script << (opcodetype)(OP_1 + nRequired - 1);
    for (int i = 0; i < (int)vPubKeys.size(); i++)
        script << vPubKeys[i];
    script << (opcodetype)(OP_1 + (int)vPubKeys.size() - 1);
    script << OP_CHECKMULTISIG;

    if (script.size() > MAX_SCRIPT_SIZE)
        throw runtime_error("Script exceeds maximum size");

    Object result;
    result.push_back(Pair("asm", script.ToString()));
    result.push_back(Pair("hex", HexStr(script.begin(), script.end(), false)));
    result.push_back(Pair("type", strprintf("%d-of-%d", nRequired, (int)vPubKeys.size())));
    result.push_back(Pair("reqSigs", nRequired));

    Array addresses;
    for (int i = 0; i < (int)vPubKeys.size(); i++)
        addresses.push_back(Hash160ToAddress(Hash160(vPubKeys[i])));
    result.push_back(Pair("addresses", addresses));

    return result;
}


Value addmultisigaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "addmultisigaddress <nrequired> [\"key\",...] [label]\n"
            "Add a multisig address to the wallet so incoming payments are tracked.\n"
            "Each key can be a hex public key or a Bitok address (if the key is in the wallet).\n"
            "Returns the multisig script hex.");

    int nRequired = params[0].get_int();
    const Array& keys = params[1].get_array();

    if (keys.size() < 1)
        throw runtime_error("A multisig address must require at least one key");
    if ((int)keys.size() > (int)MAX_PUBKEYS_PER_MULTISIG)
        throw runtime_error(strprintf("Too many keys: max %u", MAX_PUBKEYS_PER_MULTISIG));
    if (nRequired < 1)
        throw runtime_error("nrequired must be at least 1");
    if (nRequired > (int)keys.size())
        throw runtime_error("nrequired cannot exceed the number of keys");

    string strLabel;
    if (params.size() > 2)
        strLabel = params[2].get_str();

    vector<vector<unsigned char> > vPubKeys;
    for (int i = 0; i < (int)keys.size(); i++)
    {
        string strKey = keys[i].get_str();
        vector<unsigned char> vchPubKey;

        if (strKey.size() == 66 || strKey.size() == 130)
        {
            vchPubKey = ParseHex(strKey);
            if (!IsValidPubKey(vchPubKey))
                throw runtime_error(strprintf("Invalid public key: %s", strKey.c_str()));
        }
        else
        {
            uint160 hash160;
            if (!AddressToHash160(strKey, hash160))
                throw runtime_error(strprintf("Invalid address or pubkey: %s", strKey.c_str()));

            CRITICAL_BLOCK(cs_mapKeys)
            {
                map<uint160, vector<unsigned char> >::iterator mi = mapPubKeys.find(hash160);
                if (mi == mapPubKeys.end())
                    throw runtime_error(strprintf("Address not in wallet, cannot resolve pubkey: %s", strKey.c_str()));
                vchPubKey = (*mi).second;
            }
        }
        vPubKeys.push_back(vchPubKey);
    }

    CScript script;
    script << (opcodetype)(OP_1 + nRequired - 1);
    for (int i = 0; i < (int)vPubKeys.size(); i++)
        script << vPubKeys[i];
    script << (opcodetype)(OP_1 + (int)vPubKeys.size() - 1);
    script << OP_CHECKMULTISIG;

    if (script.size() > MAX_SCRIPT_SIZE)
        throw runtime_error("Script exceeds maximum size");

    CWalletDB walletdb;
    walletdb.WriteName(HexStr(script.begin(), script.end(), false), strLabel);

    Object result;
    result.push_back(Pair("hex", HexStr(script.begin(), script.end(), false)));
    result.push_back(Pair("asm", script.ToString()));
    result.push_back(Pair("type", strprintf("%d-of-%d", nRequired, (int)vPubKeys.size())));
    result.push_back(Pair("reqSigs", nRequired));

    Array addresses;
    for (int i = 0; i < (int)vPubKeys.size(); i++)
        addresses.push_back(Hash160ToAddress(Hash160(vPubKeys[i])));
    result.push_back(Pair("addresses", addresses));

    return result;
}


Value createrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,...} [locktime]\n"
            "Create a transaction spending the given inputs and sending to the given addresses.\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.\n"
            "Output keys are Bitok addresses, \"data\" for OP_RETURN hex, or raw hex scriptPubKey.");

    const Array& inputs = params[0].get_array();
    const Object& sendTo = params[1].get_obj();

    CTransaction rawTx;

    foreach(const Value& input, inputs)
    {
        const Object& o = input.get_obj();

        Value txidVal = find_value(o, "txid");
        Value voutVal = find_value(o, "vout");

        if (txidVal.type() != str_type || voutVal.type() != int_type)
            throw runtime_error("Invalid input: each input must have txid (string) and vout (integer)");

        uint256 txid;
        txid.SetHex(txidVal.get_str());
        int nOutput = voutVal.get_int();

        if (nOutput < 0)
            throw runtime_error("Invalid parameter, vout must be non-negative");

        CTxIn in(COutPoint(txid, nOutput));

        Value seqVal = find_value(o, "sequence");
        if (seqVal.type() == int_type)
            in.nSequence = (unsigned int)seqVal.get_int64();

        rawTx.vin.push_back(in);
    }

    foreach(const Pair& s, sendTo)
    {
        if (s.name_ == "data")
        {
            vector<unsigned char> data = ParseHex(s.value_.get_str());
            CScript scriptPubKey;
            scriptPubKey << OP_RETURN;
            if (!data.empty())
                scriptPubKey << data;
            rawTx.vout.push_back(CTxOut(0, scriptPubKey));
        }
        else
        {
            double dAmount = s.value_.get_real();
            if (dAmount < 0.0 || dAmount > 21000000.0)
                throw runtime_error("Invalid amount");
            int64 nAmount = roundint64(dAmount * COIN);
            if (!MoneyRange(nAmount))
                throw runtime_error("Invalid amount");

            CScript scriptPubKey;
            if (!scriptPubKey.SetBitcoinAddress(s.name_))
            {
                vector<unsigned char> scriptBytes = ParseHex(s.name_);
                if (scriptBytes.empty())
                    throw runtime_error("Invalid address or script: " + s.name_);
                scriptPubKey = CScript(scriptBytes.begin(), scriptBytes.end());
            }

            rawTx.vout.push_back(CTxOut(nAmount, scriptPubKey));
        }
    }

    if (params.size() > 2)
        rawTx.nLockTime = (unsigned int)params[2].get_int64();

    CDataStream ss(SER_NETWORK);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end(), false);
}


static bool ParseMultisigScript(const CScript& script, int& nRequiredOut,
                                vector<vector<unsigned char> >& vPubKeysOut)
{
    nRequiredOut = 0;
    vPubKeysOut.clear();

    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    vector<unsigned char> vchData;

    if (!script.GetOp(pc, opcode) || opcode < OP_1 || opcode > OP_16)
        return false;
    nRequiredOut = (int)opcode - (int)(OP_1 - 1);

    while (script.GetOp(pc, opcode, vchData))
    {
        if (opcode >= OP_1 && opcode <= OP_16)
        {
            int nKeys = (int)opcode - (int)(OP_1 - 1);
            opcodetype opcodeCheck;
            if (script.GetOp(pc, opcodeCheck) &&
                opcodeCheck == OP_CHECKMULTISIG &&
                pc == script.end() &&
                (int)vPubKeysOut.size() == nKeys &&
                nRequiredOut >= 1 && nRequiredOut <= nKeys)
            {
                return true;
            }
            return false;
        }
        if (vchData.size() == 33 || vchData.size() == 65)
            vPubKeysOut.push_back(vchData);
        else
            return false;
    }
    return false;
}

static int ParseSighashString(const string& strHashType)
{
    if (strHashType == "ALL") return SIGHASH_ALL;
    if (strHashType == "NONE") return SIGHASH_NONE;
    if (strHashType == "SINGLE") return SIGHASH_SINGLE;
    if (strHashType == "ALL|ANYONECANPAY") return SIGHASH_ALL | SIGHASH_ANYONECANPAY;
    if (strHashType == "NONE|ANYONECANPAY") return SIGHASH_NONE | SIGHASH_ANYONECANPAY;
    if (strHashType == "SINGLE|ANYONECANPAY") return SIGHASH_SINGLE | SIGHASH_ANYONECANPAY;
    throw runtime_error("Invalid sighash type. Use: ALL, NONE, SINGLE, ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY");
}

Value signrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "signrawtransaction <hex> [prevtxs] [privkeys] [sighashtype]\n"
            "Sign inputs for raw transaction (serialized, hex-encoded).\n"
            "Second optional argument is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "Third optional argument is an array of base58-encoded private keys that,\n"
            "if given, will be the only keys used to sign the transaction.\n"
            "Fourth optional argument is the signature hash type. Must be one of:\n"
            "  ALL, NONE, SINGLE, ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY\n"
            "  Default is ALL.\n"
            "Returns json object with keys:\n"
            "  hex : raw transaction with signature(s) (hex-encoded string)\n"
            "  complete : true if transaction has a complete set of signatures\n");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction txToSign;
    try {
        ssData >> txToSign;
    } catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    map<COutPoint, CScript> mapPrevOut;
    if (params.size() > 1 && params[1].type() != null_type)
    {
        const Array& prevTxs = params[1].get_array();
        foreach(const Value& p, prevTxs)
        {
            const Object& prevOut = p.get_obj();

            Value txidVal = find_value(prevOut, "txid");
            Value voutVal = find_value(prevOut, "vout");
            Value scriptVal = find_value(prevOut, "scriptPubKey");

            if (txidVal.type() != str_type || voutVal.type() != int_type || scriptVal.type() != str_type)
                throw runtime_error("Invalid prevtx: must have txid, vout, and scriptPubKey");

            uint256 txid;
            txid.SetHex(txidVal.get_str());

            vector<unsigned char> scriptData = ParseHex(scriptVal.get_str());
            CScript scriptPubKey(scriptData.begin(), scriptData.end());

            mapPrevOut[COutPoint(txid, voutVal.get_int())] = scriptPubKey;
        }
    }

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && params[3].type() != null_type)
        nHashType = ParseSighashString(params[3].get_str());

    bool fGivenKeys = false;
    map<uint160, CKey> mapTempKeys;
    map<vector<unsigned char>, CKey> mapTempKeysByPub;
    if (params.size() > 2 && params[2].type() != null_type)
    {
        fGivenKeys = true;
        const Array& keys = params[2].get_array();
        foreach(const Value& k, keys)
        {
            string strSecret = k.get_str();
            vector<unsigned char> vchWIF;
            if (!DecodeBase58Check(strSecret, vchWIF))
                throw runtime_error("Invalid private key encoding");
            if (vchWIF.size() < 33 || vchWIF[0] != 128)
                throw runtime_error("Invalid private key version");

            vector<unsigned char> vchSecret(vchWIF.begin() + 1, vchWIF.begin() + 33);
            CKey key;
            if (!key.SetSecret(vchSecret))
                throw runtime_error("Invalid private key");

            vector<unsigned char> vchPubKey = key.GetPubKey();
            mapTempKeys[Hash160(vchPubKey)] = key;
            mapTempKeysByPub[vchPubKey] = key;
        }
    }

    bool fComplete = true;
    for (unsigned int i = 0; i < txToSign.vin.size(); i++)
    {
        CTxIn& txin = txToSign.vin[i];

        if (txin.prevout.IsNull())
            continue;

        CScript scriptPubKey;
        bool fFoundPrev = false;

        if (mapPrevOut.count(txin.prevout))
        {
            scriptPubKey = mapPrevOut[txin.prevout];
            fFoundPrev = true;
        }

        if (!fFoundPrev)
        {
            CRITICAL_BLOCK(cs_mapWallet)
            {
                if (mapWallet.count(txin.prevout.hash))
                {
                    const CWalletTx& wtx = mapWallet[txin.prevout.hash];
                    if (txin.prevout.n < wtx.vout.size())
                    {
                        scriptPubKey = wtx.vout[txin.prevout.n].scriptPubKey;
                        fFoundPrev = true;
                    }
                }
            }
        }

        if (!fFoundPrev)
        {
            CTxDB txdb("r");
            CTransaction txPrev;
            CTxIndex txindex;
            if (txdb.ReadDiskTx(txin.prevout.hash, txPrev, txindex))
            {
                if (txin.prevout.n < txPrev.vout.size())
                {
                    scriptPubKey = txPrev.vout[txin.prevout.n].scriptPubKey;
                    fFoundPrev = true;
                }
            }
        }

        if (!fFoundPrev)
        {
            CRITICAL_BLOCK(cs_mapTransactions)
            {
                if (mapTransactions.count(txin.prevout.hash))
                {
                    const CTransaction& txPrev = mapTransactions[txin.prevout.hash];
                    if (txin.prevout.n < txPrev.vout.size())
                    {
                        scriptPubKey = txPrev.vout[txin.prevout.n].scriptPubKey;
                        fFoundPrev = true;
                    }
                }
            }
        }

        if (!fFoundPrev)
        {
            fComplete = false;
            continue;
        }

        if (txin.prevout.n >= 100000)
        {
            fComplete = false;
            continue;
        }

        CScript scriptSigSaved = txin.scriptSig;
        bool fSigned = false;

        if (fGivenKeys)
        {
            int nMRequired;
            vector<vector<unsigned char> > vMPubKeys;
            if (ParseMultisigScript(scriptPubKey, nMRequired, vMPubKeys))
            {
                uint256 hash = SignatureHash(scriptPubKey, txToSign, i, nHashType);

                vector<vector<unsigned char> > vExistingSigs;
                {
                    CScript::const_iterator pc = scriptSigSaved.begin();
                    opcodetype op;
                    vector<unsigned char> vchData;
                    bool fFirst = true;
                    while (scriptSigSaved.GetOp(pc, op, vchData))
                    {
                        if (fFirst) { fFirst = false; continue; }
                        if (vchData.size() > 0)
                            vExistingSigs.push_back(vchData);
                    }
                }

                CScript scriptSig;
                scriptSig << OP_0;
                int nSigned = 0;

                for (int k = 0; k < (int)vMPubKeys.size(); k++)
                {
                    const vector<unsigned char>& vchPubKey = vMPubKeys[k];
                    bool fHaveSig = false;

                    for (int s = 0; s < (int)vExistingSigs.size(); s++)
                    {
                        vector<unsigned char> vchSigCheck = vExistingSigs[s];
                        if (vchSigCheck.size() > 1)
                        {
                            int nSigHT = vchSigCheck.back();
                            vector<unsigned char> vchSigOnly(vchSigCheck.begin(), vchSigCheck.end() - 1);
                            uint256 hashCheck = SignatureHash(scriptPubKey, txToSign, i, nSigHT);
                            if (CKey::Verify(vchPubKey, hashCheck, vchSigOnly))
                            {
                                scriptSig << vchSigCheck;
                                nSigned++;
                                fHaveSig = true;
                                vExistingSigs.erase(vExistingSigs.begin() + s);
                                break;
                            }
                        }
                    }

                    if (!fHaveSig && mapTempKeysByPub.count(vchPubKey))
                    {
                        CKey& key = mapTempKeysByPub[vchPubKey];
                        vector<unsigned char> vchSig;
                        if (key.Sign(hash, vchSig))
                        {
                            vchSig.push_back((unsigned char)nHashType);
                            scriptSig << vchSig;
                            nSigned++;
                        }
                    }
                }

                txin.scriptSig = scriptSig;

                if (nSigned >= nMRequired)
                {
                    unsigned int nVerifyFlags = SCRIPT_VERIFY_NONE;
                    if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                        nVerifyFlags |= SCRIPT_VERIFY_EXEC;
                    if (VerifyScript(txin.scriptSig, scriptPubKey, txToSign, i, 0, nVerifyFlags))
                        fSigned = true;
                    else
                        txin.scriptSig = scriptSigSaved;
                }
            }

            if (!fSigned)
            {
                uint256 hash = SignatureHash(scriptPubKey, txToSign, i, nHashType);

                uint160 h160;
                if (ExtractHash160(scriptPubKey, h160) && mapTempKeys.count(h160))
                {
                    CKey& key = mapTempKeys[h160];
                    vector<unsigned char> vchSig;
                    if (key.Sign(hash, vchSig))
                    {
                        vchSig.push_back((unsigned char)nHashType);
                        CScript scriptSig;
                        scriptSig << vchSig << key.GetPubKey();
                        txin.scriptSig = scriptSig;
                        fSigned = true;
                    }
                }
            }

            if (!fSigned)
            {
                uint256 hash = SignatureHash(scriptPubKey, txToSign, i, nHashType);

                vector<unsigned char> vchScriptPubKey;
                if (scriptPubKey.size() == 35 && scriptPubKey[0] == 33 && scriptPubKey[34] == OP_CHECKSIG)
                    vchScriptPubKey.assign(scriptPubKey.begin() + 1, scriptPubKey.begin() + 34);
                else if (scriptPubKey.size() == 67 && scriptPubKey[0] == 65 && scriptPubKey[66] == OP_CHECKSIG)
                    vchScriptPubKey.assign(scriptPubKey.begin() + 1, scriptPubKey.begin() + 66);

                if (!vchScriptPubKey.empty())
                {
                    uint160 pk160 = Hash160(vchScriptPubKey);
                    if (mapTempKeys.count(pk160))
                    {
                        CKey& key = mapTempKeys[pk160];
                        vector<unsigned char> vchSig;
                        if (key.Sign(hash, vchSig))
                        {
                            vchSig.push_back((unsigned char)nHashType);
                            CScript scriptSig;
                            scriptSig << vchSig;
                            txin.scriptSig = scriptSig;
                            fSigned = true;
                        }
                    }
                }
            }

            if (fSigned)
            {
                int nMR2;
                vector<vector<unsigned char> > vMP2;
                if (!ParseMultisigScript(scriptPubKey, nMR2, vMP2))
                {
                    unsigned int nVerifyFlags = SCRIPT_VERIFY_NONE;
                    if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
                        nVerifyFlags |= SCRIPT_VERIFY_EXEC;
                    if (!VerifyScript(txin.scriptSig, scriptPubKey, txToSign, i, 0, nVerifyFlags))
                    {
                        txin.scriptSig = scriptSigSaved;
                        fSigned = false;
                    }
                }
            }
        }
        else
        {
            CTransaction txFake;
            txFake.vout.resize(txin.prevout.n + 1);
            txFake.vout[txin.prevout.n].scriptPubKey = scriptPubKey;

            fSigned = SignSignature(txFake, txToSign, i, nHashType);
        }

        if (!fSigned)
        {
            fComplete = false;
        }
    }

    CDataStream ss(SER_NETWORK);
    ss << txToSign;

    Object result;
    result.push_back(Pair("hex", HexStr(ss.begin(), ss.end(), false)));
    result.push_back(Pair("complete", fComplete));
    return result;
}


Value analyzescript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "analyzescript <hex>\n"
            "Static analysis of a hex-encoded script.\n"
            "Reports opcodes, sigops, size, type, classification, and limit usage.");

    vector<unsigned char> scriptData = ParseHex(params[0].get_str());
    if (scriptData.empty())
        throw runtime_error("Empty script");
    CScript script(scriptData.begin(), scriptData.end());

    Object result;
    result.push_back(Pair("asm", script.ToString()));
    result.push_back(Pair("hex", HexStr(script.begin(), script.end(), false)));
    result.push_back(Pair("size", (int)script.size()));

    int nRequired;
    Array addresses;
    string strType = ClassifyScript(script, nRequired, addresses);
    result.push_back(Pair("type", strType));
    if (nRequired > 0)
        result.push_back(Pair("reqSigs", nRequired));
    if (addresses.size() > 0)
        result.push_back(Pair("addresses", addresses));

    if (strType == "hashlock" || strType == "hashlock-sha256")
    {
        Object tmpl;
        tmpl.push_back(Pair("name", strType));
        tmpl.push_back(Pair("description", strType == "hashlock"
            ? "HASH160 preimage lock + P2PKH signature"
            : "SHA256 preimage lock + P2PKH signature"));
        tmpl.push_back(Pair("scriptSig", "<sig> <pubkey> <preimage>"));
        tmpl.push_back(Pair("spendable", "Requires preimage + private key"));
        result.push_back(Pair("template", tmpl));
    }
    else if (strType == "arithmetic")
    {
        Object tmpl;
        tmpl.push_back(Pair("name", "arithmetic"));
        tmpl.push_back(Pair("description", "Arithmetic condition gate"));
        tmpl.push_back(Pair("scriptSig", "<value(s) satisfying arithmetic condition>"));
        tmpl.push_back(Pair("spendable", "Requires numeric solution"));
        result.push_back(Pair("template", tmpl));
    }
    else if (strType == "bitwise" || strType == "bitwise-sig")
    {
        Object tmpl;
        tmpl.push_back(Pair("name", strType));
        tmpl.push_back(Pair("description", strType == "bitwise-sig"
            ? "Bitwise condition + signature"
            : "Bitwise condition gate"));
        tmpl.push_back(Pair("scriptSig", strType == "bitwise-sig"
            ? "<sig> <pubkey> <data matching bitmask>"
            : "<data matching bitmask>"));
        tmpl.push_back(Pair("spendable", strType == "bitwise-sig"
            ? "Requires matching data + private key"
            : "Requires matching data"));
        result.push_back(Pair("template", tmpl));
    }
    else if (strType == "cat-covenant" || strType == "cat-hash" || strType == "cat-script")
    {
        Object tmpl;
        tmpl.push_back(Pair("name", strType));
        string desc = "OP_CAT ";
        if (strType == "cat-covenant")
            desc += "concatenation covenant with hash verification + signature";
        else if (strType == "cat-hash")
            desc += "concatenation with hash verification";
        else
            desc += "concatenation script";
        tmpl.push_back(Pair("description", desc));
        tmpl.push_back(Pair("spendable", "Requires data satisfying concatenation + hash condition"));
        result.push_back(Pair("template", tmpl));
    }
    else if (strType == "splice")
    {
        Object tmpl;
        tmpl.push_back(Pair("name", "splice"));
        tmpl.push_back(Pair("description", "String manipulation script (SUBSTR/LEFT/RIGHT)"));
        tmpl.push_back(Pair("spendable", "Requires data satisfying splice condition"));
        result.push_back(Pair("template", tmpl));
    }

    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    vector<unsigned char> vchData;
    int nOpcodeCount = 0;
    int nPushCount = 0;
    int nMaxPushSize = 0;

    map<string, bool> seenDistinct;
    Array distinctList;
    map<string, bool> seenCrypto, seenStack, seenFlow, seenArith, seenSplice, seenBitwise;

    while (script.GetOp(pc, opcode, vchData))
    {
        if (opcode <= OP_PUSHDATA4)
        {
            nPushCount++;
            if ((int)vchData.size() > nMaxPushSize)
                nMaxPushSize = (int)vchData.size();
            continue;
        }

        if (opcode >= OP_1NEGATE && opcode <= OP_16)
        {
            nPushCount++;
            continue;
        }

        if (opcode > OP_16)
            nOpcodeCount++;

        string opName = GetOpName(opcode);

        if (!seenDistinct.count(opName))
        {
            seenDistinct[opName] = true;
            distinctList.push_back(opName);
        }

        switch (opcode)
        {
        case OP_RIPEMD160: case OP_SHA1: case OP_SHA256:
        case OP_HASH160: case OP_HASH256:
        case OP_CHECKSIG: case OP_CHECKSIGVERIFY:
        case OP_CHECKMULTISIG: case OP_CHECKMULTISIGVERIFY:
        case OP_CODESEPARATOR:
            seenCrypto[opName] = true;
            break;

        case OP_TOALTSTACK: case OP_FROMALTSTACK:
        case OP_2DROP: case OP_2DUP: case OP_3DUP: case OP_2OVER:
        case OP_2ROT: case OP_2SWAP: case OP_IFDUP: case OP_DEPTH:
        case OP_DROP: case OP_DUP: case OP_NIP: case OP_OVER:
        case OP_PICK: case OP_ROLL: case OP_ROT: case OP_SWAP: case OP_TUCK:
            seenStack[opName] = true;
            break;

        case OP_NOP: case OP_IF: case OP_NOTIF: case OP_ELSE: case OP_ENDIF:
        case OP_VERIFY: case OP_RETURN:
            seenFlow[opName] = true;
            break;

        case OP_1ADD: case OP_1SUB: case OP_2MUL: case OP_2DIV:
        case OP_NEGATE: case OP_ABS: case OP_NOT: case OP_0NOTEQUAL:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_LSHIFT: case OP_RSHIFT:
        case OP_BOOLAND: case OP_BOOLOR:
        case OP_NUMEQUAL: case OP_NUMEQUALVERIFY:
        case OP_NUMNOTEQUAL: case OP_LESSTHAN: case OP_GREATERTHAN:
        case OP_LESSTHANOREQUAL: case OP_GREATERTHANOREQUAL:
        case OP_MIN: case OP_MAX: case OP_WITHIN:
            seenArith[opName] = true;
            break;

        case OP_CAT: case OP_SUBSTR: case OP_LEFT: case OP_RIGHT: case OP_SIZE:
            seenSplice[opName] = true;
            break;

        case OP_INVERT: case OP_AND: case OP_OR: case OP_XOR:
        case OP_EQUAL: case OP_EQUALVERIFY:
            seenBitwise[opName] = true;
            break;

        default:
            break;
        }
    }

    unsigned int nSigOps = GetSigOpCount(script);

    Object opcodes;
    opcodes.push_back(Pair("counted", nOpcodeCount));
    opcodes.push_back(Pair("pushes", nPushCount));
    opcodes.push_back(Pair("distinct", distinctList));

    Array cryptoArr, stackArr, flowArr, arithArr, spliceArr, bitwiseArr;
    for (map<string,bool>::iterator it = seenCrypto.begin(); it != seenCrypto.end(); ++it)
        cryptoArr.push_back(it->first);
    for (map<string,bool>::iterator it = seenStack.begin(); it != seenStack.end(); ++it)
        stackArr.push_back(it->first);
    for (map<string,bool>::iterator it = seenFlow.begin(); it != seenFlow.end(); ++it)
        flowArr.push_back(it->first);
    for (map<string,bool>::iterator it = seenArith.begin(); it != seenArith.end(); ++it)
        arithArr.push_back(it->first);
    for (map<string,bool>::iterator it = seenSplice.begin(); it != seenSplice.end(); ++it)
        spliceArr.push_back(it->first);
    for (map<string,bool>::iterator it = seenBitwise.begin(); it != seenBitwise.end(); ++it)
        bitwiseArr.push_back(it->first);

    Object categories;
    categories.push_back(Pair("crypto", cryptoArr));
    categories.push_back(Pair("stack", stackArr));
    categories.push_back(Pair("flow", flowArr));
    categories.push_back(Pair("arithmetic", arithArr));
    categories.push_back(Pair("splice", spliceArr));
    categories.push_back(Pair("bitwise", bitwiseArr));
    opcodes.push_back(Pair("categories", categories));

    result.push_back(Pair("opcodes", opcodes));
    result.push_back(Pair("sigops", (int)nSigOps));
    if (nPushCount > 0)
        result.push_back(Pair("maxPushSize", nMaxPushSize));

    Object limits;
    limits.push_back(Pair("size", strprintf("%d/%d", (int)script.size(), (int)MAX_SCRIPT_SIZE)));
    limits.push_back(Pair("opcodes", strprintf("%d/%d", nOpcodeCount, (int)MAX_OPS_PER_SCRIPT)));
    limits.push_back(Pair("sigops", strprintf("%d/%d", (int)nSigOps, (int)MAX_SIGOPS_PER_BLOCK)));
    if (nMaxPushSize > 0)
        limits.push_back(Pair("maxPushSize", strprintf("%d/%d", nMaxPushSize, (int)MAX_SCRIPT_ELEMENT_SIZE)));
    bool fWithinLimits = ((int)script.size() <= (int)MAX_SCRIPT_SIZE &&
                          nOpcodeCount <= (int)MAX_OPS_PER_SCRIPT &&
                          nMaxPushSize <= (int)MAX_SCRIPT_ELEMENT_SIZE);
    limits.push_back(Pair("withinLimits", fWithinLimits));
    result.push_back(Pair("limits", limits));

    return result;
}


Value validatescript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "validatescript <script hex> [stack] [flags]\n"
            "Execute a script in sandbox mode and report the result.\n"
            "\nArguments:\n"
            "1. script  (string) Hex-encoded script to execute\n"
            "2. stack   (array)  Hex-encoded values to pre-load on stack\n"
            "                    (simulates what scriptSig would push)\n"
            "3. flags   (string) \"exec\" (default) or \"legacy\"\n"
            "\nOP_CHECKSIG/OP_CHECKMULTISIG will fail (no transaction context).\n"
            "Use this to test script logic with known stack inputs.\n"
            "\nExamples:\n"
            "  validatescript \"87\" [\"01\",\"01\"]          OP_EQUAL: 1 == 1 -> true\n"
            "  validatescript \"935587\" [\"03\",\"02\"]       OP_ADD OP_5 OP_EQUAL: 3+2==5\n"
            "  validatescript \"76a988ac\" [\"<pubkey>\"]    P2PKH template (sig check fails)");

    vector<unsigned char> scriptData = ParseHex(params[0].get_str());
    CScript script(scriptData.begin(), scriptData.end());

    vector<vector<unsigned char> > stack;
    if (params.size() > 1 && params[1].type() == array_type)
    {
        const Array& items = params[1].get_array();
        foreach(const Value& item, items)
            stack.push_back(ParseHex(item.get_str()));
    }

    unsigned int flags = SCRIPT_VERIFY_EXEC;
    if (params.size() > 2)
    {
        string strFlags = params[2].get_str();
        if (strFlags == "legacy")
            flags = SCRIPT_VERIFY_NONE;
        else if (strFlags != "exec")
            throw runtime_error("Invalid flags. Use \"exec\" or \"legacy\"");
    }

    CTransaction txDummy;
    txDummy.vin.resize(1);
    txDummy.vout.resize(1);

    bool fSuccess = EvalScript(stack, script, txDummy, 0, 0, flags);

    bool fValid = false;
    if (fSuccess && !stack.empty())
    {
        const vector<unsigned char>& vchTop = stack.back();
        for (unsigned int i = 0; i < vchTop.size(); i++)
        {
            if (vchTop[i] != 0)
            {
                if (i == vchTop.size()-1 && vchTop[i] == 0x80)
                    break;
                fValid = true;
                break;
            }
        }
    }

    Object result;
    result.push_back(Pair("valid", fValid));
    result.push_back(Pair("success", fSuccess));

    Array finalStack;
    for (int i = 0; i < (int)stack.size(); i++)
    {
        if (stack[i].empty())
            finalStack.push_back("");
        else
            finalStack.push_back(HexStr(stack[i].begin(), stack[i].end(), false));
    }
    result.push_back(Pair("finalStack", finalStack));
    result.push_back(Pair("stackSize", (int)stack.size()));

    if (!fSuccess)
    {
        Array warnings;
        if (script.size() > MAX_SCRIPT_SIZE)
            warnings.push_back("Script exceeds maximum size limit");

        unsigned int nSigOps = GetSigOpCount(script);
        if (nSigOps > 0)
            warnings.push_back("Script contains signature-checking opcodes which fail without transaction context");

        CScript::const_iterator pc2 = script.begin();
        opcodetype op2;
        int nOps = 0;
        while (script.GetOp(pc2, op2))
            if (op2 > OP_16)
                nOps++;
        if (nOps > (int)MAX_OPS_PER_SCRIPT)
            warnings.push_back("Script exceeds maximum opcode count");

        if (warnings.size() > 0)
            result.push_back(Pair("warnings", warnings));
    }

    return result;
}


Value addpreimage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "addpreimage <hex>\n"
            "Register a hash preimage with the wallet for spending hashlock scripts.\n"
            "Computes HASH160 and SHA256 of the preimage and stores both mappings.\n"
            "The wallet will then recognize hashlock outputs locked to this preimage as spendable.");

    vector<unsigned char> vchPreimage = ParseHex(params[0].get_str());
    if (vchPreimage.empty())
        throw runtime_error("Empty preimage");
    if (vchPreimage.size() > MAX_SCRIPT_ELEMENT_SIZE)
        throw runtime_error("Preimage exceeds maximum element size (520 bytes)");

    uint160 hash160 = Hash160(vchPreimage);
    vector<unsigned char> vchHash160((unsigned char*)&hash160, (unsigned char*)&hash160 + sizeof(hash160));

    vector<unsigned char> vchSha256(32);
    SHA256(&vchPreimage[0], vchPreimage.size(), &vchSha256[0]);

    CRITICAL_BLOCK(cs_mapKeys)
    {
        mapHashPreimages[vchHash160] = vchPreimage;
        mapHashPreimages[vchSha256] = vchPreimage;
    }

    CWalletDB walletdb;
    if (!walletdb.WriteHashPreimage(vchHash160, vchPreimage))
        throw runtime_error("Failed to persist preimage to wallet.dat");
    if (!walletdb.WriteHashPreimage(vchSha256, vchPreimage))
        throw runtime_error("Failed to persist preimage to wallet.dat");

    Object result;
    result.push_back(Pair("preimage", HexStr(vchPreimage.begin(), vchPreimage.end(), false)));
    result.push_back(Pair("hash160", HexStr(vchHash160.begin(), vchHash160.end(), false)));
    result.push_back(Pair("sha256", HexStr(vchSha256.begin(), vchSha256.end(), false)));
    result.push_back(Pair("size", (int)vchPreimage.size()));
    return result;
}


Value listpreimages(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "listpreimages\n"
            "List all registered hash preimages in the wallet.");

    Array result;
    map<string, bool> seen;

    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, vector<unsigned char> >::iterator it = mapHashPreimages.begin();
             it != mapHashPreimages.end(); ++it)
        {
            string preimageHex = HexStr(it->second.begin(), it->second.end(), false);
            if (seen.count(preimageHex))
                continue;
            seen[preimageHex] = true;

            Object entry;
            entry.push_back(Pair("preimage", preimageHex));
            entry.push_back(Pair("hash", HexStr(it->first.begin(), it->first.end(), false)));
            entry.push_back(Pair("hashSize", (int)it->first.size()));
            entry.push_back(Pair("preimageSize", (int)it->second.size()));
            result.push_back(entry);
        }
    }

    return result;
}


static bool IsHex(const string& str)
{
    if (str.empty() || str.size() % 2 != 0)
        return false;
    for (unsigned int i = 0; i < str.size(); i++)
    {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}


static bool GetOpcodeByName(const string& name, opcodetype& opcodeRet)
{
    static map<string, opcodetype> mapOpNames;
    if (mapOpNames.empty())
    {
        for (int op = 0; op <= OP_CHECKMULTISIGVERIFY; op++)
        {
            const char* pszName = GetOpName((opcodetype)op);
            if (pszName && string(pszName) != "UNKNOWN_OPCODE")
            {
                mapOpNames[string(pszName)] = (opcodetype)op;
                mapOpNames[string("OP_") + pszName] = (opcodetype)op;
            }
        }
        mapOpNames["OP_0"] = OP_0;
        mapOpNames["OP_FALSE"] = OP_0;
        mapOpNames["OP_1"] = OP_1;
        mapOpNames["OP_TRUE"] = OP_1;
        mapOpNames["OP_2"] = OP_2;
        mapOpNames["OP_3"] = OP_3;
        mapOpNames["OP_4"] = OP_4;
        mapOpNames["OP_5"] = OP_5;
        mapOpNames["OP_6"] = OP_6;
        mapOpNames["OP_7"] = OP_7;
        mapOpNames["OP_8"] = OP_8;
        mapOpNames["OP_9"] = OP_9;
        mapOpNames["OP_10"] = OP_10;
        mapOpNames["OP_11"] = OP_11;
        mapOpNames["OP_12"] = OP_12;
        mapOpNames["OP_13"] = OP_13;
        mapOpNames["OP_14"] = OP_14;
        mapOpNames["OP_15"] = OP_15;
        mapOpNames["OP_16"] = OP_16;
        mapOpNames["OP_1NEGATE"] = OP_1NEGATE;
        mapOpNames["0"] = OP_0;
        mapOpNames["-1"] = OP_1NEGATE;
        mapOpNames["1"] = OP_1;
        mapOpNames["2"] = OP_2;
        mapOpNames["3"] = OP_3;
        mapOpNames["4"] = OP_4;
        mapOpNames["5"] = OP_5;
        mapOpNames["6"] = OP_6;
        mapOpNames["7"] = OP_7;
        mapOpNames["8"] = OP_8;
        mapOpNames["9"] = OP_9;
        mapOpNames["10"] = OP_10;
        mapOpNames["11"] = OP_11;
        mapOpNames["12"] = OP_12;
        mapOpNames["13"] = OP_13;
        mapOpNames["14"] = OP_14;
        mapOpNames["15"] = OP_15;
        mapOpNames["16"] = OP_16;
    }

    map<string, opcodetype>::iterator it = mapOpNames.find(name);
    if (it != mapOpNames.end())
    {
        opcodeRet = it->second;
        return true;
    }
    return false;
}


Value buildscript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "buildscript [\"element\",...]\n"
            "Assemble a script from opcode names and hex data pushes.\n"
            "\nEach element is either:\n"
            "  - An opcode name: \"OP_DUP\", \"OP_HASH160\", \"OP_CHECKSIG\", etc.\n"
            "  - A hex data string to push: \"deadbeef\", \"03aabbcc...\"\n"
            "  - Numeric shorthand: \"0\" through \"16\", \"-1\"\n"
            "\nReturns hex-encoded script and analysis.\n"
            "\nExamples:\n"
            "  buildscript [\"OP_DUP\",\"OP_HASH160\",\"89abcdef...\",\"OP_EQUALVERIFY\",\"OP_CHECKSIG\"]\n"
            "  buildscript [\"OP_ADD\",\"OP_5\",\"OP_EQUAL\"]\n"
            "  buildscript [\"OP_HASH160\",\"<20-byte-hash>\",\"OP_EQUALVERIFY\",\"OP_DUP\",\"OP_HASH160\",\"<20-byte-hash>\",\"OP_EQUALVERIFY\",\"OP_CHECKSIG\"]");

    const Array& elements = params[0].get_array();
    if (elements.empty())
        throw runtime_error("Empty script elements");

    CScript script;

    for (unsigned int i = 0; i < elements.size(); i++)
    {
        string elem = elements[i].get_str();

        opcodetype opcode;
        if (GetOpcodeByName(elem, opcode))
        {
            script << opcode;
        }
        else
        {
            if (!IsHex(elem))
                throw runtime_error(strprintf("Element %d is not a recognized opcode or valid hex: %s", i, elem.c_str()));

            vector<unsigned char> data = ParseHex(elem);
            if (data.size() > MAX_SCRIPT_ELEMENT_SIZE)
                throw runtime_error(strprintf("Element %d exceeds max push size (%d > %d)", i, (int)data.size(), (int)MAX_SCRIPT_ELEMENT_SIZE));
            script << data;
        }
    }

    Object result;
    result.push_back(Pair("hex", HexStr(script.begin(), script.end(), false)));
    result.push_back(Pair("asm", script.ToString()));
    result.push_back(Pair("size", (int)script.size()));

    int nRequired;
    Array addresses;
    string strType = ClassifyScript(script, nRequired, addresses);
    result.push_back(Pair("type", strType));
    if (nRequired > 0)
        result.push_back(Pair("reqSigs", nRequired));
    if (addresses.size() > 0)
        result.push_back(Pair("addresses", addresses));

    bool fWithinLimits = ((int)script.size() <= (int)MAX_SCRIPT_SIZE);
    result.push_back(Pair("withinLimits", fWithinLimits));

    return result;
}


Value setscriptsig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 3)
        throw runtime_error(
            "setscriptsig <tx hex> <input index> <scriptsig hex or elements array>\n"
            "Set a custom scriptSig on a raw transaction input.\n"
            "The scriptSig can be raw hex or an array of hex push data.\n"
            "\nUse this to spend custom (non-standard) scripts by providing\n"
            "the exact scriptSig needed to satisfy the scriptPubKey.\n"
            "\nArguments:\n"
            "1. tx         (string) Hex-encoded raw transaction\n"
            "2. input      (int)    Input index to modify (0-based)\n"
            "3. scriptsig  (string or array) Hex scriptSig, or array of hex pushes\n"
            "\nExamples:\n"
            "  setscriptsig <txhex> 0 \"03020187\"          Raw scriptSig hex\n"
            "  setscriptsig <txhex> 0 [\"03\",\"02\"]        Push 0x03 then 0x02");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction tx;
    try {
        ssData >> tx;
    } catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    int nInput = params[1].get_int();
    if (nInput < 0 || nInput >= (int)tx.vin.size())
        throw runtime_error(strprintf("Input index %d out of range (tx has %d inputs)", nInput, (int)tx.vin.size()));

    CScript scriptSig;
    if (params[2].type() == array_type)
    {
        const Array& pushes = params[2].get_array();
        for (unsigned int i = 0; i < pushes.size(); i++)
        {
            string elem = pushes[i].get_str();
            opcodetype opcode;
            if (GetOpcodeByName(elem, opcode))
            {
                scriptSig << opcode;
            }
            else
            {
                if (!IsHex(elem))
                    throw runtime_error(strprintf("Push element %d is not valid hex: %s", i, elem.c_str()));
                vector<unsigned char> data = ParseHex(elem);
                scriptSig << data;
            }
        }
    }
    else
    {
        string strSig = params[2].get_str();

        bool fAllOpcodesOrHex = false;
        if (strSig.find(' ') != string::npos)
        {
            fAllOpcodesOrHex = true;
            vector<string> parts;
            string current;
            for (unsigned int i = 0; i < strSig.size(); i++)
            {
                if (strSig[i] == ' ')
                {
                    if (!current.empty()) parts.push_back(current);
                    current.clear();
                }
                else
                    current += strSig[i];
            }
            if (!current.empty()) parts.push_back(current);

            for (unsigned int i = 0; i < parts.size(); i++)
            {
                opcodetype opcode;
                if (GetOpcodeByName(parts[i], opcode))
                    scriptSig << opcode;
                else if (IsHex(parts[i]))
                    scriptSig << ParseHex(parts[i]);
                else
                {
                    fAllOpcodesOrHex = false;
                    break;
                }
            }
        }

        if (!fAllOpcodesOrHex)
        {
            if (!IsHex(strSig))
                throw runtime_error("scriptSig must be hex-encoded, a space-separated opcode/data string, or an array of hex pushes");
            vector<unsigned char> sigData = ParseHex(strSig);
            scriptSig = CScript(sigData.begin(), sigData.end());
        }
    }

    tx.vin[nInput].scriptSig = scriptSig;

    CDataStream ss(SER_NETWORK);
    ss << tx;

    Object result;
    result.push_back(Pair("hex", HexStr(ss.begin(), ss.end(), false)));
    result.push_back(Pair("scriptSig", HexStr(scriptSig.begin(), scriptSig.end(), false)));
    result.push_back(Pair("scriptSigAsm", scriptSig.ToString()));
    result.push_back(Pair("inputIndex", nInput));
    return result;
}


Value getscriptsighash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error(
            "getscriptsighash <tx hex> <input index> <scriptpubkey hex> [sighashtype]\n"
            "Compute the signature hash for an input of a raw transaction.\n"
            "\nThis returns the 32-byte digest that needs to be signed with\n"
            "the private key for OP_CHECKSIG/OP_CHECKMULTISIG scripts.\n"
            "\nArguments:\n"
            "1. tx             (string) Hex-encoded raw transaction\n"
            "2. input          (int)    Input index (0-based)\n"
            "3. scriptpubkey   (string) Hex-encoded scriptPubKey being spent\n"
            "4. sighashtype    (string) Optional: ALL, NONE, SINGLE,\n"
            "                           ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY\n"
            "                           Default: ALL\n"
            "\nReturns the sighash digest, use with external signing tools.");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction tx;
    try {
        ssData >> tx;
    } catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    int nInput = params[1].get_int();
    if (nInput < 0 || nInput >= (int)tx.vin.size())
        throw runtime_error(strprintf("Input index %d out of range (tx has %d inputs)", nInput, (int)tx.vin.size()));

    vector<unsigned char> scriptData = ParseHex(params[2].get_str());
    CScript scriptPubKey(scriptData.begin(), scriptData.end());

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3)
        nHashType = ParseSighashString(params[3].get_str());

    uint256 hash = SignatureHash(scriptPubKey, tx, nInput, nHashType);

    Object result;
    result.push_back(Pair("sighash", hash.ToString()));
    result.push_back(Pair("inputIndex", nInput));
    result.push_back(Pair("hashType", nHashType));

    string strHashName;
    switch (nHashType & 0x1f)
    {
    case SIGHASH_ALL: strHashName = "ALL"; break;
    case SIGHASH_NONE: strHashName = "NONE"; break;
    case SIGHASH_SINGLE: strHashName = "SINGLE"; break;
    default: strHashName = "UNKNOWN"; break;
    }
    if (nHashType & SIGHASH_ANYONECANPAY)
        strHashName += "|ANYONECANPAY";
    result.push_back(Pair("hashTypeName", strHashName));
    result.push_back(Pair("scriptPubKeyAsm", scriptPubKey.ToString()));
    return result;
}


Value decodescriptsig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 2)
        throw runtime_error(
            "decodescriptsig <scriptsig hex> <scriptpubkey hex>\n"
            "Decode a scriptSig in context of its scriptPubKey.\n"
            "\nShows each push element with its role based on the script type,\n"
            "and reports whether the pair would verify.\n"
            "\nArguments:\n"
            "1. scriptsig     (string) Hex-encoded scriptSig\n"
            "2. scriptpubkey  (string) Hex-encoded scriptPubKey");

    vector<unsigned char> sigData = ParseHex(params[0].get_str());
    CScript scriptSig(sigData.begin(), sigData.end());

    vector<unsigned char> pubData = ParseHex(params[1].get_str());
    CScript scriptPubKey(pubData.begin(), pubData.end());

    Object result;
    result.push_back(Pair("scriptSig", HexStr(scriptSig.begin(), scriptSig.end(), false)));
    result.push_back(Pair("scriptSigAsm", scriptSig.ToString()));
    result.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end(), false)));
    result.push_back(Pair("scriptPubKeyAsm", scriptPubKey.ToString()));

    int nRequired;
    Array addresses;
    string strType = ClassifyScript(scriptPubKey, nRequired, addresses);
    result.push_back(Pair("type", strType));

    Array pushElements;
    CScript::const_iterator pc = scriptSig.begin();
    opcodetype opcode;
    vector<unsigned char> vchData;
    int nPush = 0;
    while (scriptSig.GetOp(pc, opcode, vchData))
    {
        Object elem;
        if (opcode <= OP_PUSHDATA4 && vchData.size() > 0)
        {
            elem.push_back(Pair("index", nPush));
            elem.push_back(Pair("hex", HexStr(vchData.begin(), vchData.end(), false)));
            elem.push_back(Pair("size", (int)vchData.size()));

            if (strType == "pubkeyhash")
            {
                if (nPush == 0) elem.push_back(Pair("role", "signature"));
                else if (nPush == 1) elem.push_back(Pair("role", "pubkey"));
            }
            else if (strType == "pubkey")
            {
                if (nPush == 0) elem.push_back(Pair("role", "signature"));
            }
            else if (strType == "multisig")
            {
                if (nPush == 0) elem.push_back(Pair("role", "dummy (OP_0)"));
                else elem.push_back(Pair("role", strprintf("signature_%d", nPush)));
            }
            else if (strType == "hashlock" || strType == "hashlock-sha256")
            {
                if (nPush == 0) elem.push_back(Pair("role", "signature"));
                else if (nPush == 1) elem.push_back(Pair("role", "pubkey"));
                else if (nPush == 2) elem.push_back(Pair("role", "preimage"));
            }
            else
            {
                elem.push_back(Pair("role", strprintf("push_%d", nPush)));
            }
        }
        else if (opcode >= OP_1NEGATE && opcode <= OP_16)
        {
            elem.push_back(Pair("index", nPush));
            elem.push_back(Pair("opcode", GetOpName(opcode)));
            int val = (opcode == OP_1NEGATE) ? -1 : ((int)opcode - (int)(OP_1 - 1));
            elem.push_back(Pair("value", val));

            if (strType == "multisig" && nPush == 0)
                elem.push_back(Pair("role", "dummy (OP_0)"));
            else
                elem.push_back(Pair("role", strprintf("push_%d", nPush)));
        }
        else if (opcode == OP_0)
        {
            elem.push_back(Pair("index", nPush));
            elem.push_back(Pair("opcode", "OP_0"));
            elem.push_back(Pair("value", 0));
            if (strType == "multisig" && nPush == 0)
                elem.push_back(Pair("role", "dummy (required for CHECKMULTISIG bug)"));
            else
                elem.push_back(Pair("role", strprintf("push_%d", nPush)));
        }
        else
        {
            elem.push_back(Pair("index", nPush));
            elem.push_back(Pair("opcode", GetOpName(opcode)));
            elem.push_back(Pair("role", "non-push opcode (invalid in scriptSig post-activation)"));
        }
        pushElements.push_back(elem);
        nPush++;
    }
    result.push_back(Pair("elements", pushElements));
    result.push_back(Pair("pushCount", nPush));
    result.push_back(Pair("isPushOnly", scriptSig.IsPushOnly()));

    return result;
}


Value verifyscriptpair(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error(
            "verifyscriptpair <tx hex> <input index> <scriptpubkey hex> [flags]\n"
            "Execute scriptSig + scriptPubKey verification against a real transaction.\n"
            "\nUnlike validatescript, this tests OP_CHECKSIG/OP_CHECKMULTISIG\n"
            "because it has the full transaction context.\n"
            "\nArguments:\n"
            "1. tx             (string) Hex-encoded signed raw transaction\n"
            "2. input          (int)    Input index to verify (0-based)\n"
            "3. scriptpubkey   (string) Hex-encoded scriptPubKey of the output being spent\n"
            "4. flags          (string) \"exec\" (default) or \"legacy\"\n"
            "\nReturns whether the script pair verifies successfully.");

    vector<unsigned char> txData = ParseHex(params[0].get_str());
    CDataStream ssData(txData, SER_NETWORK);
    CTransaction tx;
    try {
        ssData >> tx;
    } catch (std::exception &e) {
        throw runtime_error("TX decode failed");
    }

    int nInput = params[1].get_int();
    if (nInput < 0 || nInput >= (int)tx.vin.size())
        throw runtime_error(strprintf("Input index %d out of range (tx has %d inputs)", nInput, (int)tx.vin.size()));

    vector<unsigned char> scriptData = ParseHex(params[2].get_str());
    CScript scriptPubKey(scriptData.begin(), scriptData.end());

    unsigned int flags = SCRIPT_VERIFY_EXEC;
    if (params.size() > 3)
    {
        string strFlags = params[3].get_str();
        if (strFlags == "legacy")
            flags = SCRIPT_VERIFY_NONE;
        else if (strFlags != "exec")
            throw runtime_error("Invalid flags. Use \"exec\" or \"legacy\"");
    }

    const CScript& scriptSig = tx.vin[nInput].scriptSig;

    bool fVerified = VerifyScript(scriptSig, scriptPubKey, tx, nInput, 0, flags);

    Object result;
    result.push_back(Pair("verified", fVerified));
    result.push_back(Pair("inputIndex", nInput));
    result.push_back(Pair("scriptSig", HexStr(scriptSig.begin(), scriptSig.end(), false)));
    result.push_back(Pair("scriptSigAsm", scriptSig.ToString()));
    result.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end(), false)));
    result.push_back(Pair("scriptPubKeyAsm", scriptPubKey.ToString()));

    int nReq;
    Array addrs;
    result.push_back(Pair("type", ClassifyScript(scriptPubKey, nReq, addrs)));
    result.push_back(Pair("flags", (flags & SCRIPT_VERIFY_EXEC) ? "exec" : "legacy"));

    if (!fVerified)
    {
        Array diagnostics;

        if (!scriptSig.IsPushOnly() && (flags & SCRIPT_VERIFY_EXEC))
            diagnostics.push_back("scriptSig contains non-push opcodes (forbidden post-activation)");

        if (scriptPubKey.size() > MAX_SCRIPT_SIZE)
            diagnostics.push_back("scriptPubKey exceeds MAX_SCRIPT_SIZE");

        if (scriptSig.size() > MAX_SCRIPT_SIZE)
            diagnostics.push_back("scriptSig exceeds MAX_SCRIPT_SIZE");

        unsigned int nSigOps = GetSigOpCount(scriptPubKey) + GetSigOpCount(scriptSig);
        if (nSigOps > 0)
            diagnostics.push_back(strprintf("Script contains %d sigop(s) - signature verification required", nSigOps));

        vector<vector<unsigned char> > testStack;
        CScript::const_iterator pc2 = scriptSig.begin();
        opcodetype op2;
        vector<unsigned char> vd2;
        while (scriptSig.GetOp(pc2, op2, vd2))
        {
            if (op2 == OP_0)
                testStack.push_back(vector<unsigned char>());
            else if (op2 >= OP_1 && op2 <= OP_16)
                testStack.push_back(vector<unsigned char>(1, (unsigned char)((int)op2 - (int)(OP_1 - 1))));
            else if (op2 == OP_1NEGATE)
                testStack.push_back(vector<unsigned char>(1, 0x81));
            else if (vd2.size() > 0)
                testStack.push_back(vd2);
        }

        CTransaction txDummy;
        txDummy.vin.resize(1);
        txDummy.vout.resize(1);
        bool fPubKeyExec = EvalScript(testStack, scriptPubKey, txDummy, 0, 0, flags);
        if (!fPubKeyExec)
            diagnostics.push_back("scriptPubKey execution failed with the given stack inputs");
        else if (testStack.empty())
            diagnostics.push_back("scriptPubKey execution left empty stack");
        else
        {
            bool fTop = false;
            const vector<unsigned char>& vchTop = testStack.back();
            for (unsigned int j = 0; j < vchTop.size(); j++)
            {
                if (vchTop[j] != 0) {
                    if (j == vchTop.size()-1 && vchTop[j] == 0x80) break;
                    fTop = true;
                    break;
                }
            }
            if (!fTop)
                diagnostics.push_back("scriptPubKey execution result is false/zero");
        }

        if (diagnostics.size() > 0)
            result.push_back(Pair("diagnostics", diagnostics));
    }

    return result;
}


Value getindexerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getindexerinfo\n"
            "Returns information about the UTXO/address indexer status.");

    Object result;
    result.push_back(Pair("enabled", fUseIndexer));

    if (fUseIndexer)
    {
        CTxDB txdb("r");
        int nIndexerHeight = -1;
        txdb.ReadIndexerHeight(nIndexerHeight);
        txdb.Close();
        result.push_back(Pair("height", nIndexerHeight));
        result.push_back(Pair("bestHeight", nBestHeight));
        result.push_back(Pair("synced", nIndexerHeight == nBestHeight));
    }

    return result;
}



Value getaddresstxids(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "getaddresstxids <address>\n"
            "Returns list of transaction IDs involving <address>. Requires -indexer.");

    if (!fUseIndexer)
        throw runtime_error("getaddresstxids requires node started with -indexer flag");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    CTxDB txdb("r");
    vector<uint256> vtxids;
    if (!txdb.ReadAddrTxids(hash160, vtxids))
        throw runtime_error("ReadAddrTxids failed");
    txdb.Close();

    Array result;
    for (size_t i = 0; i < vtxids.size(); i++)
        result.push_back(vtxids[i].GetHex());

    return result;
}


Value getaddressutxos(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "getaddressutxos <address>\n"
            "Returns unspent outputs for <address>. Requires -indexer.");

    if (!fUseIndexer)
        throw runtime_error("getaddressutxos requires node started with -indexer flag");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    CTxDB txdb("r");
    vector<pair<COutPoint, pair<int64, pair<int, bool> > > > vUTXOs;
    if (!txdb.ReadAddrUTXOs(hash160, vUTXOs))
        throw runtime_error("ReadAddrUTXOs failed");
    txdb.Close();

    int nMaturity = GetCoinbaseMaturity();

    Array result;
    for (size_t i = 0; i < vUTXOs.size(); i++)
    {
        const COutPoint& outpoint = vUTXOs[i].first;
        int64 nValue = vUTXOs[i].second.first;
        int nHeight = vUTXOs[i].second.second.first;
        bool fCoinBase = vUTXOs[i].second.second.second;
        int nConfs = nBestHeight - nHeight + 1;

        if (fCoinBase && nConfs < nMaturity)
            continue;

        Object entry;
        entry.push_back(Pair("txid", outpoint.hash.GetHex()));
        entry.push_back(Pair("vout", (int)outpoint.n));
        entry.push_back(Pair("value", (double)nValue / (double)COIN));
        entry.push_back(Pair("height", nHeight));
        entry.push_back(Pair("confirmations", nConfs));
        entry.push_back(Pair("coinbase", fCoinBase));
        result.push_back(entry);
    }

    return result;
}


Value getaddressbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "getaddressbalance <address>\n"
            "Returns confirmed balance of <address>. Requires -indexer.");

    if (!fUseIndexer)
        throw runtime_error("getaddressbalance requires node started with -indexer flag");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    CTxDB txdb("r");
    vector<pair<COutPoint, pair<int64, pair<int, bool> > > > vUTXOs;
    if (!txdb.ReadAddrUTXOs(hash160, vUTXOs))
        throw runtime_error("ReadAddrUTXOs failed");
    txdb.Close();

    int nMaturity = GetCoinbaseMaturity();

    int64 nBalance = 0;
    for (size_t i = 0; i < vUTXOs.size(); i++)
    {
        int nHeight = vUTXOs[i].second.second.first;
        bool fCoinBase = vUTXOs[i].second.second.second;
        int nConfs = nBestHeight - nHeight + 1;

        if (fCoinBase && nConfs < nMaturity)
            continue;

        nBalance += vUTXOs[i].second.first;
    }

    return (double)nBalance / (double)COIN;
}


Value getnewstealthaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewstealthaddress [label]\n"
            "Returns a new stealth address for receiving unlinkable payments.\n"
            "Stealth addresses implement Satoshi's 'key blinding' concept:\n"
            "each payment creates a unique one-time address that only the\n"
            "receiver can detect and spend from.");

    string strLabel;
    if (params.size() > 0)
        strLabel = params[0].get_str();

    CStealthAddress sxAddr;
    CKey scanKey, spendKey;
    if (!GenerateStealthAddress(sxAddr, scanKey, spendKey))
        throw runtime_error("Failed to generate stealth address");

    sxAddr.label = strLabel;

    CWalletDB walletdb;
    if (!walletdb.WriteStealthAddress(sxAddr))
        throw runtime_error("Failed to write stealth address to wallet");
    if (!walletdb.WriteStealthScanKey(sxAddr.scan_pubkey, scanKey.GetPrivKey()))
        throw runtime_error("Failed to write stealth scan key");
    if (!walletdb.WriteStealthSpendKey(sxAddr.spend_pubkey, spendKey.GetPrivKey()))
        throw runtime_error("Failed to write stealth spend key");

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        vStealthAddresses.push_back(sxAddr);
    }

    Object result;
    result.push_back(Pair("stealthaddress", sxAddr.Encoded()));
    result.push_back(Pair("label", strLabel));
    result.push_back(Pair("scan_pubkey", HexStr(sxAddr.scan_pubkey.begin(), sxAddr.scan_pubkey.end(), false)));
    result.push_back(Pair("spend_pubkey", HexStr(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end(), false)));
    return result;
}


Value liststealthaddresses(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "liststealthaddresses\n"
            "List all stealth addresses in the wallet.");

    Array ret;
    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        foreach(const CStealthAddress& sxAddr, vStealthAddresses)
        {
            Object entry;
            entry.push_back(Pair("stealthaddress", sxAddr.Encoded()));
            entry.push_back(Pair("label", sxAddr.label));
            entry.push_back(Pair("scan_pubkey", HexStr(sxAddr.scan_pubkey.begin(), sxAddr.scan_pubkey.end(), false)));
            entry.push_back(Pair("spend_pubkey", HexStr(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end(), false)));
            ret.push_back(entry);
        }
    }
    return ret;
}


Value decodestealthaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decodestealthaddress <stealthaddress>\n"
            "Decode a stealth address and show its component public keys.");

    string strAddr = params[0].get_str();
    CStealthAddress sxAddr;
    if (!sxAddr.SetEncoded(strAddr))
        throw runtime_error("Invalid stealth address");

    Object result;
    result.push_back(Pair("valid", true));
    result.push_back(Pair("scan_pubkey", HexStr(sxAddr.scan_pubkey.begin(), sxAddr.scan_pubkey.end(), false)));
    result.push_back(Pair("spend_pubkey", HexStr(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end(), false)));
    return result;
}


// ---------------------------------------------------------------------------
// ATOM RPC commands
// ---------------------------------------------------------------------------

static inline double AtomToDouble(int64 nRaw)
{
    double v = (double)nRaw / (double)ATOM_DECIMALS;
    return floor(v * 1e6 + 0.5) / 1e6;
}

static Object GetAtomBalanceForHash160(const uint160& hash160, const string& strAddress)
{
    CTxDB txdb("r");
    int64 nConfirmed = 0;
    uint32_t nNonceIgnored = 0;
    txdb.ReadAtomBalance(hash160, nConfirmed, nNonceIgnored);
    txdb.Close();

    int64 nPendingIn  = 0;
    int64 nPendingOut = 0;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            const CAtomTransfer& t = it->second;
            if (t.nType == ATOM_TYPE_TRANSFER)
            {
                if (t.addrTo   == hash160) nPendingIn  += t.nAmount;
                if (t.addrFrom == hash160) nPendingOut += t.nAmount;
            }
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
            {
                if (t.addrFrom == hash160) nPendingOut += t.nAmount;
            }
        }
    }

    int64 nSpendable = nConfirmed - nPendingOut;
    if (nSpendable < 0) nSpendable = 0;

    Object result;
    result.push_back(Pair("address",          strAddress));
    result.push_back(Pair("balance",     AtomToDouble(nConfirmed)));
    result.push_back(Pair("spendable",   AtomToDouble(nSpendable)));
    result.push_back(Pair("pending_in",  AtomToDouble(nPendingIn)));
    result.push_back(Pair("pending_out", AtomToDouble(nPendingOut)));
    return result;
}

Value getatombalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getatombalance\n"
            "Returns the wallet's total confirmed ATOM balance.");

    int64 nTotal = 0;
    CTxDB txdb("r");

    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<uint160, vector<unsigned char> >::iterator it = mapPubKeys.begin();
             it != mapPubKeys.end(); ++it)
        {
            const uint160& hash160 = it->first;
            int64 nConfirmed = 0;
            uint32_t nNonceIgnored = 0;
            txdb.ReadAtomBalance(hash160, nConfirmed, nNonceIgnored);
            nTotal += nConfirmed;
        }
    }

    txdb.Close();
    return AtomToDouble(nTotal);
}


Value getaddressatombalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressatombalance <address>\n"
            "Returns the confirmed ATOM balance of <address>. Requires -indexer.");

    if (!fUseIndexer)
        throw runtime_error("getaddressatombalance requires node started with -indexer flag");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    CTxDB txdb("r");
    int64 nConfirmed = 0;
    uint32_t nNonceIgnored = 0;
    txdb.ReadAtomBalance(hash160, nConfirmed, nNonceIgnored);
    txdb.Close();

    return AtomToDouble(nConfirmed);
}


Value getatomtx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getatomtx <txid>\n"
            "Returns ATOM transfer data for a transaction.\n"
            "Finds the tx in the mempool (status=accepted) or confirmed blocks (status=mined).");

    string strTxid = params[0].get_str();
    uint256 txhash;
    txhash.SetHex(strTxid);

    // Check mempool first
    {
        CRITICAL_BLOCK(cs_mapAtomMempool)
        {
            std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.find(txhash);
            if (it != mapAtomMempoolTx.end())
            {
                const CAtomTransfer& t = it->second;
                string strType;
                if (t.nType == ATOM_TYPE_TRANSFER) strType = "transfer";
                else if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
                else if (t.nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
                else if (t.nType == ATOM_TYPE_COINBASE) strType = "coinbase";
                else strType = "unknown";

                Object result;
                result.push_back(Pair("txid",       txhash.ToString()));
                result.push_back(Pair("type",       strType));
                result.push_back(Pair("status",     string("pending")));
                result.push_back(Pair("from",       Hash160ToAddress(t.addrFrom)));
                if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                    result.push_back(Pair("solana_destination", SolAddrToBase58(t.solAddrTo)));
                else
                    result.push_back(Pair("to",     Hash160ToAddress(t.addrTo)));
                result.push_back(Pair("amount",     AtomToDouble(t.nAmount)));
                result.push_back(Pair("amount_raw", (int64_t)t.nAmount));
                result.push_back(Pair("nonce",      (int64_t)t.nNonce));
                return result;
            }
        }
    }

    // Fall back to confirmed DB
    CTxDB txdb("r");
    int nHeight = 0;
    unsigned int nTime = 0;
    unsigned char nType = 0;
    uint160 addrFrom(0), addrTo(0);
    int64 nAmount = 0;
    uint32_t nNonce = 0;
    unsigned char solAddrTo[32] = {0};

    if (!txdb.ReadAtomTx(txhash, nHeight, nTime, nType, addrFrom, addrTo, nAmount, nNonce, solAddrTo))
        throw runtime_error("ATOM transfer not found for txid: " + strTxid);
    txdb.Close();

    string strType;
    if (nType == ATOM_TYPE_TRANSFER) strType = "transfer";
    else if (nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
    else if (nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
    else if (nType == ATOM_TYPE_COINBASE) strType = "coinbase";
    else strType = "unknown";

    Object result;
    result.push_back(Pair("txid",       txhash.ToString()));
    result.push_back(Pair("type",       strType));
    result.push_back(Pair("status",     string("mined")));
    result.push_back(Pair("height",     nHeight));
    result.push_back(Pair("time",       (int64_t)nTime));
    result.push_back(Pair("from",       Hash160ToAddress(addrFrom)));
    if (nType == ATOM_TYPE_BRIDGE_TO_SOL)
    {
        result.push_back(Pair("solana_destination", SolAddrToBase58(solAddrTo)));
    }
    else
    {
        result.push_back(Pair("to",     Hash160ToAddress(addrTo)));
    }
    result.push_back(Pair("amount",     AtomToDouble(nAmount)));
    result.push_back(Pair("amount_raw", (int64_t)nAmount));
    result.push_back(Pair("nonce",      (int64_t)nNonce));
    return result;
}


Value getatomhistory(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getatomhistory <address> [count=50]\n"
            "Returns ATOM transactions involving <address>, newest first.\n"
            "Includes both mempool (status=pending) and mined (status=mined) transactions.\n"
            "Requires -indexer.");

    if (!fUseIndexer)
        throw runtime_error("getatomhistory requires node started with -indexer flag");

    string strAddress = params[0].get_str();
    int nCount = 50;
    if (params.size() > 1)
        nCount = params[1].get_int();

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    Array ret;

    // Collect mempool txids for this address and emit them first.
    // Also build a set for dedup — a tx that was just mined may still linger in the mempool
    // index for a brief window and would appear in both sections without this guard.
    std::set<uint256> setMempool;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            const CAtomTransfer& t = it->second;
            if (t.addrFrom != hash160 && t.addrTo != hash160)
                continue;

            setMempool.insert(it->first);

            string strType;
            if (t.nType == ATOM_TYPE_TRANSFER) strType = "transfer";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
            else if (t.nType == ATOM_TYPE_COINBASE) strType = "coinbase";
            else strType = "unknown";

            string strDir;
            if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                strDir = "bridge_to_sol";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
                strDir = "bridge_to_bitok";
            else if (t.nType == ATOM_TYPE_COINBASE)
                strDir = "coinbase";
            else
                strDir = (t.addrTo == hash160) ? "received" : "sent";

            Object entry;
            entry.push_back(Pair("txid",      it->first.ToString()));
            entry.push_back(Pair("type",      strType));
            entry.push_back(Pair("status",    string("pending")));
            entry.push_back(Pair("direction", strDir));
            entry.push_back(Pair("from",      Hash160ToAddress(t.addrFrom)));
            if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                entry.push_back(Pair("solana_destination", SolAddrToBase58(t.solAddrTo)));
            else
                entry.push_back(Pair("to",    Hash160ToAddress(t.addrTo)));
            entry.push_back(Pair("amount",    AtomToDouble(t.nAmount)));
            entry.push_back(Pair("amount_raw",(int64_t)t.nAmount));
            entry.push_back(Pair("nonce",     (int64_t)t.nNonce));
            ret.push_back(entry);
        }
    }

    CTxDB txdb("r");
    vector<uint256> vtxids;
    txdb.ReadAtomAddrTxids(hash160, vtxids);

    int nStart = (int)vtxids.size() - nCount;
    if (nStart < 0) nStart = 0;

    for (int i = (int)vtxids.size() - 1; i >= nStart; i--)
    {
        const uint256& txhash = vtxids[i];

        if (setMempool.count(txhash))
            continue;

        int nHeight = 0;
        unsigned int nTime = 0;
        unsigned char nType = 0;
        uint160 addrFrom(0), addrTo(0);
        int64 nAmount = 0;
        uint32_t nNonce = 0;
        unsigned char solAddrTo[32] = {0};

        if (!txdb.ReadAtomTx(txhash, nHeight, nTime, nType, addrFrom, addrTo, nAmount, nNonce, solAddrTo))
            continue;

        string strType;
        if (nType == ATOM_TYPE_TRANSFER) strType = "transfer";
        else if (nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
        else if (nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
        else if (nType == ATOM_TYPE_COINBASE) strType = "coinbase";
        else strType = "unknown";

        bool fFrom = (addrFrom == hash160);
        bool fTo   = (addrTo   == hash160);

        string strDir;
        if (nType == ATOM_TYPE_BRIDGE_TO_SOL)
            strDir = "bridge_to_sol";
        else if (nType == ATOM_TYPE_BRIDGE_TO_BITOK)
            strDir = "bridge_to_bitok";
        else if (nType == ATOM_TYPE_COINBASE)
            strDir = "coinbase";
        else if (fFrom && fTo)
            strDir = "self";
        else
            strDir = fTo ? "received" : "sent";

        Object entry;
        entry.push_back(Pair("txid",      txhash.ToString()));
        entry.push_back(Pair("type",      strType));
        entry.push_back(Pair("status",    string("mined")));
        entry.push_back(Pair("direction", strDir));
        entry.push_back(Pair("height",    nHeight));
        entry.push_back(Pair("time",      (int64_t)nTime));
        entry.push_back(Pair("from",      Hash160ToAddress(addrFrom)));
        if (nType == ATOM_TYPE_BRIDGE_TO_SOL)
        {
            entry.push_back(Pair("solana_destination", SolAddrToBase58(solAddrTo)));
        }
        else
        {
            entry.push_back(Pair("to",    Hash160ToAddress(addrTo)));
        }
        entry.push_back(Pair("amount",    AtomToDouble(nAmount)));
        entry.push_back(Pair("amount_raw",(int64_t)nAmount));
        entry.push_back(Pair("nonce",     (int64_t)nNonce));
        ret.push_back(entry);
    }

    txdb.Close();
    return ret;
}


Value sendatom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "sendatom <toaddress> <amount>\n"
            "Send <amount> ATOM to <toaddress>.\n"
            "Funds are drawn from all wallet addresses as needed, largest balance first.\n"
            "If a single address covers the amount, one transaction is broadcast.\n"
            "Otherwise multiple transactions are broadcast automatically.\n"
            "<amount> is in ATOM units (e.g. 1.5 = 1.5 ATOM).\n"
            "Use createatomrawtx for web wallets or custom key management.");

    string strTo   = params[0].get_str();
    double dAmount = params[1].get_real();

    if (dAmount <= 0.0)
        throw runtime_error("Amount must be positive");
    if (dAmount > (double)ATOM_MAX_SUPPLY / (double)ATOM_DECIMALS)
        throw runtime_error("Amount exceeds ATOM max supply");

    uint160 hash160To;
    if (!AddressToHash160(strTo, hash160To))
        throw runtime_error("Invalid to address");

    int64 nAmount = roundint64(dAmount * (double)ATOM_DECIMALS);
    if (nAmount <= 0)
        throw runtime_error("Amount too small");

    // Collect all wallet addresses with a spendable ATOM balance, sorted largest first.
    struct AtomSource
    {
        uint160  h160;
        string   strAddr;
        int64    nSpendable;
        uint32_t nConfirmedNonce;
    };
    std::vector<AtomSource> sources;
    int64 nTotalSpendable = 0;

    {
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapKeys)
        {
            for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
                 it != mapKeys.end(); ++it)
            {
                const vector<unsigned char>& vchPubKey = it->first;
                uint160 h160 = Hash160(vchPubKey);

                int64    nConf  = 0;
                uint32_t nNonce = 0;
                txdb.ReadAtomBalance(h160, nConf, nNonce);

                int64 nSpend = AtomGetEffectiveBalance(h160, nConf);
                if (nSpend > 0)
                {
                    AtomSource s;
                    s.h160             = h160;
                    s.strAddr          = PubKeyToAddress(vchPubKey);
                    s.nSpendable       = nSpend;
                    s.nConfirmedNonce  = nNonce;
                    sources.push_back(s);
                    nTotalSpendable   += nSpend;
                }
            }
        }
        txdb.Close();
    }

    if (sources.empty())
        throw runtime_error("No wallet addresses with ATOM balance found");

    if (nTotalSpendable < nAmount)
        throw runtime_error(strprintf("Insufficient ATOM balance in wallet: total spendable %" PRI64d
                                      " need %" PRI64d, nTotalSpendable, nAmount));

    // Sort sources: largest spendable balance first so we minimise the number of transactions.
    std::sort(sources.begin(), sources.end(),
              [](const AtomSource& a, const AtomSource& b){ return a.nSpendable > b.nSpendable; });

    CScript scriptDest;
    if (!scriptDest.SetBitcoinAddress(strTo))
        throw runtime_error("Failed to build destination script for: " + strTo);

    int64      nRemaining = nAmount;
    Array      txids;
    string     strFirstFrom;

    for (size_t i = 0; i < sources.size() && nRemaining > 0; i++)
    {
        const AtomSource& src = sources[i];

        int64 nSend = (src.nSpendable >= nRemaining) ? nRemaining : src.nSpendable;

        // Determine next valid nonce for this source address.
        uint32_t nNextNonce = src.nConfirmedNonce + 1;
        CRITICAL_BLOCK(cs_mapAtomMempool)
        {
            std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(src.h160);
            if (it != mapAtomMempoolNonce.end() && it->second >= nNextNonce)
                nNextNonce = it->second + 1;
        }

        CAtomTransfer transfer;
        transfer.nVersion = ATOM_VERSION;
        transfer.nType    = ATOM_TYPE_TRANSFER;
        transfer.addrFrom = src.h160;
        transfer.addrTo   = hash160To;
        transfer.nAmount  = nSend;
        transfer.nNonce   = nNextNonce;

        CScript scriptOpReturn = BuildAtomScript(transfer);

        CWalletTx wtx;
        CKey changeKey;
        int64 nFeeRequired = 0;

        if (!CreateStealthTransaction(scriptDest, scriptOpReturn, DUST_THRESHOLD, wtx, changeKey, nFeeRequired))
        {
            string strError;
            if (DUST_THRESHOLD + nFeeRequired > GetBalance())
                strError = strprintf("Insufficient Bitok balance to pay fee. Need %s",
                                     FormatMoney(DUST_THRESHOLD + nFeeRequired).c_str());
            else
                strError = "Transaction creation failed";
            throw runtime_error(strError);
        }

        if (!CommitTransaction(wtx, changeKey))
            throw runtime_error("Transaction broadcast failed");

        txids.push_back(wtx.GetHash().ToString());

        if (strFirstFrom.empty())
            strFirstFrom = src.strAddr;

        nRemaining -= nSend;
    }

    if (txids.size() == 1)
    {
        Object result;
        result.push_back(Pair("txid",       txids[0]));
        result.push_back(Pair("from",       strFirstFrom));
        result.push_back(Pair("to",         strTo));
        result.push_back(Pair("amount",     dAmount));
        result.push_back(Pair("amount_raw", (int64_t)nAmount));
        result.push_back(Pair("status",     string("pending")));
        return result;
    }

    Object result;
    result.push_back(Pair("txids",      txids));
    result.push_back(Pair("to",         strTo));
    result.push_back(Pair("amount",     dAmount));
    result.push_back(Pair("amount_raw", (int64_t)nAmount));
    result.push_back(Pair("tx_count",   (int64_t)txids.size()));
    result.push_back(Pair("status",     string("pending")));
    return result;
}


Value listatomtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "listatomtransactions [count=50]\n"
            "Returns ATOM transactions involving any address in the local wallet, newest first.\n"
            "Includes both mempool (status=pending) and mined (status=mined) transactions.\n"
            "Does NOT require -indexer — works on any node with a wallet.");

    int nCount = 50;
    if (params.size() > 0)
        nCount = params[0].get_int();
    if (nCount <= 0)
        throw runtime_error("count must be positive");

    // Collect all wallet hash160s
    std::set<uint160> setWallet;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
             it != mapKeys.end(); ++it)
        {
            setWallet.insert(Hash160(it->first));
        }
    }

    if (setWallet.empty())
        return Array();

    Array ret;

    // Mempool first: scan all pending ATOM transfers for any wallet address
    std::set<uint256> setMempool;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        for (std::map<uint256, CAtomTransfer>::iterator it = mapAtomMempoolTx.begin();
             it != mapAtomMempoolTx.end(); ++it)
        {
            const CAtomTransfer& t = it->second;
            bool fFromWallet = setWallet.count(t.addrFrom) > 0;
            bool fToWallet   = setWallet.count(t.addrTo)   > 0;
            if (!fFromWallet && !fToWallet)
                continue;

            setMempool.insert(it->first);

            string strType;
            if (t.nType == ATOM_TYPE_TRANSFER) strType = "transfer";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
            else if (t.nType == ATOM_TYPE_COINBASE) strType = "coinbase";
            else strType = "unknown";

            string strDir;
            if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                strDir = "bridge_to_sol";
            else if (t.nType == ATOM_TYPE_BRIDGE_TO_BITOK)
                strDir = "bridge_to_bitok";
            else if (t.nType == ATOM_TYPE_COINBASE)
                strDir = "coinbase";
            else if (fToWallet && fFromWallet)
                strDir = "self";
            else
                strDir = fToWallet ? "received" : "sent";

            Object entry;
            entry.push_back(Pair("txid",      it->first.ToString()));
            entry.push_back(Pair("type",      strType));
            entry.push_back(Pair("status",    string("pending")));
            entry.push_back(Pair("direction", strDir));
            entry.push_back(Pair("from",      Hash160ToAddress(t.addrFrom)));
            if (t.nType == ATOM_TYPE_BRIDGE_TO_SOL)
                entry.push_back(Pair("solana_destination", SolAddrToBase58(t.solAddrTo)));
            else
                entry.push_back(Pair("to",    Hash160ToAddress(t.addrTo)));
            entry.push_back(Pair("amount",    AtomToDouble(t.nAmount)));
            entry.push_back(Pair("amount_raw",(int64_t)t.nAmount));
            entry.push_back(Pair("nonce",     (int64_t)t.nNonce));
            ret.push_back(entry);
        }
    }

    // Mined: scan wallet transactions for ATOM payloads
    vector<pair<int64, uint256> > vSorted;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = it->second;
            if (wtx.GetDepthInMainChain() <= 0)
                continue;
            vSorted.push_back(make_pair(wtx.nTimeReceived, wtx.GetHash()));
        }
    }
    sort(vSorted.rbegin(), vSorted.rend());

    CTxDB txdb("r");
    int nMined = 0;
    for (vector<pair<int64, uint256> >::iterator it = vSorted.begin();
         it != vSorted.end() && nMined < nCount; ++it)
    {
        const uint256& txhash = it->second;
        if (setMempool.count(txhash))
            continue;

        int nHeight = 0;
        unsigned int nTime = 0;
        unsigned char nType = 0;
        uint160 addrFrom(0), addrTo(0);
        int64 nAmount = 0;
        uint32_t nNonce = 0;
        unsigned char solAddrTo[32] = {0};

        if (!txdb.ReadAtomTx(txhash, nHeight, nTime, nType, addrFrom, addrTo, nAmount, nNonce, solAddrTo))
            continue;

        bool fFromWallet = setWallet.count(addrFrom) > 0;
        bool fToWallet   = setWallet.count(addrTo)   > 0;
        if (!fFromWallet && !fToWallet)
            continue;

        string strType;
        if (nType == ATOM_TYPE_TRANSFER) strType = "transfer";
        else if (nType == ATOM_TYPE_BRIDGE_TO_SOL) strType = "bridge_to_sol";
        else if (nType == ATOM_TYPE_BRIDGE_TO_BITOK) strType = "bridge_to_bitok";
        else if (nType == ATOM_TYPE_COINBASE) strType = "coinbase";
        else strType = "unknown";

        string strDir;
        if (nType == ATOM_TYPE_BRIDGE_TO_SOL)
            strDir = "bridge_to_sol";
        else if (nType == ATOM_TYPE_BRIDGE_TO_BITOK)
            strDir = "bridge_to_bitok";
        else if (nType == ATOM_TYPE_COINBASE)
            strDir = "coinbase";
        else if (fToWallet && fFromWallet)
            strDir = "self";
        else
            strDir = fToWallet ? "received" : "sent";

        Object entry;
        entry.push_back(Pair("txid",      txhash.ToString()));
        entry.push_back(Pair("type",      strType));
        entry.push_back(Pair("status",    string("mined")));
        entry.push_back(Pair("direction", strDir));
        entry.push_back(Pair("height",    nHeight));
        entry.push_back(Pair("time",      (int64_t)nTime));
        entry.push_back(Pair("from",      Hash160ToAddress(addrFrom)));
        if (nType == ATOM_TYPE_BRIDGE_TO_SOL)
        {
            entry.push_back(Pair("solana_destination", SolAddrToBase58(solAddrTo)));
        }
        else
        {
            entry.push_back(Pair("to",    Hash160ToAddress(addrTo)));
        }
        entry.push_back(Pair("amount",    AtomToDouble(nAmount)));
        entry.push_back(Pair("amount_raw",(int64_t)nAmount));
        entry.push_back(Pair("nonce",     (int64_t)nNonce));
        ret.push_back(entry);
        nMined++;
    }
    txdb.Close();

    return ret;
}


Value getnextatomnonce(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getnextatomnonce <address>\n"
            "Returns the next valid ATOM nonce for <address>.\n"
            "Accounts for both the confirmed nonce in the DB and any pending\n"
            "transactions already in the mempool, so the returned nonce is\n"
            "safe to use immediately in createatomrawtx or a manually built tx.");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw runtime_error("Invalid Bitok address");

    CTxDB txdb("r");
    int64 nConfirmedBalance = 0;
    uint32_t nConfirmedNonce = 0;
    txdb.ReadAtomBalance(hash160, nConfirmedBalance, nConfirmedNonce);
    txdb.Close();

    uint32_t nNextNonce = nConfirmedNonce + 1;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(hash160);
        if (it != mapAtomMempoolNonce.end() && it->second >= nNextNonce)
            nNextNonce = it->second + 1;
    }

    Object result;
    result.push_back(Pair("address",         strAddress));
    result.push_back(Pair("confirmed_nonce", (int64_t)nConfirmedNonce));
    result.push_back(Pair("next_nonce",      (int64_t)nNextNonce));
    return result;
}


Value createatomrawtx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error(
            "createatomrawtx <fromaddress> <toaddress> <amount> <nonce> [{\"txid\":\"id\",\"vout\":n},...]\n"
            "Build an unsigned raw transaction carrying an ATOM transfer in its OP_RETURN output.\n"
            "<fromaddress>  sender's Bitok address (HASH160 is embedded in the ATOM payload)\n"
            "<toaddress>    recipient's Bitok address\n"
            "<amount>       ATOM amount in ATOM units (e.g. 1.5 = 1.5 ATOM)\n"
            "<nonce>        per-sender monotonic counter; use getnextatomnonce to get the correct value\n"
            "[inputs]       optional array of UTXOs to fund the Bitok tx outputs;\n"
            "               if omitted the tx has no inputs (useful when the caller selects\n"
            "               coins and appends inputs externally before signing)\n"
            "Returns hex-encoded unsigned raw transaction.\n"
            "Sign with signrawtransaction, then broadcast with sendrawtransaction.\n"
            "The tx must include at least one output paying DUST_THRESHOLD (0.01 BITOK) to\n"
            "<toaddress> and enough coin inputs to cover that dust plus the network fee.");

    string strFrom   = params[0].get_str();
    string strTo     = params[1].get_str();
    double dAmount   = params[2].get_real();
    int64  nNonce    = params[3].get_int64();

    if (dAmount <= 0.0)
        throw runtime_error("Amount must be positive");
    if (dAmount > (double)ATOM_MAX_SUPPLY / (double)ATOM_DECIMALS)
        throw runtime_error("Amount exceeds ATOM max supply");
    if (nNonce <= 0 || nNonce > 0xffffffff)
        throw runtime_error("nonce must be a positive integer (use getnextatomnonce)");

    uint160 hash160From, hash160To;
    if (!AddressToHash160(strFrom, hash160From))
        throw runtime_error("Invalid from address");
    if (!AddressToHash160(strTo, hash160To))
        throw runtime_error("Invalid to address");

    int64 nAmount = roundint64(dAmount * (double)ATOM_DECIMALS);
    if (nAmount <= 0)
        throw runtime_error("Amount too small");

    CAtomTransfer transfer;
    transfer.nVersion = ATOM_VERSION;
    transfer.nType    = ATOM_TYPE_TRANSFER;
    transfer.addrFrom = hash160From;
    transfer.addrTo   = hash160To;
    transfer.nAmount  = nAmount;
    transfer.nNonce   = (uint32_t)nNonce;

    CScript scriptOpReturn = BuildAtomScript(transfer);

    CScript scriptDest;
    if (!scriptDest.SetBitcoinAddress(strTo))
        throw runtime_error("Failed to build destination script for: " + strTo);

    CTransaction rawTx;

    if (params.size() > 4)
    {
        const Array& inputs = params[4].get_array();
        foreach(const Value& input, inputs)
        {
            const Object& o = input.get_obj();
            Value txidVal = find_value(o, "txid");
            Value voutVal = find_value(o, "vout");
            if (txidVal.type() != str_type || voutVal.type() != int_type)
                throw runtime_error("Invalid input: each input must have txid (string) and vout (integer)");
            uint256 txid;
            txid.SetHex(txidVal.get_str());
            int nOutput = voutVal.get_int();
            if (nOutput < 0)
                throw runtime_error("Invalid parameter, vout must be non-negative");
            rawTx.vin.push_back(CTxIn(COutPoint(txid, nOutput)));
        }
    }

    rawTx.vout.push_back(CTxOut(DUST_THRESHOLD, scriptDest));
    rawTx.vout.push_back(CTxOut(0, scriptOpReturn));

    CDataStream ss(SER_NETWORK);
    ss << rawTx;

    Object result;
    result.push_back(Pair("hex",        HexStr(ss.begin(), ss.end(), false)));
    result.push_back(Pair("from",       strFrom));
    result.push_back(Pair("to",         strTo));
    result.push_back(Pair("amount",     dAmount));
    result.push_back(Pair("amount_raw", (int64_t)nAmount));
    result.push_back(Pair("nonce",      (int64_t)nNonce));
    result.push_back(Pair("dust_out",   (int64_t)DUST_THRESHOLD));
    result.push_back(Pair("note",       string("Add coin inputs (listunspent), sign with signrawtransaction, broadcast with sendrawtransaction")));
    return result;
}


static string SolAddrToBase58(const unsigned char* p)
{
    return EncodeBase58(p, p + 32);
}

static bool ParseSolAddr(const string& str, unsigned char* out32)
{
    vector<unsigned char> vch;
    if (!DecodeBase58(str.c_str(), vch))
        return false;
    if (vch.size() != 32)
        return false;
    memcpy(out32, &vch[0], 32);
    return true;
}


Value bridgeatomtosol(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "bridgeatomtosol <solana_address> <amount>\n"
            "Bridge <amount> ATOM to a Solana address.\n"
            "Burns ATOM on the Bitok side; the bridge mints the equivalent on Solana.\n"
            "<solana_address>  Solana address in Base58 format (e.g. 6e5ZCp6V5ogM7o8bkJhcjzJSqJqbKMs4jAtSqDujDaxr).\n"
            "<amount>          ATOM amount in ATOM units (e.g. 1.5 = 1.5 ATOM).\n"
            "Funds are drawn from the wallet address with the largest spendable ATOM balance.");

    string strSolInput = params[0].get_str();
    double dAmount     = params[1].get_real();

    unsigned char solAddr[32];
    if (!ParseSolAddr(strSolInput, solAddr))
        throw runtime_error("Invalid Solana address: expected Base58 format (e.g. 6e5ZCp6V5ogM7o8bkJhcjzJSqJqbKMs4jAtSqDujDaxr)");

    bool fAllZero = true;
    for (int i = 0; i < 32; i++)
        if (solAddr[i] != 0) { fAllZero = false; break; }
    if (fAllZero)
        throw runtime_error("Solana address must not be all zeros");

    if (dAmount <= 0.0)
        throw runtime_error("Amount must be positive");
    if (dAmount > (double)ATOM_MAX_SUPPLY / (double)ATOM_DECIMALS)
        throw runtime_error("Amount exceeds ATOM max supply");

    int64 nAmount = roundint64(dAmount * (double)ATOM_DECIMALS);
    if (nAmount <= 0)
        throw runtime_error("Amount too small");

    // Find the wallet address with the largest spendable ATOM balance.
    uint160  h160Best;
    string   strAddrBest;
    int64    nSpendableBest   = 0;
    uint32_t nConfirmedNonce  = 0;

    {
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapKeys)
        {
            for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
                 it != mapKeys.end(); ++it)
            {
                const vector<unsigned char>& vchPubKey = it->first;
                uint160 h160 = Hash160(vchPubKey);

                int64    nConf  = 0;
                uint32_t nNonce = 0;
                txdb.ReadAtomBalance(h160, nConf, nNonce);

                int64 nSpend = AtomGetEffectiveBalance(h160, nConf);
                if (nSpend > nSpendableBest)
                {
                    nSpendableBest  = nSpend;
                    h160Best        = h160;
                    strAddrBest     = PubKeyToAddress(vchPubKey);
                    nConfirmedNonce = nNonce;
                }
            }
        }
        txdb.Close();
    }

    if (nSpendableBest <= 0)
        throw runtime_error("No wallet addresses with ATOM balance found");
    if (nSpendableBest < nAmount)
        throw runtime_error(strprintf("Insufficient ATOM balance: spendable %" PRI64d
                                      " need %" PRI64d, nSpendableBest, nAmount));

    uint32_t nNextNonce = nConfirmedNonce + 1;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(h160Best);
        if (it != mapAtomMempoolNonce.end() && it->second >= nNextNonce)
            nNextNonce = it->second + 1;
    }

    CAtomTransfer transfer;
    transfer.nVersion = ATOM_VERSION;
    transfer.nType    = ATOM_TYPE_BRIDGE_TO_SOL;
    transfer.addrFrom = h160Best;
    transfer.addrTo   = uint160(0);
    memcpy(transfer.solAddrTo, solAddr, 32);
    transfer.nAmount  = nAmount;
    transfer.nNonce   = nNextNonce;

    CScript scriptOpReturn = BuildAtomScript(transfer);

    // Dust output returns to sender (bridge burns ATOM; no Bitok recipient).
    CScript scriptSender;
    if (!scriptSender.SetBitcoinAddress(strAddrBest))
        throw runtime_error("Failed to build sender script for: " + strAddrBest);

    CWalletTx wtx;
    CKey changeKey;
    int64 nFeeRequired = 0;

    if (!CreateStealthTransaction(scriptSender, scriptOpReturn, DUST_THRESHOLD, wtx, changeKey, nFeeRequired))
    {
        string strError;
        if (DUST_THRESHOLD + nFeeRequired > GetBalance())
            strError = strprintf("Insufficient Bitok balance to pay fee. Need %s",
                                 FormatMoney(DUST_THRESHOLD + nFeeRequired).c_str());
        else
            strError = "Transaction creation failed";
        throw runtime_error(strError);
    }

    if (!CommitTransaction(wtx, changeKey))
        throw runtime_error("Transaction broadcast failed");

    Object result;
    result.push_back(Pair("txid",              wtx.GetHash().ToString()));
    result.push_back(Pair("from",              strAddrBest));
    result.push_back(Pair("solana_destination",SolAddrToBase58(solAddr)));
    result.push_back(Pair("amount",            dAmount));
    result.push_back(Pair("amount_raw",        (int64_t)nAmount));
    result.push_back(Pair("status",            string("pending")));
    return result;
}


Value getatominfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getatominfo\n"
            "Returns ATOM layer parameters and statistics.");

    Object result;
    result.push_back(Pair("name",          string("ATOM")));
    result.push_back(Pair("version",       (int)ATOM_VERSION));
    result.push_back(Pair("decimals",      ATOM_DECIMALS_INT));
    result.push_back(Pair("decimals_unit", (int64_t)ATOM_DECIMALS));
    result.push_back(Pair("max_supply",    AtomToDouble(ATOM_MAX_SUPPLY)));
    result.push_back(Pair("max_supply_raw",(int64_t)ATOM_MAX_SUPPLY));
    result.push_back(Pair("payload_size",       ATOM_PAYLOAD_SIZE));
    result.push_back(Pair("activation_height", ATOM_ACTIVATION_HEIGHT));
    result.push_back(Pair("max_nonce_gap",     (int)ATOM_MAX_NONCE_GAP));
    result.push_back(Pair("finality",          string("confirmed")));
    result.push_back(Pair("pending_txs",       (int64_t)mapAtomMempoolTx.size()));
    return result;
}


Value gettotalatomsupply(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettotalatomsupply\n"
            "Returns the total circulating ATOM supply — sum of all confirmed ATOM balances on-chain.");

    CTxDB txdb("r");
    int64 nTotal = 0;
    if (!txdb.SumAllAtomBalances(nTotal))
        throw runtime_error("Failed to read ATOM balances from database");
    txdb.Close();

    Object result;
    result.push_back(Pair("total_supply",     AtomToDouble(nTotal)));
    result.push_back(Pair("total_supply_raw", (int64_t)nTotal));
    result.push_back(Pair("max_supply",       AtomToDouble(ATOM_MAX_SUPPLY)));
    result.push_back(Pair("max_supply_raw",   (int64_t)ATOM_MAX_SUPPLY));
    result.push_back(Pair("decimals",         ATOM_DECIMALS_INT));
    return result;
}


// ---------------------------------------------------------------------------
// DEX RPC commands
// ---------------------------------------------------------------------------

Value dexsell(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "dexsell <atom_amount> <price>\n"
            "Sell ATOM on the DEX. Your ATOM is locked on-chain until taken or cancelled.\n"
            "  <atom_amount> ATOM quantity to sell (e.g. 100.5).\n"
            "  <price>       Ask price in BITOK per 1 ATOM (e.g. 0.05).");

    double dAtomAmount = params[0].get_real();
    if (dAtomAmount <= 0.0)
        throw runtime_error("atom_amount must be positive");
    int64 nAtomAmount = roundint64(dAtomAmount * (double)ATOM_DECIMALS);
    if (nAtomAmount <= 0)
        throw runtime_error("atom_amount too small");

    double dPrice = params[1].get_real();
    if (dPrice <= 0.0)
        throw runtime_error("price must be positive");
    int64 nPrice = roundint64(dPrice * (double)COIN);
    if (nPrice <= 0)
        throw runtime_error("price too small");

    uint160 h160Best;
    string strAddrBest;
    int64 nSpendableBest = 0;
    uint32_t nConfirmedNonce = 0;

    {
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapKeys)
        {
            for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
                 it != mapKeys.end(); ++it)
            {
                const vector<unsigned char>& vchPubKey = it->first;
                uint160 h160 = Hash160(vchPubKey);
                int64 nConf = 0;
                uint32_t nNon = 0;
                txdb.ReadAtomBalance(h160, nConf, nNon);
                int64 nSpend = AtomGetEffectiveBalance(h160, nConf);
                if (nSpend > nSpendableBest)
                {
                    h160Best = h160;
                    strAddrBest = PubKeyToAddress(vchPubKey);
                    nSpendableBest = nSpend;
                    nConfirmedNonce = nNon;
                }
            }
        }
        txdb.Close();
    }

    if (h160Best == 0)
        throw runtime_error("No wallet addresses found");

    if (nSpendableBest < nAtomAmount)
        throw runtime_error(strprintf("Insufficient ATOM balance: spendable %s need %s",
                                      FormatMoney(nSpendableBest).c_str(),
                                      FormatMoney(nAtomAmount).c_str()));

    int64 nTotalBitok = DexComputeBitokPayment(nAtomAmount, nPrice);
    if (nTotalBitok <= 0)
        throw runtime_error("Order too small: total BITOK payment rounds to zero");

    uint32_t nNextNonce = nConfirmedNonce + 1;
    CRITICAL_BLOCK(cs_mapAtomMempool)
    {
        std::map<uint160, uint32_t>::iterator it = mapAtomMempoolNonce.find(h160Best);
        if (it != mapAtomMempoolNonce.end() && it->second >= nNextNonce)
            nNextNonce = it->second + 1;
    }

    CAtomDexOffer offer;
    offer.nVersion    = ATOM_VERSION;
    offer.nType       = ATOM_TYPE_DEX_OFFER;
    offer.addrFrom    = h160Best;
    offer.nSide       = ATOM_DEX_SIDE_SELL_ATOM;
    offer.nAtomAmount = nAtomAmount;
    offer.nPrice      = nPrice;
    offer.nNonce      = nNextNonce;

    CScript scriptOpReturn = BuildDexOfferScript(offer);

    CScript scriptDest;
    if (!scriptDest.SetBitcoinAddress(strAddrBest))
        throw runtime_error("Failed to build destination script");

    CWalletTx wtx;
    CKey changeKey;
    int64 nFeeRequired = 0;

    if (!CreateStealthTransaction(scriptDest, scriptOpReturn, DUST_THRESHOLD, wtx, changeKey, nFeeRequired))
    {
        if (DUST_THRESHOLD + nFeeRequired > GetBalance())
            throw runtime_error(strprintf("Insufficient Bitok balance to pay fee. Need %s",
                                         FormatMoney(DUST_THRESHOLD + nFeeRequired).c_str()));
        throw runtime_error("Transaction creation failed");
    }

    if (!CommitTransaction(wtx, changeKey))
        throw runtime_error("Transaction broadcast failed");

    Object result;
    result.push_back(Pair("txid",        wtx.GetHash().ToString()));
    result.push_back(Pair("from",        strAddrBest));
    result.push_back(Pair("atom_amount", dAtomAmount));
    result.push_back(Pair("price",       dPrice));
    result.push_back(Pair("total_bitok", (double)nTotalBitok / (double)COIN));
    result.push_back(Pair("nonce",       (int64_t)nNextNonce));
    result.push_back(Pair("status",      string("pending")));
    return result;
}


Value dexcancel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dexcancel <offer_txid>\n"
            "Cancel an open sell order. You must be the maker.\n"
            "Your locked ATOM will be returned to your balance.");

    string strOfferTxid = params[0].get_str();
    uint256 hashOffer;
    hashOffer.SetHex(strOfferTxid);
    if (hashOffer == 0)
        throw runtime_error("Invalid offer txid");

    CDexOrderEntry order;
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(hashOffer);
        if (it == mapDexOrders.end())
            throw runtime_error("Offer not found in active order book: " + strOfferTxid);
        order = it->second;
    }

    bool fOwner = false;
    string strAddr;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
             it != mapKeys.end(); ++it)
        {
            if (Hash160(it->first) == order.addrMaker)
            {
                fOwner = true;
                strAddr = PubKeyToAddress(it->first);
                break;
            }
        }
    }

    if (!fOwner)
        throw runtime_error("You are not the maker of this order");

    CAtomDexCancel cancel;
    cancel.nVersion  = ATOM_VERSION;
    cancel.nType     = ATOM_TYPE_DEX_CANCEL;
    cancel.addrFrom  = order.addrMaker;
    cancel.hashOffer = hashOffer;

    CScript scriptOpReturn = BuildDexCancelScript(cancel);

    CScript scriptDest;
    if (!scriptDest.SetBitcoinAddress(strAddr))
        throw runtime_error("Failed to build destination script");

    CWalletTx wtx;
    CKey changeKey;
    int64 nFeeRequired = 0;

    if (!CreateStealthTransaction(scriptDest, scriptOpReturn, DUST_THRESHOLD, wtx, changeKey, nFeeRequired))
    {
        if (DUST_THRESHOLD + nFeeRequired > GetBalance())
            throw runtime_error(strprintf("Insufficient Bitok balance to pay fee. Need %s",
                                         FormatMoney(DUST_THRESHOLD + nFeeRequired).c_str()));
        throw runtime_error("Transaction creation failed");
    }

    if (!CommitTransaction(wtx, changeKey))
        throw runtime_error("Transaction broadcast failed");

    Object result;
    result.push_back(Pair("txid",       wtx.GetHash().ToString()));
    result.push_back(Pair("cancelled",  strOfferTxid));
    result.push_back(Pair("status",     string("pending")));
    return result;
}


Value dexbuy(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "dexbuy <atom_amount> [max_price]\n"
            "Buy ATOM from the cheapest sell order on the DEX.\n"
            "You pay BITOK, you receive ATOM. Settlement is atomic.\n"
            "  <atom_amount>  Min ATOM to buy (e.g. 100.5). Fills smallest order >= this amount.\n"
            "                 Use 0 to fill the cheapest order entirely.\n"
            "  [max_price]    Max price in BITOK per ATOM. Omit to take cheapest.");

    double dAtomWant = params[0].get_real();
    if (dAtomWant < 0.0)
        throw runtime_error("atom_amount must not be negative");
    int64 nAtomWant = roundint64(dAtomWant * (double)ATOM_DECIMALS);

    int64 nMaxPrice = 0;
    if (params.size() > 1)
    {
        double dMax = params[1].get_real();
        if (dMax <= 0.0)
            throw runtime_error("max_price must be positive");
        nMaxPrice = roundint64(dMax * (double)COIN);
    }

    uint256 hashBestOffer;
    CDexOrderEntry bestOrder;
    bool fFoundOrder = false;

    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.begin();
             it != mapDexOrders.end(); ++it)
        {
            const CDexOrderEntry& o = it->second;
            if (o.nSide != ATOM_DEX_SIDE_SELL_ATOM)
                continue;
            if (nMaxPrice > 0 && o.nPrice > nMaxPrice)
                continue;
            if (nAtomWant > 0 && o.nAtomAmount < nAtomWant)
                continue;
            if (setDexMempoolTakes.count(it->first))
                continue;
            if (setDexMempoolCancels.count(it->first))
                continue;
            if (!fFoundOrder || o.nPrice < bestOrder.nPrice ||
                (o.nPrice == bestOrder.nPrice && nAtomWant > 0 && o.nAtomAmount < bestOrder.nAtomAmount))
            {
                hashBestOffer = it->first;
                bestOrder = o;
                fFoundOrder = true;
            }
        }
    }

    if (!fFoundOrder)
    {
        if (nMaxPrice > 0)
            throw runtime_error(strprintf("No sell orders at or below %s BITOK/ATOM",
                                         FormatMoney(nMaxPrice).c_str()));
        if (nAtomWant > 0)
            throw runtime_error("No matching sell order found");
        throw runtime_error("No sell orders available on the DEX");
    }

    uint160 h160Taker;
    string strTakerAddr;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
             it != mapKeys.end(); ++it)
        {
            uint160 h = Hash160(it->first);
            if (h != bestOrder.addrMaker)
            {
                h160Taker = h;
                strTakerAddr = PubKeyToAddress(it->first);
                break;
            }
        }
    }

    if (h160Taker == 0)
        throw runtime_error("No suitable wallet address found");

    int64 nBitokPayment = DexComputeBitokPayment(bestOrder.nAtomAmount, bestOrder.nPrice);
    if (nBitokPayment + DUST_THRESHOLD > GetBalance())
        throw runtime_error(strprintf("Insufficient BITOK. Need %s + fee",
                                     FormatMoney(nBitokPayment).c_str()));

    CAtomDexTake take;
    take.nVersion  = ATOM_VERSION;
    take.nType     = ATOM_TYPE_DEX_TAKE;
    take.addrFrom  = h160Taker;
    take.hashOffer = hashBestOffer;

    CScript scriptOpReturn = BuildDexTakeScript(take);

    CScript scriptDest;
    string strMakerAddr = Hash160ToAddress(bestOrder.addrMaker);
    if (!scriptDest.SetBitcoinAddress(strMakerAddr))
        throw runtime_error("Failed to build maker destination script");

    CWalletTx wtx;
    CKey changeKey;
    int64 nFeeRequired = 0;

    if (!CreateStealthTransaction(scriptDest, scriptOpReturn, nBitokPayment, wtx, changeKey, nFeeRequired))
    {
        if (nBitokPayment + nFeeRequired > GetBalance())
            throw runtime_error(strprintf("Insufficient BITOK. Need %s",
                                         FormatMoney(nBitokPayment + nFeeRequired).c_str()));
        throw runtime_error("Transaction creation failed");
    }

    if (!CommitTransaction(wtx, changeKey))
        throw runtime_error("Transaction broadcast failed");

    Object result;
    result.push_back(Pair("txid",          wtx.GetHash().ToString()));
    result.push_back(Pair("offer_taken",   hashBestOffer.ToString()));
    result.push_back(Pair("taker",         strTakerAddr));
    result.push_back(Pair("atom_received", AtomToDouble(bestOrder.nAtomAmount)));
    result.push_back(Pair("bitok_paid",    (double)nBitokPayment / (double)COIN));
    result.push_back(Pair("price",         (double)bestOrder.nPrice / (double)COIN));
    result.push_back(Pair("status",        string("pending")));
    return result;
}

Value dextake(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dextake <offer_txid>\n"
            "Take (fill) a specific sell order by txid.\n"
            "You pay BITOK to the seller, receive their ATOM. Atomic settlement.");

    string strOfferTxid = params[0].get_str();
    uint256 hashOffer;
    hashOffer.SetHex(strOfferTxid);
    if (hashOffer == 0)
        throw runtime_error("Invalid offer txid");

    CDexOrderEntry order;
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.find(hashOffer);
        if (it == mapDexOrders.end())
            throw runtime_error("Order not found: " + strOfferTxid);
        order = it->second;
    }

    uint160 h160Taker;
    string strTakerAddr;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
             it != mapKeys.end(); ++it)
        {
            uint160 h = Hash160(it->first);
            if (h != order.addrMaker)
            {
                h160Taker = h;
                strTakerAddr = PubKeyToAddress(it->first);
                break;
            }
        }
    }

    if (h160Taker == 0)
        throw runtime_error("No suitable wallet address found");

    int64 nBitokPayment = DexComputeBitokPayment(order.nAtomAmount, order.nPrice);
    if (nBitokPayment + DUST_THRESHOLD > GetBalance())
        throw runtime_error(strprintf("Insufficient BITOK. Need %s + fee",
                                     FormatMoney(nBitokPayment).c_str()));

    CAtomDexTake take;
    take.nVersion  = ATOM_VERSION;
    take.nType     = ATOM_TYPE_DEX_TAKE;
    take.addrFrom  = h160Taker;
    take.hashOffer = hashOffer;

    CScript scriptOpReturn = BuildDexTakeScript(take);

    CScript scriptDest;
    string strMakerAddr = Hash160ToAddress(order.addrMaker);
    if (!scriptDest.SetBitcoinAddress(strMakerAddr))
        throw runtime_error("Failed to build maker destination script");

    CWalletTx wtx;
    CKey changeKey;
    int64 nFeeRequired = 0;

    if (!CreateStealthTransaction(scriptDest, scriptOpReturn, nBitokPayment, wtx, changeKey, nFeeRequired))
    {
        if (nBitokPayment + nFeeRequired > GetBalance())
            throw runtime_error(strprintf("Insufficient BITOK. Need %s",
                                         FormatMoney(nBitokPayment + nFeeRequired).c_str()));
        throw runtime_error("Transaction creation failed");
    }

    if (!CommitTransaction(wtx, changeKey))
        throw runtime_error("Transaction broadcast failed");

    Object result;
    result.push_back(Pair("txid",          wtx.GetHash().ToString()));
    result.push_back(Pair("offer_taken",   strOfferTxid));
    result.push_back(Pair("taker",         strTakerAddr));
    result.push_back(Pair("atom_received", AtomToDouble(order.nAtomAmount)));
    result.push_back(Pair("bitok_paid",    (double)nBitokPayment / (double)COIN));
    result.push_back(Pair("price",         (double)order.nPrice / (double)COIN));
    result.push_back(Pair("status",        string("pending")));
    return result;
}


Value dexorderbook(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "dexorderbook\n"
            "Returns all open sell orders, sorted by price ascending (cheapest first).");

    std::vector<std::pair<int64, Object> > vOrders;

    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.begin();
             it != mapDexOrders.end(); ++it)
        {
            const CDexOrderEntry& o = it->second;

            Object entry;
            entry.push_back(Pair("txid",        it->first.ToString()));
            entry.push_back(Pair("seller",      Hash160ToAddress(o.addrMaker)));
            entry.push_back(Pair("atom_amount", AtomToDouble(o.nAtomAmount)));
            entry.push_back(Pair("price",       (double)o.nPrice / (double)COIN));
            entry.push_back(Pair("total_bitok", (double)DexComputeBitokPayment(o.nAtomAmount, o.nPrice) / (double)COIN));
            entry.push_back(Pair("height",      o.nHeight));

            vOrders.push_back(std::make_pair(o.nPrice, entry));
        }
    }

    std::sort(vOrders.begin(), vOrders.end(),
              [](const std::pair<int64, Object>& a, const std::pair<int64, Object>& b){ return a.first < b.first; });

    Array orders;
    for (size_t i = 0; i < vOrders.size(); i++)
        orders.push_back(vOrders[i].second);

    Object result;
    result.push_back(Pair("orders",      orders));
    result.push_back(Pair("total_count", (int64_t)vOrders.size()));
    return result;
}


Value dexmyorders(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "dexmyorders\n"
            "Returns your open sell orders on the DEX.");

    std::set<uint160> setWallet;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        for (map<vector<unsigned char>, CPrivKey>::iterator it = mapKeys.begin();
             it != mapKeys.end(); ++it)
            setWallet.insert(Hash160(it->first));
    }

    Array ret;
    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.begin();
             it != mapDexOrders.end(); ++it)
        {
            const CDexOrderEntry& o = it->second;
            if (setWallet.count(o.addrMaker))
            {
                Object entry;
                entry.push_back(Pair("txid",        it->first.ToString()));
                entry.push_back(Pair("address",     Hash160ToAddress(o.addrMaker)));
                entry.push_back(Pair("atom_amount", AtomToDouble(o.nAtomAmount)));
                entry.push_back(Pair("price",       (double)o.nPrice / (double)COIN));
                entry.push_back(Pair("total_bitok", (double)DexComputeBitokPayment(o.nAtomAmount, o.nPrice) / (double)COIN));
                entry.push_back(Pair("height",      o.nHeight));
                ret.push_back(entry);
            }
        }
    }

    return ret;
}


Value dexinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "dexinfo\n"
            "Returns DEX statistics and parameters.");

    int64 nOrderCount = 0;
    int64 nTotalAtom = 0;
    int64 nTotalBitok = 0;
    int64 nBestAsk = 0;

    CRITICAL_BLOCK(cs_mapDexOrders)
    {
        for (std::map<uint256, CDexOrderEntry>::iterator it = mapDexOrders.begin();
             it != mapDexOrders.end(); ++it)
        {
            const CDexOrderEntry& o = it->second;
            nOrderCount++;
            nTotalAtom += o.nAtomAmount;
            nTotalBitok += DexComputeBitokPayment(o.nAtomAmount, o.nPrice);
            if (nBestAsk == 0 || o.nPrice < nBestAsk)
                nBestAsk = o.nPrice;
        }
    }

    Object result;
    result.push_back(Pair("activation_height", ATOM_DEX_ACTIVATION_HEIGHT));
    result.push_back(Pair("open_orders",       (int64_t)nOrderCount));
    result.push_back(Pair("total_atom",        AtomToDouble(nTotalAtom)));
    result.push_back(Pair("total_bitok_value", (double)nTotalBitok / (double)COIN));
    if (nBestAsk > 0)
        result.push_back(Pair("cheapest_ask",  (double)nBestAsk / (double)COIN));
    else
        result.push_back(Pair("cheapest_ask",  Value::null));
    {
        int64_t nPendOffers = 0, nPendCancels = 0, nPendTakes = 0;
        CRITICAL_BLOCK(cs_mapDexOrders)
        {
            nPendOffers  = (int64_t)mapDexMempoolOffers.size();
            nPendCancels = (int64_t)setDexMempoolCancels.size();
            nPendTakes   = (int64_t)setDexMempoolTakes.size();
        }
        result.push_back(Pair("pending_sells",     nPendOffers));
        result.push_back(Pair("pending_cancels",   nPendCancels));
        result.push_back(Pair("pending_buys",      nPendTakes));
    }
    return result;
}


//
// Call Table
//

pair<string, rpcfn_type> pCallTable[] =
{
    make_pair("help",                  &help),
    make_pair("stop",                  &stop),
    make_pair("getblockcount",         &getblockcount),
    make_pair("getblocknumber",        &getblocknumber),
    make_pair("getblockhash",          &getblockhash),
    make_pair("getbestblockhash",      &getbestblockhash),
    make_pair("getblock",              &getblock),
    make_pair("gettransaction",        &gettransaction),
    make_pair("getrawtransaction",     &getrawtransaction),
    make_pair("getrawmempool",         &getrawmempool),
    make_pair("gettxout",              &gettxout),
    make_pair("validateaddress",       &validateaddress),
    make_pair("getconnectioncount",    &getconnectioncount),
    make_pair("getpeerinfo",           &getpeerinfo),
    make_pair("getdifficulty",         &getdifficulty),
    make_pair("getbalance",            &getbalance),
    make_pair("getgenerate",           &getgenerate),
    make_pair("setgenerate",           &setgenerate),
    make_pair("getmininginfo",         &getmininginfo),
    make_pair("getblocktemplate",      &getblocktemplate),
    make_pair("submitblock",           &submitblock),
    make_pair("getwork",               &getwork),
    make_pair("getinfo",               &getinfo),
    make_pair("getnewaddress",         &getnewaddress),
    make_pair("setlabel",              &setlabel),
    make_pair("getlabel",              &getlabel),
    make_pair("getaddressesbylabel",   &getaddressesbylabel),
    make_pair("sendtoaddress",         &sendtoaddress),
    make_pair("listtransactions",      &listtransactions),
    make_pair("listunspent",           &listunspent),
    make_pair("getamountreceived",     &getreceivedbyaddress), // deprecated, renamed to getreceivedbyaddress
    make_pair("getallreceived",        &listreceivedbyaddress), // deprecated, renamed to listreceivedbyaddress
    make_pair("getreceivedbyaddress",  &getreceivedbyaddress),
    make_pair("getreceivedbylabel",    &getreceivedbylabel),
    make_pair("listreceivedbyaddress", &listreceivedbyaddress),
    make_pair("listreceivedbylabel",   &listreceivedbylabel),
    make_pair("getblockheader",        &getblockheader),
    make_pair("sendrawtransaction",    &sendrawtransaction),
    make_pair("gettxoutproof",         &gettxoutproof),
    make_pair("verifytxoutproof",      &verifytxoutproof),
    make_pair("dumpprivkey",           &dumpprivkey),
    make_pair("importprivkey",         &importprivkey),
    make_pair("rescanwallet",          &rescanwallet),
    make_pair("decoderawtransaction",  &decoderawtransaction),
    make_pair("decodescript",          &decodescript),
    make_pair("createrawtransaction",  &createrawtransaction),
    make_pair("signrawtransaction",    &signrawtransaction),
    make_pair("createmultisig",        &createmultisig),
    make_pair("addmultisigaddress",    &addmultisigaddress),
    make_pair("analyzescript",         &analyzescript),
    make_pair("validatescript",        &validatescript),
    make_pair("addpreimage",           &addpreimage),
    make_pair("listpreimages",         &listpreimages),
    make_pair("buildscript",           &buildscript),
    make_pair("setscriptsig",          &setscriptsig),
    make_pair("getscriptsighash",      &getscriptsighash),
    make_pair("decodescriptsig",       &decodescriptsig),
    make_pair("verifyscriptpair",      &verifyscriptpair),
    make_pair("getindexerinfo",        &getindexerinfo),

    make_pair("getaddresstxids",       &getaddresstxids),
    make_pair("getaddressutxos",       &getaddressutxos),
    make_pair("getaddressbalance",     &getaddressbalance),

    make_pair("getnewstealthaddress",  &getnewstealthaddress),
    make_pair("liststealthaddresses",  &liststealthaddresses),
    make_pair("decodestealthaddress",  &decodestealthaddress),

    make_pair("getatominfo",             &getatominfo),
    make_pair("gettotalatomsupply",      &gettotalatomsupply),
    make_pair("getatombalance",          &getatombalance),
    make_pair("getaddressatombalance",   &getaddressatombalance),
    make_pair("getnextatomnonce",        &getnextatomnonce),
    make_pair("getatomtx",               &getatomtx),
    make_pair("getatomhistory",          &getatomhistory),
    make_pair("listatomtransactions",    &listatomtransactions),
    make_pair("createatomrawtx",         &createatomrawtx),
    make_pair("sendatom",                &sendatom),
    make_pair("bridgeatomtosol",         &bridgeatomtosol),

    make_pair("dexsell",                 &dexsell),
    make_pair("dexbuy",                  &dexbuy),
    make_pair("dextake",                 &dextake),
    make_pair("dexcancel",               &dexcancel),
    make_pair("dexorderbook",            &dexorderbook),
    make_pair("dexmyorders",             &dexmyorders),
    make_pair("dexinfo",                 &dexinfo),
};
map<string, rpcfn_type> mapCallTable(pCallTable, pCallTable + sizeof(pCallTable)/sizeof(pCallTable[0]));




//
// HTTP protocol
//
// This ain't Apache.  We're just using HTTP header for the length field
// and to be compatible with other JSON-RPC implementations.
//

string EncodeBase64(const string& str);

string HTTPPost(const string& strMsg)
{
    string strUserPass = GetArg("-rpcuser", "") + ":" + GetArg("-rpcpassword", "");
    string strAuth;
    if (strUserPass != ":")
        strAuth = strprintf("Authorization: Basic %s\r\n", EncodeBase64(strUserPass).c_str());

    return strprintf(
            "POST / HTTP/1.1\r\n"
            "User-Agent: json-rpc/1.0\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Accept: application/json\r\n"
            "%s"
            "\r\n"
            "%s",
        strMsg.size(),
        strAuth.c_str(),
        strMsg.c_str());
}

string HTTPReply(const string& strMsg, int nStatus=200)
{
    string strStatus;
    if (nStatus == 200) strStatus = "OK";
    if (nStatus == 204) strStatus = "No Content";
    if (nStatus == 401) strStatus = "Unauthorized";
    if (nStatus == 403) strStatus = "Forbidden";
    if (nStatus == 500) strStatus = "Internal Server Error";

    string strHeaders = strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Connection: close\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: application/json\r\n"
            "Date: Sat, 08 Jul 2006 12:04:08 GMT\r\n"
            "Server: json-rpc/1.0\r\n",
        nStatus,
        strStatus.c_str(),
        strMsg.size());

    if (nStatus == 401)
        strHeaders += "WWW-Authenticate: Basic realm=\"jsonrpc\"\r\n";

    if (fCORS)
    {
        string strCORSOrigin = GetArg("-corsorigin", "*");
        strHeaders += "Access-Control-Allow-Origin: " + strCORSOrigin + "\r\n";
        strHeaders += "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
        strHeaders += "Access-Control-Allow-Headers: Authorization, Content-Type\r\n";
    }

    return strHeaders + "\r\n" + strMsg;
}

string EncodeBase64(const string& str)
{
    const char* pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string result;
    int val = 0;
    int valb = -6;
    for (unsigned char c : str)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            result.push_back(pbase64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        result.push_back(pbase64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4)
        result.push_back('=');
    return result;
}

string DecodeBase64(const string& str)
{
    const char* pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string result;
    int val = 0;
    int valb = -8;
    for (unsigned char c : str)
    {
        if (c == '=') break;
        const char* p = strchr(pbase64, c);
        if (p == NULL) continue;
        val = (val << 6) + (p - pbase64);
        valb += 6;
        if (valb >= 0)
        {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

bool HTTPAuthorized(const string& strAuth)
{
    string strRPCUserColonPass = GetArg("-rpcuser", "") + ":" + GetArg("-rpcpassword", "");
    if (strRPCUserColonPass == ":")
        return true;

    if (strAuth.substr(0, 6) != "Basic ")
        return false;

    string strUserPass64 = strAuth.substr(6);
    string strUserPass = DecodeBase64(strUserPass64);
    return strUserPass == strRPCUserColonPass;
}

bool ClientAllowed(const string& strAddr)
{
    if (!mapMultiArgs.count("-rpcallowip"))
        return strAddr == "127.0.0.1";

    foreach(string strAllow, mapMultiArgs["-rpcallowip"])
    {
        if (strAllow == strAddr)
            return true;

        size_t pos = strAllow.find('/');
        if (pos != string::npos)
        {
            unsigned int mask = 32;
            if (pos < strAllow.length() - 1)
                mask = atoi(strAllow.substr(pos + 1).c_str());

            string strNet = strAllow.substr(0, pos);
            unsigned int ip1[4], ip2[4];
            if (sscanf(strAddr.c_str(), "%u.%u.%u.%u", &ip1[0], &ip1[1], &ip1[2], &ip1[3]) == 4 &&
                sscanf(strNet.c_str(), "%u.%u.%u.%u", &ip2[0], &ip2[1], &ip2[2], &ip2[3]) == 4)
            {
                unsigned int nIP1 = (ip1[0] << 24) | (ip1[1] << 16) | (ip1[2] << 8) | ip1[3];
                unsigned int nIP2 = (ip2[0] << 24) | (ip2[1] << 16) | (ip2[2] << 8) | ip2[3];
                unsigned int nMask = 0xFFFFFFFF << (32 - mask);

                if ((nIP1 & nMask) == (nIP2 & nMask))
                    return true;
            }
        }
    }
    return false;
}

int ReadHTTPHeader(tcp::iostream& stream, map<string, string>& mapHeadersRet)
{
    int nLen = 0;
    bool fFirstLine = true;
    loop
    {
        string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
        if (fFirstLine)
        {
            fFirstLine = false;
            string::size_type nSpace = str.find(' ');
            if (nSpace != string::npos)
                mapHeadersRet["_method"] = str.substr(0, nSpace);
            continue;
        }
        if (str.substr(0,15) == "Content-Length:")
            nLen = atoi(str.substr(15));
        else
        {
            string::size_type nColon = str.find(":");
            if (nColon != string::npos)
            {
                string strHeader = str.substr(0, nColon);
                string strValue = str.substr(nColon + 1);
                while (!strValue.empty() && isspace(strValue[0]))
                    strValue = strValue.substr(1);
                while (!strValue.empty() && (isspace(strValue[strValue.size()-1]) || strValue[strValue.size()-1] == '\r'))
                    strValue = strValue.substr(0, strValue.size()-1);
                mapHeadersRet[strHeader] = strValue;
            }
        }
    }
    return nLen;
}

inline string ReadHTTP(tcp::iostream& stream, map<string, string>& mapHeadersRet)
{
    mapHeadersRet.clear();
    int nLen = ReadHTTPHeader(stream, mapHeadersRet);
    if (nLen <= 0)
        return string();

    vector<char> vch(nLen);
    stream.read(&vch[0], nLen);
    return string(vch.begin(), vch.end());
}



//
// JSON-RPC protocol
//
// http://json-rpc.org/wiki/specification
// http://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
//

string JSONRPCRequest(const string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("params", params));
    request.push_back(Pair("id", id));
    return write_string(Value(request), false) + "\n";
}

string JSONRPCReply(const Value& result, const Value& error, const Value& id)
{
    Object reply;
    if (error.type() != null_type)
        reply.push_back(Pair("result", Value::null));
    else
        reply.push_back(Pair("result", result));
    reply.push_back(Pair("error", error));
    reply.push_back(Pair("id", id));
    return write_string(Value(reply), false) + "\n";
}




void ThreadRPCServer(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadRPCServer(parg));
    try
    {
        vnThreadsRunning[4]++;
        ThreadRPCServer2(parg);
        vnThreadsRunning[4]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[4]--;
        PrintException(&e, "ThreadRPCServer()");
    } catch (...) {
        vnThreadsRunning[4]--;
        PrintException(NULL, "ThreadRPCServer()");
    }
    printf("ThreadRPCServer exiting\n");
}

void ThreadRPCServer2(void* parg)
{
    printf("ThreadRPCServer started\n");

    int nRPCPort = GetIntArg("-rpcport", 8332);
    string strRPCBind = GetArg("-rpcbind", "127.0.0.1");

    boost::asio::ip::address bindAddress;
    try
    {
#if BOOST_VERSION >= 106600
        bindAddress = boost::asio::ip::make_address(strRPCBind);
#else
        bindAddress = boost::asio::ip::address::from_string(strRPCBind);
#endif
    }
    catch (...)
    {
        printf("Invalid -rpcbind address: %s, using 127.0.0.1\n", strRPCBind.c_str());
        bindAddress = boost::asio::ip::address_v4::loopback();
    }

    printf("RPC server binding to %s:%d\n", bindAddress.to_string().c_str(), nRPCPort);

#if BOOST_VERSION >= 106600
    boost::asio::io_context io_service;
#else
    boost::asio::io_service io_service;
#endif
    tcp::endpoint endpoint(bindAddress, nRPCPort);
    tcp::acceptor acceptor(io_service, endpoint);

    loop
    {
        tcp::iostream stream;
        tcp::endpoint peer;
        vnThreadsRunning[4]--;
#if BOOST_VERSION >= 106600
        acceptor.accept(stream.rdbuf()->socket(), peer);
#else
        acceptor.accept(*stream.rdbuf(), peer);
#endif
        vnThreadsRunning[4]++;
        if (fShutdown)
            return;

        string strPeerAddr = peer.address().to_string();

        if (!ClientAllowed(strPeerAddr))
        {
            printf("RPC connection from %s denied\n", strPeerAddr.c_str());
            stream << HTTPReply("Forbidden", 403) << std::flush;
            continue;
        }

        map<string, string> mapHeaders;
        string strRequest = ReadHTTP(stream, mapHeaders);

        if (fCORS && mapHeaders["_method"] == "OPTIONS")
        {
            stream << HTTPReply("", 204) << std::flush;
            continue;
        }

        if (!HTTPAuthorized(mapHeaders["Authorization"]))
        {
            printf("RPC authorization failed from %s\n", strPeerAddr.c_str());
            stream << HTTPReply("Unauthorized", 401) << std::flush;
            continue;
        }

        if (fDebug)
            printf("[RPC] Request from %s\n", strPeerAddr.c_str());

        // Handle multiple invocations per request
        string::iterator begin = strRequest.begin();
        while (skipspaces(begin), begin != strRequest.end())
        {
            string::iterator prev = begin;
            Value id;
            try
            {
                // Parse request
                Value valRequest;
                if (!read_range(begin, strRequest.end(), valRequest))
                    throw runtime_error("Parse error.");
                const Object& request = valRequest.get_obj();
                if (find_value(request, "method").type() != str_type ||
                    find_value(request, "params").type() != array_type)
                    throw runtime_error("Invalid request.");

                string strMethod    = find_value(request, "method").get_str();
                const Array& params = find_value(request, "params").get_array();
                id                  = find_value(request, "id");

                // Execute
                map<string, rpcfn_type>::iterator mi = mapCallTable.find(strMethod);
                if (mi == mapCallTable.end())
                    throw runtime_error("Method not found.");

                int64 nStartTime = GetTimeMillis();
                Value result = (*(*mi).second)(params, false);
                int64 nDuration = GetTimeMillis() - nStartTime;

                if (fDebug)
                    printf("[RPC] %s (%d params) completed in %lld ms\n", strMethod.c_str(), (int)params.size(), (long long)nDuration);

                // Send reply
                string strReply = JSONRPCReply(result, Value::null, id);
                stream << HTTPReply(strReply, 200) << std::flush;
            }
            catch (std::exception& e)
            {
                // Send error reply
                string strReply = JSONRPCReply(Value::null, e.what(), id);
                stream << HTTPReply(strReply, 500) << std::flush;
            }
            if (begin == prev)
                break;
        }
    }
}




Value CallRPC(const string& strMethod, const Array& params)
{
    int nRPCPort = GetIntArg("-rpcport", 8332);
    string strRPCHost = GetArg("-rpcconnect", "127.0.0.1");

    tcp::iostream stream(strRPCHost, itostr(nRPCPort));
    if (stream.fail())
        throw runtime_error("couldn't connect to server");

    string strRequest = JSONRPCRequest(strMethod, params, 1);
    stream << HTTPPost(strRequest) << std::flush;

    map<string, string> mapHeaders;
    string strReply = ReadHTTP(stream, mapHeaders);
    if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    const Value& result = find_value(reply, "result");
    const Value& error  = find_value(reply, "error");
    const Value& id     = find_value(reply, "id");

    if (error.type() == str_type)
        throw runtime_error(error.get_str());
    else if (error.type() != null_type)
        throw runtime_error(write_string(error, false));
    return result;
}




template<typename T>
void ConvertTo(Value& value)
{
    if (value.type() == str_type)
    {
        // reinterpret string as unquoted json value
        Value value2;
        if (!read_string(value.get_str(), value2))
            throw runtime_error("type mismatch");
        value = value2.get_value<T>();
    }
    else
    {
        value = value.get_value<T>();
    }
}

int CommandLineRPC(int argc, char *argv[])
{
    try
    {
        // Check that method exists
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strMethod = argv[1];
        if (!mapCallTable.count(strMethod))
            throw runtime_error(strprintf("unknown command: %s", strMethod.c_str()));

        Value result;
        if (argc == 3 && (strcmp(argv[2], "-?") == 0 || strcmp(argv[2], "--help") == 0))
        {
            // Call help locally, help text is returned in an exception
            try
            {
                map<string, rpcfn_type>::iterator mi = mapCallTable.find(strMethod);
                Array params;
                (*(*mi).second)(params, true);
            }
            catch (std::exception& e)
            {
                result = e.what();
            }
        }
        else
        {
            // Parameters default to strings
            Array params;
            for (int i = 2; i < argc; i++)
                params.push_back(argv[i]);
            int n = params.size();

            //
            // Special case non-string parameter types
            //
            if (strMethod == "getblockhash"           && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "getrawtransaction"      && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "gettxout"               && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "gettxout"               && n > 2) ConvertTo<bool>(params[2]);
            if (strMethod == "setgenerate"            && n > 0) ConvertTo<bool>(params[0]);
            if (strMethod == "setgenerate"            && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "sendtoaddress"          && n > 1) ConvertTo<double>(params[1]);
            if (strMethod == "listtransactions"       && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "listtransactions"       && n > 1) ConvertTo<bool>(params[1]);
            if (strMethod == "listunspent"            && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "listunspent"            && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "getamountreceived"      && n > 1) ConvertTo<boost::int64_t>(params[1]); // deprecated
            if (strMethod == "getreceivedbyaddress"   && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "getreceivedbylabel"     && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "getallreceived"         && n > 0) ConvertTo<boost::int64_t>(params[0]); // deprecated
            if (strMethod == "getallreceived"         && n > 1) ConvertTo<bool>(params[1]);
            if (strMethod == "listreceivedbyaddress"  && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "listreceivedbyaddress"  && n > 1) ConvertTo<bool>(params[1]);
            if (strMethod == "listreceivedbylabel"    && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "listreceivedbylabel"    && n > 1) ConvertTo<bool>(params[1]);
            if (strMethod == "createrawtransaction"   && n > 0) ConvertTo<Array>(params[0]);
            if (strMethod == "createrawtransaction"   && n > 1) ConvertTo<Object>(params[1]);
            if (strMethod == "createrawtransaction"   && n > 2) ConvertTo<boost::int64_t>(params[2]);
            if (strMethod == "signrawtransaction"     && n > 1) ConvertTo<Array>(params[1]);
            if (strMethod == "signrawtransaction"     && n > 2) ConvertTo<Array>(params[2]);
            if (strMethod == "createmultisig"          && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "createmultisig"          && n > 1) ConvertTo<Array>(params[1]);
            if (strMethod == "addmultisigaddress"      && n > 0) ConvertTo<boost::int64_t>(params[0]);
            if (strMethod == "addmultisigaddress"      && n > 1) ConvertTo<Array>(params[1]);
            if (strMethod == "validatescript"           && n > 1) ConvertTo<Array>(params[1]);
            if (strMethod == "buildscript"              && n > 0) ConvertTo<Array>(params[0]);
            if (strMethod == "setscriptsig"             && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "setscriptsig"             && n > 2)
            {
                try { ConvertTo<Array>(params[2]); } catch (...) { }
            }
            if (strMethod == "getscriptsighash"         && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "verifyscriptpair"         && n > 1) ConvertTo<boost::int64_t>(params[1]);
            if (strMethod == "importprivkey"            && n > 2) ConvertTo<bool>(params[2]);

            // Execute
            result = CallRPC(strMethod, params);
        }

        // Print result
        string strResult = (result.type() == str_type ? result.get_str() : write_string(result, true));
        if (result.type() != null_type)
        {
            if (fWindows && fGUI)
                // Windows GUI apps can't print to command line,
                // so settle for a message box yuck
                MyMessageBox(strResult.c_str(), "Bitok", wxOK);
            else
                fprintf(stdout, "%s\n", strResult.c_str());
        }
        return 0;
    }
    catch (std::exception& e) {
        if (fWindows && fGUI)
            MyMessageBox(strprintf("error: %s\n", e.what()).c_str(), "Bitok", wxOK);
        else
            fprintf(stderr, "error: %s\n", e.what());
    } catch (...) {
        PrintException(NULL, "CommandLineRPC()");
    }
    return 1;
}




#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
    // Turn off microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFile("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    try
    {
        if (argc >= 2 && string(argv[1]) == "-server")
        {
            printf("server ready\n");
            ThreadRPCServer(NULL);
        }
        else
        {
            return CommandLineRPC(argc, argv);
        }
    }
    catch (std::exception& e) {
        PrintException(&e, "main()");
    } catch (...) {
        PrintException(NULL, "main()");
    }
    return 0;
}
#endif
