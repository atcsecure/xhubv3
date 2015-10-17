//******************************************************************************
//******************************************************************************

#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>
#include <boost/pool/pool_alloc.hpp>

#define WARN()  LOG('W')
#define ERR()   LOG('E')
#define TRACE() LOG('T')

#define DEBUG_TRACE() (TRACE() << __FUNCTION__)
#define DEBUG_TRACE_TODO() (TRACE() << "TODO " << __FUNCTION__)
// #define DEBUG_TRACE()
// #define DEBUG_TRACE_TODO()

//******************************************************************************
//******************************************************************************
class LOG : public std::basic_stringstream<char, std::char_traits<char>,
                                        boost::pool_allocator<char> > // std::stringstream
{
public:
    LOG(const char reason = 'I');
    virtual ~LOG();

private:
    const std::string makeFileName() const;

private:
    char m_r;
};

#endif // LOGGER_H
