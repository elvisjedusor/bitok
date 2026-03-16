// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

class CTransaction;
class CTxIndex;
class CDiskBlockIndex;
class CDiskTxPos;
class COutPoint;
class CUser;
class CReview;
class CAddress;
class CWalletTx;

extern map<string, string> mapAddressBook;
extern CCriticalSection cs_mapAddressBook;
extern vector<unsigned char> vchDefaultKey;
extern bool fClient;



extern unsigned int nWalletDBUpdated;
extern DbEnv dbenv;


extern void DBFlush(bool fShutdown);
extern bool RecoverDatabaseEnvironment();
extern bool RecoverWalletKeys();

extern bool fUseIndexer;
extern bool fIndexerRebuilding;




class CDB
{
protected:
    Db* pdb;
    string strFile;
    vector<DbTxn*> vTxn;
    bool fReadOnly;

    explicit CDB(const char* pszFile, const char* pszMode="r+");
    ~CDB() { Close(); }
public:
    void Close();
private:
    CDB(const CDB&);
    void operator=(const CDB&);

protected:
    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Read
        Dbt datValue;
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pdb->get(GetTxn(), &datKey, &datValue, 0);
        memset(datKey.get_data(), 0, datKey.get_size());
        if (datValue.get_data() == NULL)
            return false;

        // Unserialize value
        CDataStream ssValue((char*)datValue.get_data(), (char*)datValue.get_data() + datValue.get_size(), SER_DISK);
        ssValue >> value;

        // Clear and free memory
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datValue.get_data());
        return (ret == 0);
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite=true)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(("Write called on database in read-only mode", false));

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Value
        CDataStream ssValue(SER_DISK);
        ssValue.reserve(10000);
        ssValue << value;
        Dbt datValue(&ssValue[0], ssValue.size());

        // Write
        int ret = pdb->put(GetTxn(), &datKey, &datValue, (fOverwrite ? 0 : DB_NOOVERWRITE));

        // Clear memory in case it was a private key
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        return (ret == 0);
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(("Erase called on database in read-only mode", false));

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Erase
        int ret = pdb->del(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0 || ret == DB_NOTFOUND);
    }

    template<typename K>
    bool Exists(const K& key)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Exists
        int ret = pdb->exists(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0);
    }

    Dbc* GetCursor()
    {
        if (!pdb)
            return NULL;
        Dbc* pcursor = NULL;
        int ret = pdb->cursor(NULL, &pcursor, 0);
        if (ret != 0)
            return NULL;
        return pcursor;
    }

    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags=DB_NEXT)
    {
        // Read at cursor
        Dbt datKey;
        if (fFlags == DB_SET || fFlags == DB_SET_RANGE || fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datKey.set_data(&ssKey[0]);
            datKey.set_size(ssKey.size());
        }
        Dbt datValue;
        if (fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datValue.set_data(&ssValue[0]);
            datValue.set_size(ssValue.size());
        }
        datKey.set_flags(DB_DBT_MALLOC);
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pcursor->get(&datKey, &datValue, fFlags);
        if (ret != 0)
            return ret;
        else if (datKey.get_data() == NULL || datValue.get_data() == NULL)
            return 99999;

        // Convert to streams
        ssKey.SetType(SER_DISK);
        ssKey.clear();
        ssKey.write((char*)datKey.get_data(), datKey.get_size());
        ssValue.SetType(SER_DISK);
        ssValue.clear();
        ssValue.write((char*)datValue.get_data(), datValue.get_size());

        // Clear and free memory
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datKey.get_data());
        free(datValue.get_data());
        return 0;
    }

    DbTxn* GetTxn()
    {
        if (!vTxn.empty())
            return vTxn.back();
        else
            return NULL;
    }

