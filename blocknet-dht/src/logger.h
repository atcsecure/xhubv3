//******************************************************************************
//******************************************************************************

#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>
#include <boost/pool/pool_alloc.hpp>

#define DEBUG_TRACE() (LOG() << __FUNCTION__)
// #define DEBUG_TRACE()

//******************************************************************************
//******************************************************************************
class LOG : public std::basic_stringstream<char, std::char_traits<char>,
                                        boost::pool_allocator<char> > // std::stringstream
{
public:
    LOG();
    virtual ~LOG();

//private:
//    const std::string makeFileName() const;
};

#endif // LOGGER_H
