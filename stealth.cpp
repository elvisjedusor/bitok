// Copyright (c) 2026 Bitok Developers
// Stealth address implementation - Satoshi's "key blinding" brought to life

#include "headers.h"
#include "stealth.h"

vector<CStealthAddress> vStealthAddresses;
map<vector<unsigned char>, pair<vector<unsigned char>, vector<unsigned char> > > mapStealthDestToScan;
map<vector<unsigned char>, uint32_t> mapStealthChangeIndex;
CCriticalSection cs_stealthAddresses;


static bool ECPointAdd(const EC_GROUP* group,
                       const vector<unsigned char>& vchA,
                       const vector<unsigned char>& vchB,
                       vector<unsigned char>& vchResult)
{
    EC_POINT* pointA = EC_POINT_new(group);
    EC_POINT* pointB = EC_POINT_new(group);
    EC_POINT* pointR = EC_POINT_new(group);
    BN_CTX* ctx = BN_CTX_new();
    bool fOk = false;

    if (!pointA || !pointB || !pointR || !ctx)
        goto cleanup;

    if (!EC_POINT_oct2point(group, pointA, &vchA[0], vchA.size(), ctx))
        goto cleanup;
    if (!EC_POINT_oct2point(group, pointB, &vchB[0], vchB.size(), ctx))
        goto cleanup;
    if (!EC_POINT_add(group, pointR, pointA, pointB, ctx))
        goto cleanup;

    {
        size_t nSize = EC_POINT_point2oct(group, pointR, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, ctx);
        vchResult.resize(nSize);
        if (EC_POINT_point2oct(group, pointR, POINT_CONVERSION_UNCOMPRESSED, &vchResult[0], nSize, ctx) != nSize)
            goto cleanup;
    }

    fOk = true;

cleanup:
    if (pointA) EC_POINT_free(pointA);
    if (pointB) EC_POINT_free(pointB);
    if (pointR) EC_POINT_free(pointR);
    if (ctx) BN_CTX_free(ctx);
    return fOk;
}


static bool ECPointMultiply(const EC_GROUP* group,
                            const vector<unsigned char>& vchScalar,
                            vector<unsigned char>& vchResult)
{
    BN_CTX* ctx = BN_CTX_new();
    BIGNUM* bnScalar = BN_new();
    EC_POINT* point = EC_POINT_new(group);
    bool fOk = false;

    if (!ctx || !bnScalar || !point)
        goto cleanup;

    if (!BN_bin2bn(&vchScalar[0], vchScalar.size(), bnScalar))
        goto cleanup;
    if (!EC_POINT_mul(group, point, bnScalar, NULL, NULL, ctx))
        goto cleanup;

    {
        size_t nSize = EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, ctx);
        vchResult.resize(nSize);
        if (EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED, &vchResult[0], nSize, ctx) != nSize)
            goto cleanup;
    }

    fOk = true;

cleanup:
    if (bnScalar) BN_free(bnScalar);
    if (point) EC_POINT_free(point);
    if (ctx) BN_CTX_free(ctx);
    return fOk;
}


static bool ECDHSecret(const vector<unsigned char>& vchPrivKey,
                       const vector<unsigned char>& vchPubKey,
                       vector<unsigned char>& vchSecret)
{
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey)
        return false;

    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    BN_CTX* ctx = BN_CTX_new();
    BIGNUM* bnPriv = BN_new();
    EC_POINT* pointPub = EC_POINT_new(group);
    EC_POINT* pointResult = EC_POINT_new(group);
    bool fOk = false;

    if (!ctx || !bnPriv || !pointPub || !pointResult)
        goto cleanup;

    if (!BN_bin2bn(&vchPrivKey[0], vchPrivKey.size(), bnPriv))
        goto cleanup;
    if (!EC_POINT_oct2point(group, pointPub, &vchPubKey[0], vchPubKey.size(), ctx))
        goto cleanup;
    if (!EC_POINT_mul(group, pointResult, NULL, pointPub, bnPriv, ctx))
        goto cleanup;

    {
        size_t nSize = EC_POINT_point2oct(group, pointResult, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, ctx);
        vchSecret.resize(nSize);
        if (EC_POINT_point2oct(group, pointResult, POINT_CONVERSION_UNCOMPRESSED, &vchSecret[0], nSize, ctx) != nSize)
            goto cleanup;
    }

    fOk = true;