public:
    bool TxnBegin()
    {
        if (!pdb)
            return false;
        DbTxn* ptxn = NULL;
        int ret = dbenv.txn_begin(GetTxn(), &ptxn, 0);
        if (!ptxn || ret != 0)
            return false;
        vTxn.push_back(ptxn);
        return true;
    }

    bool TxnCommit()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->commit(0);
        vTxn.pop_back();
        return (ret == 0);
    }

    bool TxnAbort()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->abort();
        vTxn.pop_back();
        return (ret == 0);
    }

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(string("version"), nVersion);
    }
};








class CTxDB : public CDB
{
public:
    CTxDB(const char* pszMode="r+") : CDB(!fClient ? "blkindex.dat" : NULL, pszMode) { }
private:
    CTxDB(const CTxDB&);
    void operator=(const CTxDB&);
public:
    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadOwnerTxes(uint160 hash160, int nHeight, vector<CTransaction>& vtx);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool LoadBlockIndex();

    bool WriteIndexerHeight(int nHeight);
    bool ReadIndexerHeight(int& nHeight);

    bool WriteAddrTx(uint160 addr, uint256 txhash, int nHeight);
    bool EraseAddrTx(uint160 addr, uint256 txhash);
    bool ReadAddrTxids(uint160 addr, vector<uint256>& vtxids);

    bool WriteAddrOut(uint160 addr, uint256 txhash, unsigned int n, int64 nValue, int nHeight, bool fCoinBase);
    bool EraseAddrOut(uint160 addr, uint256 txhash, unsigned int n);
    bool ReadAddrUTXOs(uint160 addr, vector<pair<COutPoint, pair<int64, pair<int, bool> > > >& vUTXOs);
    bool ReadAddrOut(uint160 addr, uint256 txhash, unsigned int n, int64& nValue, int& nHeight);
    bool EraseAllIndexerData();

    // ATOM layer
    bool WriteAtomBalance(uint160 addr, int64 nBalance, uint32_t nNonce);
    bool ReadAtomBalance(uint160 addr, int64& nBalance, uint32_t& nNonce);
    bool ReadAtomBalance(uint160 addr, int64& nBalance, uint32_t& nNonce, bool& fDbError);
    bool EraseAtomBalance(uint160 addr);
    bool SumAllAtomBalances(int64& nTotalOut);

    bool WriteAtomTx(uint256 txhash, int nHeight, unsigned int nTime, unsigned char nType,
                     uint160 addrFrom, uint160 addrTo, int64 nAmount, uint32_t nNonce,
                     const unsigned char* pSolAddrTo = NULL);
    bool ReadAtomTx(uint256 txhash, int& nHeight, unsigned int& nTime, unsigned char& nType,
                    uint160& addrFrom, uint160& addrTo, int64& nAmount, uint32_t& nNonce,
                    unsigned char* pSolAddrTo = NULL);
    bool EraseAtomTx(uint256 txhash);

    bool ReadAtomAddrTxids(uint160 addr, vector<uint256>& vtxids);
    bool WriteAtomAddrTx(uint160 addr, uint256 txhash, int nHeight);
    bool EraseAtomAddrTx(uint160 addr, uint256 txhash);

    bool EraseAllAtomData();

    bool WriteAtomTotalMinted(int64 nTotal);
    bool ReadAtomTotalMinted(int64& nTotal);
};





class CAddrDB : public CDB
{
public:
    CAddrDB(const char* pszMode="cr+") : CDB("addr.dat", pszMode) { }
private:
    CAddrDB(const CAddrDB&);
    void operator=(const CAddrDB&);
public:
    bool WriteAddress(const CAddress& addr);
    bool EraseAddress(const CAddress& addr);
    bool LoadAddresses();
};

bool LoadAddresses();






class CWalletDB : public CDB
{
public:
    CWalletDB(const char* pszMode="r+") : CDB("wallet.dat", pszMode) { }
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
public:
    bool ReadName(const string& strAddress, string& strName)
    {
        strName = "";
        return Read(make_pair(string("name"), strAddress), strName);
    }

    bool WriteName(const string& strAddress, const string& strName)
    {
        CRITICAL_BLOCK(cs_mapAddressBook)
            mapAddressBook[strAddress] = strName;
        nWalletDBUpdated++;
        return Write(make_pair(string("name"), strAddress), strName);
    }

