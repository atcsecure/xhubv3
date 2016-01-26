//*****************************************************************************
//*****************************************************************************

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "util/logger.h"
#include "util/settings.h"

#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
XBridge::XBridge()
    : m_timerIoWork(m_timerIo)
    , m_timerThread(boost::bind(&boost::asio::io_service::run, &m_timerIo))
    , m_timer(m_timerIo, boost::posix_time::seconds(TIMER_INTERVAL))
{
    try
    {
        // services and threas
        for (int i = 0; i < THREAD_COUNT; ++i)
        {
            IoServicePtr ios(new boost::asio::io_service);

            m_services.push_back(ios);
            m_works.push_back(boost::asio::io_service::work(*ios));

            m_threads.create_thread(boost::bind(&boost::asio::io_service::run, ios));
        }

        m_timer.async_wait(boost::bind(&XBridge::onTimer, this));

        // sessions
        {
            Settings & s = settings();
            std::vector<std::string> wallets = s.exchangeWallets();
            for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
            {
                // std::string label   = s.get<std::string>(*i + ".Title");
                // std::string address = s.get<std::string>(*i + ".Address");
                std::string ip       = s.get<std::string>(*i + ".Ip");
                std::string port     = s.get<std::string>(*i + ".Port");
                std::string user     = s.get<std::string>(*i + ".Username");
                std::string passwd   = s.get<std::string>(*i + ".Password");
                boost::uint64_t COIN = s.get<boost::uint64_t>(*i + ".COIN", 0);

                if (ip.empty() || port.empty() || user.empty() || passwd.empty() || COIN == 0)
                {
                    LOG() << "read wallet " << *i << " with empty parameters>";
                    continue;
                }

                XBridgeSessionPtr session(new XBridgeSession(*i, ip, port, user, passwd, COIN));
                session->requestAddressBook();
            }
        }
    }
    catch (std::exception & e)
    {
        ERR() << e.what();
        ERR() << __FUNCTION__;
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridge::run()
{
    m_threads.join_all();
}

//*****************************************************************************
//*****************************************************************************
void XBridge::stop()
{
    m_timer.cancel();
    m_timerIo.stop();

    for (auto i = m_services.begin(); i != m_services.end(); ++i)
    {
        (*i)->stop();
    }
}

//******************************************************************************
//******************************************************************************
void XBridge::onTimer()
{
    // DEBUG_TRACE();

    {
        m_services.push_back(m_services.front());
        m_services.pop_front();

        XBridgeSessionPtr session(new XBridgeSession);

        IoServicePtr io = m_services.front();

        // call check expired transactions
        io->post(boost::bind(&XBridgeSession::checkFinishedTransactions, session));

        // send list of wallets (broadcast)
        io->post(boost::bind(&XBridgeSession::sendListOfWallets, session));

        // send transactions list
        io->post(boost::bind(&XBridgeSession::sendListOfTransactions, session));

        // send transactions list
        io->post(boost::bind(&XBridgeSession::eraseExpiredPendingTransactions, session));

        // resend addressbook
        // io->post(boost::bind(&XBridgeSession::resendAddressBook, session));
        io->post(boost::bind(&XBridgeSession::getAddressBook, session));
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&XBridge::onTimer, this));
}