cleanup:
    if (bnPriv) BN_free(bnPriv);
    if (pointPub) EC_POINT_free(pointPub);
    if (pointResult) EC_POINT_free(pointResult);
    if (ctx) BN_CTX_free(ctx);
    EC_KEY_free(eckey);
    return fOk;
}


static vector<unsigned char> HashToScalar(const vector<unsigned char>& vchData)
{
    uint256 hash;
    SHA256(&vchData[0], vchData.size(), (unsigned char*)&hash);
    uint256 hash2;
    SHA256((unsigned char*)&hash, sizeof(hash), (unsigned char*)&hash2);
    vector<unsigned char> vchResult(32);
    memcpy(&vchResult[0], &hash2, 32);
    return vchResult;
}


bool StealthSecret(const vector<unsigned char>& vchPriv,
                   const vector<unsigned char>& vchPub,
                   const vector<unsigned char>& vchSpendPub,
                   vector<unsigned char>& vchSharedSecret,
                   vector<unsigned char>& vchDestPubKey)
{
    vector<unsigned char> vchECDH;
    if (!ECDHSecret(vchPriv, vchPub, vchECDH))
        return false;

    vchSharedSecret = HashToScalar(vchECDH);

    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey)
        return false;

    const EC_GROUP* group = EC_KEY_get0_group(eckey);

    vector<unsigned char> vchTweakPub;
    if (!ECPointMultiply(group, vchSharedSecret, vchTweakPub))
    {
        EC_KEY_free(eckey);
        return false;
    }

    if (!ECPointAdd(group, vchSpendPub, vchTweakPub, vchDestPubKey))
    {
        EC_KEY_free(eckey);
        return false;
    }

    EC_KEY_free(eckey);
    return true;
}


bool StealthSecretSpend(const vector<unsigned char>& vchScanPriv,
                        const vector<unsigned char>& vchEphemPub,
                        const vector<unsigned char>& vchSpendPriv,
                        vector<unsigned char>& vchDestPrivKey)
{
    vector<unsigned char> vchECDH;
    if (!ECDHSecret(vchScanPriv, vchEphemPub, vchECDH))
        return false;

    vector<unsigned char> vchTweak = HashToScalar(vchECDH);

    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey)
        return false;

    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    BIGNUM* order = BN_new();
    BIGNUM* bnSpend = BN_new();
    BIGNUM* bnTweak = BN_new();
    BIGNUM* bnResult = BN_new();
    BN_CTX* ctx = BN_CTX_new();
    bool fOk = false;

    if (!order || !bnSpend || !bnTweak || !bnResult || !ctx)
        goto cleanup;

    if (!EC_GROUP_get_order(group, order, ctx))
        goto cleanup;
    if (!BN_bin2bn(&vchSpendPriv[0], vchSpendPriv.size(), bnSpend))
        goto cleanup;
    if (!BN_bin2bn(&vchTweak[0], vchTweak.size(), bnTweak))
        goto cleanup;
    if (!BN_mod_add(bnResult, bnSpend, bnTweak, order, ctx))
        goto cleanup;

    vchDestPrivKey.resize(32, 0);
    {
        int nBytes = BN_num_bytes(bnResult);
        if (nBytes > 32)
            goto cleanup;
        BN_bn2bin(bnResult, &vchDestPrivKey[32 - nBytes]);
    }

    fOk = true;

cleanup:
    if (order) BN_free(order);
    if (bnSpend) BN_free(bnSpend);
    if (bnTweak) BN_free(bnTweak);
    if (bnResult) BN_free(bnResult);
    if (ctx) BN_CTX_free(ctx);
    EC_KEY_free(eckey);
    return fOk;
}


bool StealthEphemeral(const CStealthAddress& addr,
                      vector<unsigned char>& vchEphemPubKey,
                      vector<unsigned char>& vchDestPubKey,
                      vector<unsigned char>& vchSharedSecret)
{
    CKey ephemKey;
    ephemKey.MakeNewKey();

    vector<unsigned char> vchEphemPriv = ephemKey.GetSecret();
    vchEphemPubKey = ephemKey.GetCompressedPubKey();

    return StealthSecret(vchEphemPriv, addr.scan_pubkey, addr.spend_pubkey,
                         vchSharedSecret, vchDestPubKey);
}


