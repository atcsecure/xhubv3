//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/util.h"
#include "util/settings.h"
#include "version.h"

#ifdef BREAKPAD_ENABLED
#include "ExceptionHandler.h"
#endif

//*****************************************************************************
//*****************************************************************************
int main(int argc, char *argv[])
{
    Settings & s = settings();

    {
        std::string path(*argv);
        std::string::size_type pos = path.rfind(".");
        if (pos != std::string::npos)
        {
            path = path.substr(0, pos);
        }
        path += ".conf";

        s.read(path.c_str());
        s.parseCmdLine(argc, argv);
    }

    util::init();

    XBridgeApp & a = XBridgeApp::instance();
    a.init(argc, argv);

#ifdef BREAKPAD_ENABLED
    std::wstring name(L"xbridgep2p.exe");
    std::wstring mailto(L"xbridge_bugs@aktivsystems.ru");

    {
        if (s.logPath().length() > 0)
        {
            ExceptionHandler & e = ExceptionHandler::instance();
            e.init(util::wide_string(s.logPath()),
                   name,
                   util::wide_string(a.version()),
                   mailto);
        }
    }
#endif

    // init xbridge network
    a.initDht();

    // init exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    e.init();

    int retcode = a.exec();

    // stop
    a.stopDht();

    return retcode;
}
