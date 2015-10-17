//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include <string>

//*****************************************************************************
//*****************************************************************************
namespace util
{
    // TODO make std::vector<unsigned char> instead of strings
    std::string base64_encode(const std::string& s);
    std::string base64_decode(const std::string& s);
}

#endif // UTIL_H
