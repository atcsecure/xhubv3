//******************************************************************************
//******************************************************************************

#include "logger.h"
#include "settings.h"
#include "../uiconnector.h"

#include <string>
#include <fstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

boost::mutex logLocker;

//******************************************************************************
//******************************************************************************
LOG::LOG(const char reason)
    : std::basic_stringstream<char, std::char_traits<char>,
                    boost::pool_allocator<char> >()
    , m_r(reason)
{
    *this << "\n" << "[" << (char)std::toupper(m_r) << "] "
          << boost::posix_time::second_clock::local_time()
          << " [0x" << boost::this_thread::get_id() << "] ";
}

//******************************************************************************
//******************************************************************************
LOG::~LOG()
{
    boost::mutex::scoped_lock l(logLocker);

    // const static std::string path     = settings().logPath().size() ? settings().logPath() : settings().appPath();
    const static bool logToFile       = true; // !path.empty();
    static boost::gregorian::date day =
            boost::gregorian::day_clock::local_day();
    static std::string logFileName    = makeFileName();

    std::cout << str().c_str();

    try
    {
        uiConnector.NotifyLogMessage(str().c_str());

        if (logToFile)
        {
            boost::gregorian::date tmpday =
                    boost::gregorian::day_clock::local_day();

            if (day != tmpday)
            {
                logFileName = makeFileName();
                day = tmpday;
            }

            std::ofstream file(logFileName.c_str(), std::ios_base::app);
            file << str().c_str();
        }
    }
    catch (std::exception &)
    {
    }
}

//******************************************************************************
//******************************************************************************
const std::string LOG::makeFileName() const
{
    const static std::string path     = settings().logPath().size() ?
                                        settings().logPath() :
                                        settings().appPath();

    return path + "/xbridgep2p_" +
            boost::gregorian::to_iso_string(boost::gregorian::day_clock::local_day()) + ".log";
}
