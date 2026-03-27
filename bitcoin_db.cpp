// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"

void ThreadFlushWalletDB(void* parg);

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
static string GetBDBPath(const string& path)
{
    string result = path;
    for (size_t i = 0; i < result.size(); i++)
        if (result[i] == '/')
            result[i] = '\\';
    return result;
}
#else
static string GetBDBPath(const string& path)
{
    return path;
}
#endif


unsigned int nWalletDBUpdated;


static void CleanupLogFiles(const string& strDataDir, const string& strLogDir)
{
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    WIN32_FIND_DATAA findData;
    string strPattern = strLogDir + "\\log.*";
    HANDLE hFind = FindFirstFileA(strPattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            string strFile = strLogDir + "\\" + findData.cFileName;
            printf("  Removing: %s\n", strFile.c_str());
            DeleteFileA(strFile.c_str());
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    strPattern = strLogDir + "\\__db.*";
    hFind = FindFirstFileA(strPattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            string strFile = strLogDir + "\\" + findData.cFileName;
            printf("  Removing: %s\n", strFile.c_str());
            DeleteFileA(strFile.c_str());
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    strPattern = strDataDir + "\\__db.*";
    hFind = FindFirstFileA(strPattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            string strFile = strDataDir + "\\" + findData.cFileName;
            printf("  Removing: %s\n", strFile.c_str());
            DeleteFileA(strFile.c_str());
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(strLogDir.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            string strName = entry->d_name;
            if (strName.substr(0, 4) == "log." || strName.substr(0, 5) == "__db.")
            {
                string strFile = strLogDir + "/" + strName;
                printf("  Removing: %s\n", strFile.c_str());
                unlink(strFile.c_str());
            }
        }
        closedir(dir);
    }

    dir = opendir(strDataDir.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            string strName = entry->d_name;
            if (strName.substr(0, 5) == "__db.")
            {
                string strFile = strDataDir + "/" + strName;
                printf("  Removing: %s\n", strFile.c_str());
                unlink(strFile.c_str());
            }
        }
        closedir(dir);
    }
#endif
}

static void RemoveIncompatibleDatabases(const string& strDataDir)
{
    printf("Recovery: Removing incompatible database files (wallet.dat preserved)...\n");

    string strAddrFile = strDataDir + "/addr.dat";
    string strBlkIndexFile = strDataDir + "/blkindex.dat";

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    for (size_t i = 0; i < strAddrFile.size(); i++)
        if (strAddrFile[i] == '/') strAddrFile[i] = '\\';
    for (size_t i = 0; i < strBlkIndexFile.size(); i++)
        if (strBlkIndexFile[i] == '/') strBlkIndexFile[i] = '\\';

    if (DeleteFileA(strAddrFile.c_str()))
        printf("  Removed: %s\n", strAddrFile.c_str());

    if (DeleteFileA(strBlkIndexFile.c_str()))
        printf("  Removed: %s\n", strBlkIndexFile.c_str());
#else
    if (unlink(strAddrFile.c_str()) == 0)
        printf("  Removed: %s\n", strAddrFile.c_str());

    if (unlink(strBlkIndexFile.c_str()) == 0)
        printf("  Removed: %s\n", strBlkIndexFile.c_str());
#endif
}

static bool TryRecoverEnvironment(const string& strDataDir, const string& strLogDir)
{
    DbEnv dbenvRecover(0);

    dbenvRecover.set_lg_dir(strLogDir.c_str());
    dbenvRecover.set_lg_max(10000000);
    dbenvRecover.set_lk_max_locks(10000);
    dbenvRecover.set_lk_max_objects(10000);
    dbenvRecover.set_flags(DB_AUTO_COMMIT, 1);

    int ret = dbenvRecover.open(strDataDir.c_str(),
                     DB_CREATE     |
                     DB_INIT_LOCK  |
                     DB_INIT_LOG   |
                     DB_INIT_MPOOL |
                     DB_INIT_TXN   |
                     DB_RECOVER    |
                     DB_PRIVATE,
                     0);

    if (ret != 0)
        return false;

    dbenvRecover.close(0);
    return true;
}

bool RecoverDatabaseEnvironment()
{
    string strDataDir = GetDataDir();
    string strLogDir = strDataDir + "/database";

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    strDataDir = GetBDBPath(strDataDir);
    strLogDir = GetBDBPath(strLogDir);
#endif

    printf("Recovery: Cleaning database environment in %s\n", strDataDir.c_str());
    CleanupLogFiles(strDataDir, strLogDir);

    RemoveIncompatibleDatabases(strDataDir);

    printf("Recovery: Attempting environment recovery...\n");
    if (TryRecoverEnvironment(strDataDir, strLogDir))
    {
        printf("Recovery: Success!\n");
        printf("Note: Address database and block index will be rebuilt.\n");
        return true;
    }

    printf("Recovery: Environment recovery failed, cleaning up...\n");
    CleanupLogFiles(strDataDir, strLogDir);

    if (TryRecoverEnvironment(strDataDir, strLogDir))
    {
        printf("Recovery: Success after cleanup!\n");
        printf("Note: Address database and block index will be rebuilt.\n");
        return true;
    }

    printf("Recovery: Failed. Manual intervention required.\n");
    printf("Try deleting all files in %s except wallet.dat\n", strDataDir.c_str());
    return false;
}

bool RecoverWalletKeys()
{
    string strDataDir = GetDataDir();
    string strWalletFile = strDataDir + "/wallet.dat";
    string strOldWalletFile = strDataDir + "/wallet_old.dat";

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    for (size_t i = 0; i < strWalletFile.size(); i++)
        if (strWalletFile[i] == '/') strWalletFile[i] = '\\';
    for (size_t i = 0; i < strOldWalletFile.size(); i++)
        if (strOldWalletFile[i] == '/') strOldWalletFile[i] = '\\';
#endif

    FILE* file = fopen(strWalletFile.c_str(), "rb");
    if (!file)
    {
        printf("Wallet recovery: wallet.dat not found, nothing to recover\n");
        return true;
    }
    fclose(file);

    file = fopen(strOldWalletFile.c_str(), "rb");
    if (file)
    {
        fclose(file);
        printf("Wallet recovery: wallet_old.dat already exists\n");
        printf("Please remove or rename it first to avoid data loss\n");
        return false;
    }

    printf("Wallet recovery: Renaming wallet.dat to wallet_old.dat...\n");
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    if (!MoveFileA(strWalletFile.c_str(), strOldWalletFile.c_str()))
#else
    if (rename(strWalletFile.c_str(), strOldWalletFile.c_str()) != 0)
#endif
    {
        printf("Wallet recovery: Failed to rename wallet.dat\n");
        return false;
    }

    printf("Wallet recovery: Extracting keys from wallet_old.dat...\n");

    vector<pair<vector<unsigned char>, CPrivKey> > vRecoveredKeys;
    vector<unsigned char> vchRecoveredDefaultKey;
    map<string, string> mapRecoveredNames;

    {
        Db dbOldWallet(NULL, 0);

        int ret = dbOldWallet.open(NULL, strOldWalletFile.c_str(), "main", DB_BTREE, DB_RDONLY, 0);
        if (ret != 0)
        {
            printf("Wallet recovery: Normal open failed (error %d), trying raw key scan...\n", ret);

            FILE* fWallet = fopen(strOldWalletFile.c_str(), "rb");
            if (!fWallet)
            {
                printf("Wallet recovery: Cannot open wallet file for scanning\n");
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
                MoveFileA(strOldWalletFile.c_str(), strWalletFile.c_str());
#else
                rename(strOldWalletFile.c_str(), strWalletFile.c_str());
#endif
                return false;
            }

            fseek(fWallet, 0, SEEK_END);
            long fileSize = ftell(fWallet);
            fseek(fWallet, 0, SEEK_SET);

            vector<unsigned char> fileData(fileSize);
            size_t bytesRead = fread(&fileData[0], 1, fileSize, fWallet);
            fclose(fWallet);

            if (bytesRead != (size_t)fileSize)
            {
                printf("Wallet recovery: Failed to read wallet file\n");
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
                MoveFileA(strOldWalletFile.c_str(), strWalletFile.c_str());
#else
                rename(strOldWalletFile.c_str(), strWalletFile.c_str());
#endif
                return false;
            }

            printf("Wallet recovery: Scanning %ld bytes for private keys...\n", fileSize);

            for (size_t i = 0; i + 300 < fileData.size(); i++)
            {
                if (fileData[i] == 0x30)
                {
                    try
                    {
                        size_t privKeyLen = 0;
                        size_t headerLen = 2;

                        if (fileData[i + 1] == 0x81)
                        {
                            privKeyLen = fileData[i + 2] + 3;
                            headerLen = 3;
                        }
                        else if (fileData[i + 1] == 0x82)
                        {
                            privKeyLen = (fileData[i + 2] << 8) + fileData[i + 3] + 4;
                            headerLen = 4;
                        }
                        else if (fileData[i + 1] >= 0x40 && fileData[i + 1] <= 0x7F)
                        {
                            privKeyLen = fileData[i + 1] + 2;
                        }
                        else
                        {
                            continue;
                        }

                        if (privKeyLen < 100 || privKeyLen > 300) continue;
                        if (i + privKeyLen > fileData.size()) continue;

                        if (i + headerLen + 1 >= fileData.size()) continue;
                        if (fileData[i + headerLen] != 0x02) continue;

                        CPrivKey vchPrivKey(fileData.begin() + i, fileData.begin() + i + privKeyLen);

                        CKey keyTest;
                        if (keyTest.SetPrivKey(vchPrivKey))
                        {
                            vector<unsigned char> vchPubKey = keyTest.GetPubKey();

                            if (vchPubKey.size() == 33 || vchPubKey.size() == 65)
                            {
                                bool duplicate = false;
                                for (size_t k = 0; k < vRecoveredKeys.size(); k++)
                                {
                                    if (vRecoveredKeys[k].first == vchPubKey)
                                    {
                                        duplicate = true;
                                        break;
                                    }
                                }
                                if (!duplicate)
                                {
                                    vRecoveredKeys.push_back(make_pair(vchPubKey, vchPrivKey));
                                    printf("  Recovered key: %s\n", HexStr(vchPubKey.begin(), vchPubKey.end(), true).substr(0, 16).c_str());
                                }
                            }
                        }
                    }
                    catch (...) { }
                }

                if (fileData[i] == 0x03 && fileData[i+1] == 'k' && fileData[i+2] == 'e' && fileData[i+3] == 'y')
                {
                    try
                    {
                        size_t keyStart = i + 4;
                        if (keyStart + 34 >= fileData.size()) continue;

                        unsigned char pubKeyLen = fileData[keyStart];
                        if (pubKeyLen != 33 && pubKeyLen != 65) continue;

                        keyStart++;
                        if (keyStart + pubKeyLen >= fileData.size()) continue;

                        vector<unsigned char> vchPubKey(fileData.begin() + keyStart, fileData.begin() + keyStart + pubKeyLen);

                        if (pubKeyLen == 33 && (vchPubKey[0] != 0x02 && vchPubKey[0] != 0x03)) continue;
                        if (pubKeyLen == 65 && vchPubKey[0] != 0x04) continue;

                        size_t privKeyStart = keyStart + pubKeyLen;
                        if (privKeyStart + 2 >= fileData.size()) continue;

                        size_t privKeyLen = 0;
                        if (fileData[privKeyStart] == 0x30)
                        {
                            if (fileData[privKeyStart + 1] == 0x81)
                            {
                                privKeyLen = fileData[privKeyStart + 2] + 3;
                            }
                            else if (fileData[privKeyStart + 1] == 0x82)
                            {
                                privKeyLen = (fileData[privKeyStart + 2] << 8) + fileData[privKeyStart + 3] + 4;
                            }
                            else
                            {
                                privKeyLen = fileData[privKeyStart + 1] + 2;
                            }
                        }

                        if (privKeyLen < 100 || privKeyLen > 300) continue;
                        if (privKeyStart + privKeyLen > fileData.size()) continue;

                        CPrivKey vchPrivKey(fileData.begin() + privKeyStart, fileData.begin() + privKeyStart + privKeyLen);

                        CKey keyTest;
                        if (keyTest.SetPrivKey(vchPrivKey))
                        {
                            vector<unsigned char> vchTestPubKey = keyTest.GetPubKey();
                            if (vchTestPubKey == vchPubKey)
                            {
                                bool duplicate = false;
                                for (size_t k = 0; k < vRecoveredKeys.size(); k++)
                                {
                                    if (vRecoveredKeys[k].first == vchPubKey)
                                    {
                                        duplicate = true;
                                        break;
                                    }
                                }
                                if (!duplicate)
                                {
                                    vRecoveredKeys.push_back(make_pair(vchPubKey, vchPrivKey));
                                    printf("  Recovered key (matched): %s\n", HexStr(vchPubKey.begin(), vchPubKey.end(), true).substr(0, 16).c_str());
                                }
                            }
                        }
                    }
                    catch (...) { }
                }

                if (fileData[i] == 0x0a && i + 11 < fileData.size() &&
                    fileData[i+1] == 'd' && fileData[i+2] == 'e' && fileData[i+3] == 'f' &&
                    fileData[i+4] == 'a' && fileData[i+5] == 'u' && fileData[i+6] == 'l' &&
                    fileData[i+7] == 't' && fileData[i+8] == 'k' && fileData[i+9] == 'e' &&
                    fileData[i+10] == 'y')
                {
                    try
                    {
                        size_t keyStart = i + 11;
                        if (keyStart + 34 >= fileData.size()) continue;

                        unsigned char pubKeyLen = fileData[keyStart];
                        if (pubKeyLen != 33 && pubKeyLen != 65) continue;

                        keyStart++;
                        if (keyStart + pubKeyLen > fileData.size()) continue;

                        vchRecoveredDefaultKey.assign(fileData.begin() + keyStart, fileData.begin() + keyStart + pubKeyLen);
                    }
                    catch (...) { }
                }
            }
        }
        else
        {
            Dbc* pcursor = NULL;
            ret = dbOldWallet.cursor(NULL, &pcursor, 0);
            if (ret != 0 || pcursor == NULL)
            {
                printf("Wallet recovery: Failed to create cursor\n");
                dbOldWallet.close(0);
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
                MoveFileA(strOldWalletFile.c_str(), strWalletFile.c_str());
#else
                rename(strOldWalletFile.c_str(), strWalletFile.c_str());
#endif
                return false;
            }

            Dbt datKey;
            Dbt datValue;
            datKey.set_flags(DB_DBT_MALLOC);
            datValue.set_flags(DB_DBT_MALLOC);

            while ((ret = pcursor->get(&datKey, &datValue, DB_NEXT)) == 0)
            {
                CDataStream ssKey((char*)datKey.get_data(), (char*)datKey.get_data() + datKey.get_size(), SER_DISK);
                CDataStream ssValue((char*)datValue.get_data(), (char*)datValue.get_data() + datValue.get_size(), SER_DISK);

                string strType;
                ssKey >> strType;

                if (strType == "key" || strType == "wkey")
                {
                    vector<unsigned char> vchPubKey;
                    ssKey >> vchPubKey;
                    CPrivKey vchPrivKey;
                    if (strType == "key")
                        ssValue >> vchPrivKey;
                    else
                    {
                        CWalletKey wkey;
                        ssValue >> wkey;
                        vchPrivKey = wkey.vchPrivKey;
                    }
                    vRecoveredKeys.push_back(make_pair(vchPubKey, vchPrivKey));
                    printf("  Recovered key: %s\n", HexStr(vchPubKey.begin(), vchPubKey.end(), true).substr(0, 16).c_str());
                }
                else if (strType == "defaultkey")
                {
                    ssValue >> vchRecoveredDefaultKey;
                }
                else if (strType == "name")
                {
                    string strAddress;
                    ssKey >> strAddress;
                    string strName;
                    ssValue >> strName;
                    mapRecoveredNames[strAddress] = strName;
                }

                if (datKey.get_data()) { memset(datKey.get_data(), 0, datKey.get_size()); free(datKey.get_data()); }
                if (datValue.get_data()) { memset(datValue.get_data(), 0, datValue.get_size()); free(datValue.get_data()); }
                datKey.set_data(NULL);
                datValue.set_data(NULL);
            }

            pcursor->close();
            dbOldWallet.close(0);
        }
    }

    printf("Wallet recovery: Found %d keys\n", (int)vRecoveredKeys.size());

    if (vRecoveredKeys.empty())
    {
        printf("Wallet recovery: No keys found in old wallet\n");
        printf("Wallet recovery: Restoring original wallet.dat\n");
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
        MoveFileA(strOldWalletFile.c_str(), strWalletFile.c_str());
#else
        rename(strOldWalletFile.c_str(), strWalletFile.c_str());
#endif
        return false;
    }

    printf("Wallet recovery: Creating new wallet.dat with recovered keys...\n");

    {
        CWalletDB walletdb("cr+");

        for (size_t i = 0; i < vRecoveredKeys.size(); i++)
        {
            const vector<unsigned char>& vchPubKey = vRecoveredKeys[i].first;
            const CPrivKey& vchPrivKey = vRecoveredKeys[i].second;

            if (!walletdb.WriteKey(vchPubKey, vchPrivKey))
            {
                printf("Wallet recovery: Failed to write key %d\n", (int)i);
            }
            else
            {
                CRITICAL_BLOCK(cs_mapKeys)
                {
                    mapKeys[vchPubKey] = vchPrivKey;
                    mapPubKeys[Hash160(vchPubKey)] = vchPubKey;
                }
            }
        }

        if (!vchRecoveredDefaultKey.empty())
        {
            walletdb.WriteDefaultKey(vchRecoveredDefaultKey);
        }
        else if (!vRecoveredKeys.empty())
        {
            walletdb.WriteDefaultKey(vRecoveredKeys[0].first);
        }

        for (map<string, string>::iterator it = mapRecoveredNames.begin(); it != mapRecoveredNames.end(); ++it)
        {
            walletdb.WriteName(it->first, it->second);
        }
    }

    printf("Wallet recovery: Successfully recovered %d keys into new wallet.dat\n", (int)vRecoveredKeys.size());
    printf("Wallet recovery: Old wallet saved as wallet_old.dat\n");

    return true;
}


//
// CDB
//

static CCriticalSection cs_db;
static bool fDbEnvInit = false;
DbEnv dbenv(0);
static map<string, int> mapFileUseCount;
static map<string, Db*> mapDb;

class CDBInit
{
public:
    CDBInit()
    {
    }
    ~CDBInit()
    {
        if (fDbEnvInit)
        {
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}
instance_of_cdbinit;


CDB::CDB(const char* pszFile, const char* pszMode) : pdb(NULL)
{
    int ret;
    if (pszFile == NULL)
        return;

    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    bool fCreate = strchr(pszMode, 'c');
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    CRITICAL_BLOCK(cs_db)
    {
        if (!fDbEnvInit)
        {
            if (fShutdown)
                return;
            string strDataDir = GetDataDir();
            string strLogDir = strDataDir + "/database";
            string strErrorFile = strDataDir + "/db.log";

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
            string strDataDirWin = GetBDBPath(strDataDir);
            string strLogDirWin = GetBDBPath(strLogDir);
            _mkdir(strDataDirWin.c_str());
            _mkdir(strLogDirWin.c_str());
#else
            _mkdir(strDataDir.c_str());
            _mkdir(strLogDir.c_str());
#endif
            printf("dbenv.open strLogDir=%s strErrorFile=%s\n", strLogDir.c_str(), strErrorFile.c_str());

            string strBDBDataDir = GetBDBPath(strDataDir);
            string strBDBLogDir = GetBDBPath(strLogDir);

            dbenv.set_lg_dir(strBDBLogDir.c_str());
            dbenv.set_lg_max(10000000);
            dbenv.set_lk_max_locks(10000);
            dbenv.set_lk_max_objects(10000);
            dbenv.set_errfile(fopen(strErrorFile.c_str(), "a")); /// debug
            dbenv.set_flags(DB_AUTO_COMMIT, 1);
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
            ret = dbenv.open(strBDBDataDir.c_str(),
                             DB_CREATE     |
                             DB_INIT_LOCK  |
                             DB_INIT_LOG   |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN   |
                             DB_THREAD     |
                             DB_PRIVATE    |
                             DB_RECOVER,
                             0);
#else
            ret = dbenv.open(strBDBDataDir.c_str(),
                             DB_CREATE     |
                             DB_INIT_LOCK  |
                             DB_INIT_LOG   |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN   |
                             DB_THREAD     |
                             DB_PRIVATE    |
                             DB_RECOVER,
                             S_IRUSR | S_IWUSR);
#endif
            if (ret > 0)
                throw runtime_error(strprintf("CDB() : error %d opening database environment\n", ret));
            fDbEnvInit = true;
        }

        strFile = pszFile;
        ++mapFileUseCount[strFile];
        pdb = mapDb[strFile];
        if (pdb == NULL)
        {
            pdb = new Db(&dbenv, 0);

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
            ret = pdb->open(NULL,      // Txn pointer
                            pszFile,   // Filename
                            "main",    // Logical db name
                            DB_BTREE,  // Database type
                            nFlags,    // Flags
                            0);
#else
            ret = pdb->open(NULL,      // Txn pointer
                            pszFile,   // Filename
                            "main",    // Logical db name
                            DB_BTREE,  // Database type
                            nFlags,    // Flags
                            S_IRUSR | S_IWUSR);
#endif

            if (ret > 0)
            {
                delete pdb;
                pdb = NULL;
                CRITICAL_BLOCK(cs_db)
                    --mapFileUseCount[strFile];
                strFile = "";
                throw runtime_error(strprintf("CDB() : can't open database file %s, error %d\n", pszFile, ret));
            }

            if (fCreate && !Exists(string("version")))
            {
                bool fTmp = fReadOnly;
                fReadOnly = false;
                WriteVersion(VERSION);
                fReadOnly = fTmp;
            }

            mapDb[strFile] = pdb;
        }
    }
}

void CDB::Close()
{
    if (!pdb)
        return;
    if (!vTxn.empty())
        vTxn.front()->abort();
    vTxn.clear();
    pdb = NULL;
    dbenv.txn_checkpoint(0, 0, 0);

    CRITICAL_BLOCK(cs_db)
        --mapFileUseCount[strFile];
}

void CloseDb(const string& strFile)
{
    CRITICAL_BLOCK(cs_db)
    {
        if (mapDb[strFile] != NULL)
        {
            // Close the database handle
            Db* pdb = mapDb[strFile];
            pdb->close(0);
            delete pdb;
            mapDb[strFile] = NULL;
        }
    }
}

void DBFlush(bool fShutdown)
{
    // Flush log data to the actual data file
    //  on all files that are not in use
    printf("DBFlush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started");
    if (!fDbEnvInit)
        return;
    CRITICAL_BLOCK(cs_db)
    {
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            printf("%s refcount=%d\n", strFile.c_str(), nRefCount);
            if (nRefCount == 0)
            {
                // Move log data to the dat file
                CloseDb(strFile);
                dbenv.txn_checkpoint(0, 0, 0);
                printf("%s flush\n", strFile.c_str());
                dbenv.lsn_reset(strFile.c_str(), 0);
                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
        if (fShutdown)
        {
            char** listp;
            if (mapFileUseCount.empty())
                dbenv.log_archive(&listp, DB_ARCH_REMOVE);
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}






//
// CTxDB
//

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    assert(!fClient);
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    assert(!fClient);
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    assert(!fClient);

    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();

    return Erase(make_pair(string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    assert(!fClient);
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDB::ReadOwnerTxes(uint160 hash160, int nMinHeight, vector<CTransaction>& vtx)
{
    assert(!fClient);
    vtx.clear();

    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << string("owner") << hash160 << CDiskTxPos(0, 0, 0);
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        // Unserialize
        string strType;
        uint160 hashItem;
        CDiskTxPos pos;
        ssKey >> strType >> hashItem >> pos;
        int nItemHeight;
        ssValue >> nItemHeight;

        // Read transaction
        if (strType != "owner" || hashItem != hash160)
            break;
        if (nItemHeight >= nMinHeight)
        {
            vtx.resize(vtx.size()+1);
            if (!vtx.back().ReadFromDisk(pos))
            {
                pcursor->close();
                return false;
            }
        }
    }

    pcursor->close();
    return true;
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    assert(!fClient);
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::EraseBlockIndex(uint256 hash)
{
    return Erase(make_pair(string("blockindex"), hash));
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool fUseIndexer = false;
bool fIndexerRebuilding = false;

bool CTxDB::WriteIndexerHeight(int nHeight)
{
    return Write(string("indexerheight"), nHeight);
}

bool CTxDB::ReadIndexerHeight(int& nHeight)
{
    nHeight = -1;
    return Read(string("indexerheight"), nHeight);
}

bool CTxDB::WriteAddrTx(uint160 addr, uint256 txhash, int nHeight)
{
    return Write(make_pair(make_pair(string("addrtx"), addr), txhash), nHeight);
}

bool CTxDB::EraseAddrTx(uint160 addr, uint256 txhash)
{
    return Erase(make_pair(make_pair(string("addrtx"), addr), txhash));
}

bool CTxDB::ReadAddrTxids(uint160 addr, vector<uint256>& vtxids)
{
    vtxids.clear();

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        CDataStream ssKey(SER_DISK);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(make_pair(string("addrtx"), addr), uint256(0));
        CDataStream ssValue(SER_DISK);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        string strType;
        uint160 addrItem;
        uint256 txhash;
        try
        {
            ssKey >> strType >> addrItem >> txhash;
        }
        catch (...)
        {
            break;
        }

        if (strType != "addrtx" || addrItem != addr)
            break;

        vtxids.push_back(txhash);
    }

    pcursor->close();
    return true;
}

struct CAddrOutEntry
{
    int64 nValue;
    int nHeight;
    bool fCoinBase;

    CAddrOutEntry() : nValue(0), nHeight(0), fCoinBase(false) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nValue);
        READWRITE(nHeight);
        READWRITE(fCoinBase);
    )
};

bool CTxDB::WriteAddrOut(uint160 addr, uint256 txhash, unsigned int n, int64 nValue, int nHeight, bool fCoinBase)
{
    CAddrOutEntry entry;
    entry.nValue = nValue;
    entry.nHeight = nHeight;
    entry.fCoinBase = fCoinBase;
    return Write(make_pair(make_pair(string("addrout"), addr), make_pair(txhash, n)), entry);
}

bool CTxDB::EraseAddrOut(uint160 addr, uint256 txhash, unsigned int n)
{
    return Erase(make_pair(make_pair(string("addrout"), addr), make_pair(txhash, n)));
}

bool CTxDB::ReadAddrOut(uint160 addr, uint256 txhash, unsigned int n, int64& nValue, int& nHeight)
{
    CAddrOutEntry entry;
    if (!Read(make_pair(make_pair(string("addrout"), addr), make_pair(txhash, n)), entry))
        return false;
    nValue = entry.nValue;
    nHeight = entry.nHeight;
    return true;
}

bool CTxDB::ReadAddrUTXOs(uint160 addr, vector<pair<COutPoint, pair<int64, pair<int, bool> > > >& vUTXOs)
{
    vUTXOs.clear();

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        CDataStream ssKey(SER_DISK);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(make_pair(string("addrout"), addr), make_pair(uint256(0), (unsigned int)0));
        CDataStream ssValue(SER_DISK);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        string strType;
        uint160 addrItem;
        uint256 txhash;
        unsigned int nOut;
        try
        {
            ssKey >> strType >> addrItem >> txhash >> nOut;
        }
        catch (...)
        {
            break;
        }

        if (strType != "addrout" || addrItem != addr)
            break;

        CAddrOutEntry entry;
        try
        {
            ssValue >> entry;
        }
        catch (...)
        {
            continue;
        }

        vUTXOs.push_back(make_pair(COutPoint(txhash, nOut),
            make_pair(entry.nValue, make_pair(entry.nHeight, entry.fCoinBase))));
    }

    pcursor->close();
    return true;
}

static bool EraseKeysByPrefix(Db* pdb, DbTxn* ptxn, const string& strPrefix)
{
    Dbc* pcursor = NULL;
    if (pdb->cursor(ptxn, &pcursor, 0) != 0 || pcursor == NULL)
        return false;

    vector<vector<unsigned char> > vKeysToErase;

    CDataStream ssSeek(SER_DISK);
    ssSeek << strPrefix;
    Dbt datSeek;
    datSeek.set_data(&ssSeek[0]);
    datSeek.set_size(ssSeek.size());
    Dbt datValue;
    datValue.set_flags(DB_DBT_MALLOC);

    Dbt datKey;
    datKey.set_data(&ssSeek[0]);
    datKey.set_size(ssSeek.size());
    datKey.set_flags(DB_DBT_MALLOC);

    int ret = pcursor->get(&datKey, &datValue, DB_SET_RANGE);
    while (ret == 0)
    {
        if (datKey.get_data() == NULL) break;

        const char* pKey = (const char*)datKey.get_data();
        size_t nKeySize = datKey.get_size();

        vector<unsigned char> vKey((const unsigned char*)pKey, (const unsigned char*)pKey + nKeySize);

        bool fMatch = false;
        if (nKeySize >= ssSeek.size())
        {
            CDataStream ssKeyType(pKey, pKey + nKeySize, SER_DISK);
            string strType;
            try { ssKeyType >> strType; } catch (...) {}
            fMatch = (strType == strPrefix);
        }

        if (datKey.get_data()) { free(datKey.get_data()); datKey.set_data(NULL); }
        if (datValue.get_data()) { free(datValue.get_data()); datValue.set_data(NULL); }

        if (!fMatch)
            break;

        vKeysToErase.push_back(vKey);

        datKey.set_flags(DB_DBT_MALLOC);
        datValue.set_flags(DB_DBT_MALLOC);
        ret = pcursor->get(&datKey, &datValue, DB_NEXT);
    }

    if (datKey.get_data()) free(datKey.get_data());
    if (datValue.get_data()) free(datValue.get_data());
    pcursor->close();

    for (size_t i = 0; i < vKeysToErase.size(); i++)
    {
        Dbt dk(&vKeysToErase[i][0], vKeysToErase[i].size());
        pdb->del(ptxn, &dk, 0);
    }

    return true;
}

bool CTxDB::EraseAllIndexerData()
{
    DbTxn* ptxn = GetTxn();
    EraseKeysByPrefix(pdb, ptxn, string("addrtx"));
    EraseKeysByPrefix(pdb, ptxn, string("addrout"));
    Erase(string("indexerheight"));
    return true;
}


// ---------------------------------------------------------------------------
// ATOM layer DB methods
// ---------------------------------------------------------------------------

struct CAtomBalanceEntry
{
    int64    nBalance;
    uint32_t nNonce;

    CAtomBalanceEntry() : nBalance(0), nNonce(0) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nBalance);
        READWRITE(nNonce);
    )
};

bool CTxDB::WriteAtomBalance(uint160 addr, int64 nBalance, uint32_t nNonce)
{
    CAtomBalanceEntry entry;
    entry.nBalance = nBalance;
    entry.nNonce   = nNonce;
    return Write(make_pair(string("atombal"), addr), entry);
}

bool CTxDB::ReadAtomBalance(uint160 addr, int64& nBalance, uint32_t& nNonce)
{
    CAtomBalanceEntry entry;
    if (!Read(make_pair(string("atombal"), addr), entry))
    {
        nBalance = 0;
        nNonce   = 0;
        return false;
    }
    nBalance = entry.nBalance;
    nNonce   = entry.nNonce;
    return true;
}

bool CTxDB::ReadAtomBalance(uint160 addr, int64& nBalance, uint32_t& nNonce, bool& fDbError)
{
    fDbError = false;
    CAtomBalanceEntry entry;
    if (!Read(make_pair(string("atombal"), addr), entry))
    {
        nBalance = 0;
        nNonce   = 0;
        if (Exists(make_pair(string("atombal"), addr)))
            fDbError = true;
        return false;
    }
    nBalance = entry.nBalance;
    nNonce   = entry.nNonce;
    return true;
}

bool CTxDB::EraseAtomBalance(uint160 addr)
{
    return Erase(make_pair(string("atombal"), addr));
}

bool CTxDB::SumAllAtomBalances(int64& nTotalOut)
{
    nTotalOut = 0;

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    CDataStream ssKey;
    ssKey << string("atombal");
    unsigned int fFlags = DB_SET_RANGE;

    loop
    {
        CDataStream ssKeyRet;
        if (fFlags == DB_SET_RANGE)
            ssKeyRet = ssKey;
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKeyRet, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        string strType;
        try { ssKeyRet >> strType; } catch (...) { break; }
        if (strType != "atombal")
            break;

        CAtomBalanceEntry entry;
        try { ssValue >> entry; } catch (...) { break; }

        if (entry.nBalance > 0)
            nTotalOut += entry.nBalance;
    }

    pcursor->close();
    return true;
}

struct CAtomTxEntry
{
    int           nHeight;
    unsigned int  nTime;
    unsigned char nType;
    uint160       addrFrom;
    uint160       addrTo;
    int64         nAmount;
    uint32_t      nNonce;
    unsigned char solAddrTo[32];

    CAtomTxEntry() : nHeight(0), nTime(0), nType(0), addrFrom(0), addrTo(0), nAmount(0), nNonce(0)
    {
        memset(solAddrTo, 0, 32);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nHeight);
        READWRITE(nTime);
        READWRITE(nType);
        READWRITE(addrFrom);
        READWRITE(addrTo);
        READWRITE(nAmount);
        READWRITE(nNonce);
        READWRITE(FLATDATA(solAddrTo));
    )
};

bool CTxDB::WriteAtomTx(uint256 txhash, int nHeight, unsigned int nTime, unsigned char nType,
                        uint160 addrFrom, uint160 addrTo, int64 nAmount, uint32_t nNonce,
                        const unsigned char* pSolAddrTo)
{
    CAtomTxEntry entry;
    entry.nHeight  = nHeight;
    entry.nTime    = nTime;
    entry.nType    = nType;
    entry.addrFrom = addrFrom;
    entry.addrTo   = addrTo;
    entry.nAmount  = nAmount;
    entry.nNonce   = nNonce;
    if (pSolAddrTo)
        memcpy(entry.solAddrTo, pSolAddrTo, 32);
    return Write(make_pair(string("atomtx"), txhash), entry);
}

bool CTxDB::ReadAtomTx(uint256 txhash, int& nHeight, unsigned int& nTime, unsigned char& nType,
                       uint160& addrFrom, uint160& addrTo, int64& nAmount, uint32_t& nNonce,
                       unsigned char* pSolAddrTo)
{
    CAtomTxEntry entry;
    if (!Read(make_pair(string("atomtx"), txhash), entry))
        return false;
    nHeight  = entry.nHeight;
    nTime    = entry.nTime;
    nType    = entry.nType;
    addrFrom = entry.addrFrom;
    addrTo   = entry.addrTo;
    nAmount  = entry.nAmount;
    nNonce   = entry.nNonce;
    if (pSolAddrTo)
        memcpy(pSolAddrTo, entry.solAddrTo, 32);
    return true;
}

bool CTxDB::EraseAtomTx(uint256 txhash)
{
    return Erase(make_pair(string("atomtx"), txhash));
}

bool CTxDB::WriteAtomAddrTx(uint160 addr, uint256 txhash, int nHeight)
{
    return Write(make_pair(make_pair(string("atomaddrtx"), addr), txhash), nHeight);
}

bool CTxDB::EraseAtomAddrTx(uint160 addr, uint256 txhash)
{
    return Erase(make_pair(make_pair(string("atomaddrtx"), addr), txhash));
}

bool CTxDB::ReadAtomAddrTxids(uint160 addr, vector<uint256>& vtxids)
{
    vtxids.clear();

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        CDataStream ssKey(SER_DISK);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(make_pair(string("atomaddrtx"), addr), uint256(0));
        CDataStream ssValue(SER_DISK);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        string strType;
        uint160 addrItem;
        uint256 txhash;
        try
        {
            ssKey >> strType >> addrItem >> txhash;
        }
        catch (...)
        {
            break;
        }

        if (strType != "atomaddrtx" || addrItem != addr)
            break;

        vtxids.push_back(txhash);
    }

    pcursor->close();
    return true;
}

bool CTxDB::EraseAllAtomData()
{
    DbTxn* ptxn = GetTxn();
    EraseKeysByPrefix(pdb, ptxn, string("atombal"));
    EraseKeysByPrefix(pdb, ptxn, string("atomtx"));
    EraseKeysByPrefix(pdb, ptxn, string("atomaddrtx"));
    Erase(string("atomtotalminted"));
    return true;
}

bool CTxDB::WriteAtomTotalMinted(int64 nTotal)
{
    return Write(string("atomtotalminted"), nTotal);
}

bool CTxDB::ReadAtomTotalMinted(int64& nTotal)
{
    if (!Read(string("atomtotalminted"), nTotal))
    {
        nTotal = 0;
        return false;
    }
    return true;
}


// ---------------------------------------------------------------------------
// DEX order DB methods
// ---------------------------------------------------------------------------

bool CTxDB::WriteDexOrder(uint256 txhash, const CDexOrderEntry& entry)
{
    return Write(make_pair(string("dexorder"), txhash), entry);
}

bool CTxDB::ReadDexOrder(uint256 txhash, CDexOrderEntry& entry)
{
    return Read(make_pair(string("dexorder"), txhash), entry);
}

bool CTxDB::EraseDexOrder(uint256 txhash)
{
    return Erase(make_pair(string("dexorder"), txhash));
}

bool CTxDB::LoadAllDexOrders(std::map<uint256, CDexOrderEntry>& orders)
{
    orders.clear();

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    CDataStream ssKey;
    ssKey << string("dexorder");
    unsigned int fFlags = DB_SET_RANGE;

    loop
    {
        CDataStream ssKeyRet;
        if (fFlags == DB_SET_RANGE)
            ssKeyRet = ssKey;
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKeyRet, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        string strType;
        uint256 txhash;
        try { ssKeyRet >> strType >> txhash; } catch (...) { break; }
        if (strType != "dexorder")
            break;

        CDexOrderEntry entry;
        try { ssValue >> entry; } catch (...) { continue; }

        orders[txhash] = entry;
    }

    pcursor->close();
    return true;
}


CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    auto mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool CTxDB::LoadBlockIndex()
{
    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("blockindex"), uint256(0));
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "blockindex")
        {
            CDiskBlockIndex diskindex;
            ssValue >> diskindex;

            // Construct block index object
            CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
            pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
            pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
            pindexNew->nFile          = diskindex.nFile;
            pindexNew->nBlockPos      = diskindex.nBlockPos;
            pindexNew->nHeight        = diskindex.nHeight;
            pindexNew->nVersion       = diskindex.nVersion;
            pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
            pindexNew->nTime          = diskindex.nTime;
            pindexNew->nBits          = diskindex.nBits;
            pindexNew->nNonce         = diskindex.nNonce;

            // Watch for genesis block and best block
            if (pindexGenesisBlock == NULL && diskindex.GetBlockHash() == hashGenesisBlock)
                pindexGenesisBlock = pindexNew;
        }
        else
        {
            break;
        }
    }
    pcursor->close();

    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found");
    }

    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : blockindex for hashBestChain not found");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;
    printf("LoadBlockIndex(): hashBestChain=%s  height=%d\n", hashBestChain.ToString().substr(0,16).c_str(), nBestHeight);

    return true;
}





