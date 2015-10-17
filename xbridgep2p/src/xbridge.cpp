//*****************************************************************************
//*****************************************************************************

#include "xbridge.h"
#include "xbridgesession.h"
#include "logger.h"

//*****************************************************************************
//*****************************************************************************
XBridge::XBridge()
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
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), LISTEN_PORT);
        m_acceptor = std::shared_ptr<boost::asio::ip::tcp::acceptor>
                        (new boost::asio::ip::tcp::acceptor
                                (*m_services.front(), ep));

        LOG() << "xbridge service listen at port " << LISTEN_PORT;
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
