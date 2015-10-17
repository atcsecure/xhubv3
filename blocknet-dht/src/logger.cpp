//******************************************************************************
//******************************************************************************

#include "logger.h"
//#include "settings.h"

#include <string>
#include <fstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <QDebug>

boost::mutex logLocker;

//******************************************************************************
//******************************************************************************
LOG::LOG()
    : std::basic_stringstream<char, std::char_traits<char>,
                    boost::pool_allocator<char> >()
{
    *this << "\n" << boost::posix_time::second_clock::local_time()
            << " [0x" << boost::this_thread::get_id() << "] ";
}

//******************************************************************************
//******************************************************************************
LOG::~LOG()
{
    boost::mutex::scoped_lock l(logLocker);

    // const static std::string path     = settings().logPath();
    // const static bool logToFile       = !path.empty();
    static boost::gregorian::date day =
            boost::gregorian::day_clock::local_day();
    // static std::string logFileName    = makeFileName();

    // std::cout << str().c_str();
    qDebug() << str().c_str();

//    try
//    {
//        if (logToFile)
//        {
//            boost::gregorian::date tmpday =
//                    boost::gregorian::day_clock::local_day();

//            if (day != tmpday)
//            {
//                logFileName = makeFileName();
//                day = tmpday;
//            }

//            std::ofstream file(logFileName.c_str(), std::ios_base::app);
//            file << str().c_str();
//        }
//    }
//    catch (std::exception &)
//    {
//    }
}

//******************************************************************************
//******************************************************************************
//const std::string LOG::makeFileName() const
//{
//    return settings().logPath() + "/pswrtool_" +
//            boost::gregorian::to_iso_string(boost::gregorian::day_clock::local_day()) + ".log";
//}
