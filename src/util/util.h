//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"
#include "../serialize.h"

#include <string>

#include <openssl/sha.h>
#include <openssl/ripemd.h>

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))

//*****************************************************************************
//*****************************************************************************
namespace util
{
    void init();

    std::wstring wide_string(std::string const & s);//, std::locale const &loc);
    // std::string narrow_string(std::wstring const &s, char default_char = '?');//, std::locale const &loc, char default_char = '?');

    std::string mb_string(std::string const & s);
    std::string mb_string(std::wstring const & s);

    std::string base64_encode(const std::vector<unsigned char> & s);
    std::string base64_encode(const std::string & s);
    std::string base64_decode(const std::string & s);

    template<typename T1> uint256 hash(const T1 pbegin, const T1 pend)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T1, typename T2>
    inline uint256 hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
        SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T1, typename T2, typename T3>
    inline uint256 hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end,
                        const T3 p3begin, const T3 p3end)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
        SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
        SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T1, typename T2, typename T3, typename T4>
    inline uint256 hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end,
                        const T3 p3begin, const T3 p3end,
                        const T4 p4begin, const T4 p4end)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
        SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
        SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
        SHA256_Update(&ctx, (p4begin == p4end ? pblank : (unsigned char*)&p4begin[0]), (p4end - p4begin) * sizeof(p4begin[0]));
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    inline uint256 hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end,
                        const T3 p3begin, const T3 p3end,
                        const T4 p4begin, const T4 p4end,
                        const T5 p5begin, const T5 p5end,
                        const T6 p6begin, const T6 p6end)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
        SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
        SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
        SHA256_Update(&ctx, (p4begin == p4end ? pblank : (unsigned char*)&p4begin[0]), (p4end - p4begin) * sizeof(p4begin[0]));
        SHA256_Update(&ctx, (p5begin == p5end ? pblank : (unsigned char*)&p5begin[0]), (p5end - p5begin) * sizeof(p5begin[0]));
        SHA256_Update(&ctx, (p6begin == p6end ? pblank : (unsigned char*)&p6begin[0]), (p6end - p6begin) * sizeof(p6begin[0]));
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    inline uint256 hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end,
                        const T3 p3begin, const T3 p3end,
                        const T4 p4begin, const T4 p4end,
                        const T5 p5begin, const T5 p5end,
                        const T6 p6begin, const T6 p6end,
                        const T7 p7begin, const T7 p7end)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
        SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
        SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
        SHA256_Update(&ctx, (p4begin == p4end ? pblank : (unsigned char*)&p4begin[0]), (p4end - p4begin) * sizeof(p4begin[0]));
        SHA256_Update(&ctx, (p5begin == p5end ? pblank : (unsigned char*)&p5begin[0]), (p5end - p5begin) * sizeof(p5begin[0]));
        SHA256_Update(&ctx, (p6begin == p6end ? pblank : (unsigned char*)&p6begin[0]), (p6end - p6begin) * sizeof(p6begin[0]));
        SHA256_Update(&ctx, (p7begin == p7end ? pblank : (unsigned char*)&p7begin[0]), (p7end - p7begin) * sizeof(p7begin[0]));
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }
}

std::string real_strprintf(const std::string &format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces = false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

std::string HexStr(const std::vector<unsigned char>& vch, bool fSpaces=false);

std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);

inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

static const int PROTOCOL_VERSION = 0;

class CHashWriter
{
private:
    SHA256_CTX ctx;

public:
    int nType;
    int nVersion;

    void Init() {
        SHA256_Init(&ctx);
    }

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {
        Init();
    }

    CHashWriter& write(const char *pch, size_t size) {
        SHA256_Update(&ctx, pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 hash1;
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T>
    CHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

template<typename T>
uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

uint256 GetRandHash();

#endif // UTIL_H