    bool EraseName(const string& strAddress)
    {
        // This should only be used for sending addresses, never for receiving addresses,
        // receiving addresses must always have an address book entry if they're not change return.
        CRITICAL_BLOCK(cs_mapAddressBook)
            mapAddressBook.erase(strAddress);
        nWalletDBUpdated++;
        return Erase(make_pair(string("name"), strAddress));
    }

    bool ReadTx(uint256 hash, CWalletTx& wtx)
    {
        return Read(make_pair(string("tx"), hash), wtx);
    }

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        nWalletDBUpdated++;
        return Erase(make_pair(string("tx"), hash));
    }

    bool ReadKey(const vector<unsigned char>& vchPubKey, CPrivKey& vchPrivKey)
    {
        vchPrivKey.clear();
        return Read(make_pair(string("key"), vchPubKey), vchPrivKey);
    }

    bool WriteKey(const vector<unsigned char>& vchPubKey, const CPrivKey& vchPrivKey)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("key"), vchPubKey), vchPrivKey, false);
    }

    bool ReadDefaultKey(vector<unsigned char>& vchPubKey)
    {
        vchPubKey.clear();
        return Read(string("defaultkey"), vchPubKey);
    }

    bool WriteDefaultKey(const vector<unsigned char>& vchPubKey)
    {
        vchDefaultKey = vchPubKey;
        nWalletDBUpdated++;
        return Write(string("defaultkey"), vchPubKey);
    }

    template<typename T>
    bool ReadSetting(const string& strKey, T& value)
    {
        return Read(make_pair(string("setting"), strKey), value);
    }

    template<typename T>
    bool WriteSetting(const string& strKey, const T& value)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("setting"), strKey), value);
    }

    bool WriteHashPreimage(const vector<unsigned char>& vchHash, const vector<unsigned char>& vchPreimage)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("preimage"), vchHash), vchPreimage);
    }

    bool WriteStealthAddress(const CStealthAddress& sxAddr)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("sxaddr"), sxAddr.scan_pubkey), sxAddr);
    }

    bool WriteStealthScanKey(const vector<unsigned char>& vchScanPub, const CPrivKey& vchScanPriv)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("sxscan"), vchScanPub), vchScanPriv, false);
    }

    bool WriteStealthSpendKey(const vector<unsigned char>& vchSpendPub, const CPrivKey& vchSpendPriv)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("sxspend"), vchSpendPub), vchSpendPriv, false);
    }

    bool ReadStealthScanKey(const vector<unsigned char>& vchScanPub, CPrivKey& vchScanPriv)
    {
        return Read(make_pair(string("sxscan"), vchScanPub), vchScanPriv);
    }

    bool ReadStealthSpendKey(const vector<unsigned char>& vchSpendPub, CPrivKey& vchSpendPriv)
    {
        return Read(make_pair(string("sxspend"), vchSpendPub), vchSpendPriv);
    }

    bool WriteStealthChangeIndex(const vector<unsigned char>& vchSpendPub, uint32_t nIndex)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("sxchgidx"), vchSpendPub), nIndex);
    }

    bool ReadStealthChangeIndex(const vector<unsigned char>& vchSpendPub, uint32_t& nIndex)
    {
        return Read(make_pair(string("sxchgidx"), vchSpendPub), nIndex);
    }

    bool WriteStealthDestMap(const vector<unsigned char>& vchDestPub,
                             const pair<vector<unsigned char>, vector<unsigned char> >& scanAndEphem)
    {
        nWalletDBUpdated++;
        return Write(make_pair(string("sxdest"), vchDestPub), scanAndEphem);
    }

    bool LoadWallet();
};

bool LoadWallet(bool& fFirstRunRet);

inline bool SetAddressBookName(const string& strAddress, const string& strName)
{
    return CWalletDB().WriteName(strAddress, strName);
}
