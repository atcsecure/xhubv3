//*****************************************************************************
//*****************************************************************************

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "util/logger.h"

#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
XBridge::XBridge(const unsigned short port)
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

        // listener
        if (port)
        {
            // boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), port);
            boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), port);

            m_acceptor = std::shared_ptr<boost::asio::ip::tcp::acceptor>
                            (new boost::asio::ip::tcp::acceptor
                                    (*m_services.front(), ep));

            LOG() << "xbridge service listen at port " << port;
        }

        m_timer.async_wait(boost::bind(&XBridge::onTimer, this));

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
    listen();
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

//*****************************************************************************
//*****************************************************************************
void XBridge::listen()
{
    if (m_services.size() > 0 && m_acceptor)
    {
        m_services.push_back(m_services.front());
        m_services.pop_front();

        SocketPtr socket(new Socket(*m_services.front()));
        m_acceptor->async_accept(*socket,
                                 boost::bind(&XBridge::accept,
                                             this, socket,
                                             boost::asio::placeholders::error));
    }
}

//******************************************************************************
//******************************************************************************
void XBridge::accept(XBridge::SocketPtr socket,
                     const boost::system::error_code & error)
{
    if (error)
    {
        ERR() << "xbridge failed to accept TCP connection";
        return;
    }

    // listen next
    listen();

    // create session for client
    XBridgeSessionPtr session(new XBridgeSession);
    session->start(socket);
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
        io->post(boost::bind(&XBridgeSession::resendAddressBook, session));
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&XBridge::onTimer, this));
}