//
// CAddrDB
//

bool CAddrDB::WriteAddress(const CAddress& addr)
{
    return Write(make_pair(string("addr"), addr.GetKey()), addr);
}

bool CAddrDB::EraseAddress(const CAddress& addr)
{
    return Erase(make_pair(string("addr"), addr.GetKey()));
}

bool CAddrDB::LoadAddresses()
{
    CRITICAL_BLOCK(cs_mapAddresses)
    {
        // Load user provided addresses
        CAutoFile filein = fopen((GetDataDir() + "/addr.txt").c_str(), "rt");
        if (filein)
        {
            try
            {
                char psz[1000];
                while (fgets(psz, sizeof(psz), filein))
                {
                    CAddress addr(psz, NODE_NETWORK);
                    addr.nTime = 0; // so it won't relay unless successfully connected
                    if (addr.IsValid())
                        AddAddress(addr);
                }
            }
            catch (...) { }
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return false;

        loop
        {
            // Read next record
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return false;

            // Unserialize
            string strType;
            ssKey >> strType;
            if (strType == "addr")
            {
                CAddress addr;
                ssValue >> addr;
                mapAddresses.insert(make_pair(addr.GetKey(), addr));
            }
        }
        pcursor->close();

        printf("Loaded %d addresses\n", mapAddresses.size());

        // Fix for possible bug that manifests in mapAddresses.count in irc.cpp,
        // just need to call count here and it doesn't happen there.  The bug was the
        // pack pragma in irc.cpp and has been fixed, but I'm not in a hurry to delete this.
        mapAddresses.count(vector<unsigned char>(18));
    }

    return true;
}

bool LoadAddresses()
{
    return CAddrDB("cr+").LoadAddresses();
}




//
// CWalletDB
//

bool CWalletDB::LoadWallet()
{
    vchDefaultKey.clear();
    int nFileVersion = 0;

    // Modify defaults
#if !defined(_WIN32) && !defined(__MINGW32__) && !defined(__WXMSW__)
    // Tray icon sometimes disappears on 9.10 karmic koala 64-bit, leaving no way to access the program
    fMinimizeToTray = false;
    fMinimizeOnClose = false;
#endif

    //// todo: shouldn't we catch exceptions and try to recover and continue?
    CRITICAL_BLOCK(cs_mapKeys)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return false;

        loop
        {
            // Read next record
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return false;

            // Unserialize
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            string strType;
            ssKey >> strType;
            if (strType == "name")
            {
                string strAddress;
                ssKey >> strAddress;
                ssValue >> mapAddressBook[strAddress];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = mapWallet[hash];
                ssValue >> wtx;

                if (wtx.GetHash() != hash)
                    printf("Error in wallet.dat, hash mismatch\n");

                //// debug print
                //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
                //printf(" %12I64d  %s  %s  %s\n",
                //    wtx.vout[0].nValue,
                //    DateTimeStrFormat("%x %H:%M:%S", wtx.nTime).c_str(),
                //    wtx.hashBlock.ToString().substr(0,16).c_str(),
                //    wtx.mapValue["message"].c_str());
            }
            else if (strType == "key" || strType == "wkey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CWalletKey wkey;
                if (strType == "key")
                    ssValue >> wkey.vchPrivKey;
                else
                    ssValue >> wkey;

                mapKeys[vchPubKey] = wkey.vchPrivKey;
                mapPubKeys[Hash160(vchPubKey)] = vchPubKey;
            }
            else if (strType == "defaultkey")
            {
                ssValue >> vchDefaultKey;
            }
            else if (strType == "version")
            {
                ssValue >> nFileVersion;
            }
            else if (strType == "setting")
            {
                string strKey;
                ssKey >> strKey;

                // Menu state
                // Only load wallet setting if not specified in command line or config file
                if (strKey == "fGenerateBitcoins" && mapArgs.count("-gen") == 0)
                    ssValue >> fGenerateBitcoins;

                // Options - only load from wallet if not specified in command line or config
                if (strKey == "nTransactionFee" && mapArgs.count("-paytxfee") == 0)
                    ssValue >> nTransactionFee;
                if (strKey == "addrIncoming")       ssValue >> addrIncoming;
                if (strKey == "fLimitProcessors" && mapArgs.count("-genproclimit") == 0)
                    ssValue >> fLimitProcessors;
                if (strKey == "nLimitProcessors" && mapArgs.count("-genproclimit") == 0)
                    ssValue >> nLimitProcessors;
                if (strKey == "fMinimizeToTray")    ssValue >> fMinimizeToTray;
                if (strKey == "fMinimizeOnClose")   ssValue >> fMinimizeOnClose;
                if (strKey == "fUseProxy" && mapArgs.count("-proxy") == 0)
                    ssValue >> fUseProxy;
                if (strKey == "addrProxy" && mapArgs.count("-proxy") == 0)
                    ssValue >> addrProxy;

            }
            else if (strType == "preimage")
            {
                vector<unsigned char> vchHash;
                ssKey >> vchHash;
                vector<unsigned char> vchPreimage;
                ssValue >> vchPreimage;
                mapHashPreimages[vchHash] = vchPreimage;
            }
            else if (strType == "sxaddr")
            {
                vector<unsigned char> vchScanPub;
                ssKey >> vchScanPub;
                CStealthAddress sxAddr;
                ssValue >> sxAddr;
                CRITICAL_BLOCK(cs_stealthAddresses)
                {
                    vStealthAddresses.push_back(sxAddr);
                }
            }
            else if (strType == "sxchgidx")
            {
                vector<unsigned char> vchSpendPub;
                ssKey >> vchSpendPub;
                uint32_t nIndex;
                ssValue >> nIndex;
                CRITICAL_BLOCK(cs_stealthAddresses)
                {
                    mapStealthChangeIndex[vchSpendPub] = nIndex;
                }
            }
            else if (strType == "sxdest")
            {
                vector<unsigned char> vchDestPub;
                ssKey >> vchDestPub;
                pair<vector<unsigned char>, vector<unsigned char> > scanAndEphem;
                ssValue >> scanAndEphem;
                mapStealthDestToScan[vchDestPub] = scanAndEphem;
            }
        }
        pcursor->close();
    }

    printf("nFileVersion = %d\n", nFileVersion);
    printf("fGenerateBitcoins = %d\n", fGenerateBitcoins);
    printf("nTransactionFee = %" PRI64d "\n", nTransactionFee);
    printf("addrIncoming = %s\n", addrIncoming.ToString().c_str());
    printf("fMinimizeToTray = %d\n", fMinimizeToTray);
    printf("fMinimizeOnClose = %d\n", fMinimizeOnClose);
    printf("fUseProxy = %d\n", fUseProxy);
    printf("addrProxy = %s\n", addrProxy.ToString().c_str());
    printf("stealth addresses loaded = %d\n", (int)vStealthAddresses.size());


    // The transaction fee setting won't be needed for many years to come.
    // Setting it to zero here in case they set it to something in an earlier version.
    if (nTransactionFee != 0)
    {
        nTransactionFee = 0;
        WriteSetting("nTransactionFee", nTransactionFee);
    }

    // Upgrade
    if (nFileVersion < VERSION)
    {
        // Get rid of old debug.log file in current directory
        if (nFileVersion <= 105 && !pszSetDataDir[0])
            unlink("debug.log");

        WriteVersion(VERSION);
    }

    return true;
}