bool StealthScan(const vector<unsigned char>& vchScanPriv,
                 const vector<unsigned char>& vchSpendPub,
                 const vector<unsigned char>& vchEphemPub,
                 vector<unsigned char>& vchDestPubKey)
{
    vector<unsigned char> vchSharedSecret;
    return StealthSecret(vchScanPriv, vchEphemPub, vchSpendPub,
                         vchSharedSecret, vchDestPubKey);
}


bool GenerateStealthAddress(CStealthAddress& stealthAddr, CKey& scanKey, CKey& spendKey)
{
    scanKey.MakeNewKey();
    spendKey.MakeNewKey();

    stealthAddr.scan_pubkey = scanKey.GetCompressedPubKey();
    stealthAddr.spend_pubkey = spendKey.GetCompressedPubKey();

    return true;
}


vector<unsigned char> BuildStealthOpReturn(const vector<unsigned char>& vchEphemPub)
{
    vector<unsigned char> vchData;
    vchData.push_back(STEALTH_OP_RETURN_PREFIX);
    vchData.insert(vchData.end(), vchEphemPub.begin(), vchEphemPub.end());
    return vchData;
}


bool ParseStealthOpReturn(const CScript& script, vector<unsigned char>& vchEphemPub)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    vector<unsigned char> vchData;

    if (!script.GetOp(pc, opcode))
        return false;
    if (opcode != OP_RETURN)
        return false;
    if (!script.GetOp(pc, opcode, vchData))
        return false;

    if (vchData.size() < 2)
        return false;
    if (vchData[0] != STEALTH_OP_RETURN_PREFIX)
        return false;

    vchEphemPub.assign(vchData.begin() + 1, vchData.end());

    if (vchEphemPub.size() != 33)
        return false;

    if (vchEphemPub[0] != 0x02 && vchEphemPub[0] != 0x03)
        return false;

    return true;
}


string CStealthAddress::Encoded() const
{
    if (!IsValid())
        return "";

    vector<unsigned char> vchRaw;
    vchRaw.push_back(STEALTH_ADDRESS_VERSION);
    vchRaw.insert(vchRaw.end(), scan_pubkey.begin(), scan_pubkey.end());
    vchRaw.insert(vchRaw.end(), spend_pubkey.begin(), spend_pubkey.end());

    return string(STEALTH_ADDRESS_PREFIX) + EncodeBase58Check(vchRaw);
}


bool CStealthAddress::SetEncoded(const string& strAddr)
{
    string strPrefix(STEALTH_ADDRESS_PREFIX);
    if (strAddr.size() <= strPrefix.size() ||
        strAddr.substr(0, strPrefix.size()) != strPrefix)
        return false;

    string strBase58 = strAddr.substr(strPrefix.size());
    vector<unsigned char> vchRaw;
    if (!DecodeBase58Check(strBase58, vchRaw))
        return false;

    if (vchRaw.empty() || vchRaw[0] != STEALTH_ADDRESS_VERSION)
        return false;

    if (vchRaw.size() != 1 + 33 + 33)
        return false;

    scan_pubkey.assign(vchRaw.begin() + 1, vchRaw.begin() + 1 + 33);
    spend_pubkey.assign(vchRaw.begin() + 1 + 33, vchRaw.end());

    return IsValid();
}


bool CStealthAddress::IsValid() const
{
    if (scan_pubkey.size() != 33 || (scan_pubkey[0] != 0x02 && scan_pubkey[0] != 0x03))
        return false;
    if (spend_pubkey.size() != 33 || (spend_pubkey[0] != 0x02 && spend_pubkey[0] != 0x03))
        return false;

    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!eckey)
        return false;

    const EC_GROUP* group = EC_KEY_get0_group(eckey);
    BN_CTX* ctx = BN_CTX_new();
    EC_POINT* point = EC_POINT_new(group);
    bool fValid = true;

    if (!ctx || !point)
    {
        fValid = false;
    }
    else
    {
        if (!EC_POINT_oct2point(group, point, &scan_pubkey[0], scan_pubkey.size(), ctx))
            fValid = false;
        if (fValid && !EC_POINT_oct2point(group, point, &spend_pubkey[0], spend_pubkey.size(), ctx))
            fValid = false;
    }

    if (point) EC_POINT_free(point);
    if (ctx) BN_CTX_free(ctx);
    EC_KEY_free(eckey);
    return fValid;
}


