//*****************************************************************************
//*****************************************************************************

#ifndef __EXCEPTION_HANDLER_H_INCLUDED__
#define __EXCEPTION_HANDLER_H_INCLUDED__

#include "client\windows\handler\exception_handler.h"

//*****************************************************************************
//*****************************************************************************
struct CallbackContext
{
    std::wstring name;
    std::wstring version;
    std::wstring mailto;
    std::wstring temppath;

    CallbackContext()
    {
    }

    CallbackContext(const std::wstring & _name,
                    const std::wstring & _version,
                    const std::wstring & _mailto,
                    const std::wstring & _temppath)
    {
        name     = _name;
        version  = _version;
        mailto   = _mailto;
        temppath = _temppath;
    }
};

//*****************************************************************************
//*****************************************************************************
class ExceptionHandler
{
public:
    static ExceptionHandler & instance();

    void init(const std::wstring & _path, const std::wstring & _name,
              const std::wstring & _version, const std::wstring & _mailto);
    void writeMinidump();

protected:
    static struct CallbackContext m_context;

protected:
    ExceptionHandler(void)  {}
    ~ExceptionHandler(void) {}

protected:
    static const std::wstring temppath;
    static google_breakpad::ExceptionHandler * m_handler;
};

#endif // __EXCEPTION_HANDLER_H_INCLUDED__