bool LoadWallet(bool& fFirstRunRet)
{
    fFirstRunRet = false;
    if (!CWalletDB("cr+").LoadWallet())
        return false;
    fFirstRunRet = vchDefaultKey.empty();

    if (mapKeys.count(vchDefaultKey))
    {
        // Set keyUser
        keyUser.SetPubKey(vchDefaultKey);
        keyUser.SetPrivKey(mapKeys[vchDefaultKey]);
    }
    else
    {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();
        keyUser.MakeNewKey();
        if (!AddKey(keyUser))
            return false;
        if (!SetAddressBookName(PubKeyToAddress(keyUser.GetPubKey()), "Your Address"))
            return false;
        CWalletDB().WriteDefaultKey(keyUser.GetPubKey());
    }

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        bool fHasDefault = false;
        for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
        {
            if (vStealthAddresses[i].label == "default")
            {
                fHasDefault = true;
                break;
            }
        }

        if (vStealthAddresses.empty())
        {
            CStealthAddress sxAddr;
            CKey scanKey, spendKey;
            if (GenerateStealthAddress(sxAddr, scanKey, spendKey))
            {
                sxAddr.label = "default";
                CWalletDB walletdb;
                walletdb.WriteStealthAddress(sxAddr);
                walletdb.WriteStealthScanKey(sxAddr.scan_pubkey, scanKey.GetPrivKey());
                walletdb.WriteStealthSpendKey(sxAddr.spend_pubkey, spendKey.GetPrivKey());
                vStealthAddresses.push_back(sxAddr);
                printf("stealth: created default ok-address %s\n",
                       sxAddr.Encoded().substr(0, 24).c_str());
            }
        }
        else if (!fHasDefault)
        {
            vStealthAddresses[0].label = "default";
            CWalletDB().WriteStealthAddress(vStealthAddresses[0]);
            printf("stealth: assigned 'default' label to ok-address %s\n",
                   vStealthAddresses[0].Encoded().substr(0, 24).c_str());
        }
    }

    CreateThread(ThreadFlushWalletDB, NULL);
    return true;
}

