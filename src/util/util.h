//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"

#include <string>

#include <openssl/sha.h>

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

#endif // UTIL_H