string EncodeStealthSecret(const vector<unsigned char>& vchScanSecret, const vector<unsigned char>& vchSpendSecret)
{
    if (vchScanSecret.size() != 32 || vchSpendSecret.size() != 32)
        return "";

    vector<unsigned char> vchRaw;
    vchRaw.push_back(STEALTH_SECRET_VERSION);
    vchRaw.insert(vchRaw.end(), vchScanSecret.begin(), vchScanSecret.end());
    vchRaw.insert(vchRaw.end(), vchSpendSecret.begin(), vchSpendSecret.end());

    return string(STEALTH_SECRET_PREFIX) + EncodeBase58Check(vchRaw);
}


bool DecodeStealthSecret(const string& strEncoded, vector<unsigned char>& vchScanSecret, vector<unsigned char>& vchSpendSecret)
{
    string strPrefix(STEALTH_SECRET_PREFIX);
    if (strEncoded.size() <= strPrefix.size() ||
        strEncoded.substr(0, strPrefix.size()) != strPrefix)
        return false;

    string strBase58 = strEncoded.substr(strPrefix.size());
    vector<unsigned char> vchRaw;
    if (!DecodeBase58Check(strBase58, vchRaw))
        return false;

    if (vchRaw.size() != 1 + 32 + 32)
        return false;

    if (vchRaw[0] != STEALTH_SECRET_VERSION)
        return false;

    vchScanSecret.assign(vchRaw.begin() + 1, vchRaw.begin() + 1 + 32);
    vchSpendSecret.assign(vchRaw.begin() + 1 + 32, vchRaw.end());
    return true;
}


bool StealthDeriveChangeKey(const vector<unsigned char>& vchSpendSecret,
                            uint32_t nIndex,
                            vector<unsigned char>& vchChangePrivKey,
                            vector<unsigned char>& vchChangePubKey)
{
    if (vchSpendSecret.size() != 32)
        return false;

    static const string strDomain("stealth_change");

    vector<unsigned char> vchData;
    vchData.insert(vchData.end(), vchSpendSecret.begin(), vchSpendSecret.end());
    vchData.insert(vchData.end(), strDomain.begin(), strDomain.end());
    vchData.push_back((nIndex >>  0) & 0xFF);
    vchData.push_back((nIndex >>  8) & 0xFF);
    vchData.push_back((nIndex >> 16) & 0xFF);
    vchData.push_back((nIndex >> 24) & 0xFF);

    uint256 hash;
    SHA256(&vchData[0], vchData.size(), (unsigned char*)&hash);
    uint256 hash2;
    SHA256((unsigned char*)&hash, sizeof(hash), (unsigned char*)&hash2);

    vchChangePrivKey.resize(32);
    memcpy(&vchChangePrivKey[0], &hash2, 32);

    CKey changeKey;
    if (!changeKey.SetSecret(vchChangePrivKey))
        return false;

    vchChangePubKey = changeKey.GetPubKey();
    return true;
}


bool ScanStealthChangeKeys(const vector<unsigned char>& vchSpendSecret, const set<uint160>& setOnChainAddresses)
{
    CKey spendKey;
    if (!spendKey.SetSecret(vchSpendSecret))
        return false;

    vector<unsigned char> vchSpendPub = spendKey.GetCompressedPubKey();

    int nRecovered = 0;
    for (uint32_t nIndex = 0; ; nIndex++)
    {
        vector<unsigned char> vchChangePriv, vchChangePub;
        if (!StealthDeriveChangeKey(vchSpendSecret, nIndex, vchChangePriv, vchChangePub))
            return false;

        uint160 hash160 = Hash160(vchChangePub);

        if (setOnChainAddresses.count(hash160) == 0)
        {
            CRITICAL_BLOCK(cs_stealthAddresses)
            {
                mapStealthChangeIndex[vchSpendPub] = nIndex;
            }
            CWalletDB().WriteStealthChangeIndex(vchSpendPub, nIndex);
            printf("stealth: change key scan complete - recovered %d keys, next index %u\n",
                   nRecovered, nIndex);
            break;
        }

        CKey changeKey;
        if (!changeKey.SetSecret(vchChangePriv))
            return false;

        if (!AddKey(changeKey))
            return false;

        nRecovered++;
        printf("stealth: recovered change key at index %u (addr=%s)\n",
               nIndex, PubKeyToAddress(vchChangePub).c_str());
    }

    return true;
}