void ThreadFlushWalletDB(void* parg)
{
    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (GetBoolArg("-noflushwallet"))
        return;

    unsigned int nLastSeen = nWalletDBUpdated;
    unsigned int nLastFlushed = nWalletDBUpdated;
    int64 nLastWalletUpdate = GetTime();
    while (!fShutdown)
    {
        Sleep(500);

        if (nLastSeen != nWalletDBUpdated)
        {
            nLastSeen = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2)
        {
            TRY_CRITICAL_BLOCK(cs_db)
            {
                // Don't do this if any databases are in use
                int nRefCount = 0;
                map<string, int>::iterator mi = mapFileUseCount.begin();
                while (mi != mapFileUseCount.end())
                {
                    nRefCount += (*mi).second;
                    mi++;
                }

                if (nRefCount == 0 && !fShutdown)
                {
                    string strFile = "wallet.dat";
                    map<string, int>::iterator mi = mapFileUseCount.find(strFile);
                    if (mi != mapFileUseCount.end())
                    {
                        printf("%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
                        printf("Flushing wallet.dat\n");
                        nLastFlushed = nWalletDBUpdated;
                        int64 nStart = GetTimeMillis();

                        // Flush wallet.dat so it's self contained
                        CloseDb(strFile);
                        dbenv.txn_checkpoint(0, 0, 0);
                        dbenv.lsn_reset(strFile.c_str(), 0);

                        mapFileUseCount.erase(mi++);
                        printf("Flushed wallet.dat %" PRI64d "ms\n", GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}
