//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGE_H
#define XBRIDGE_H

#include <deque>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridge
{
    enum
    {
        THREAD_COUNT = 2,
        LISTEN_PORT = 30330
    };

    typedef std::shared_ptr<boost::asio::io_service>      IoServicePtr;

public:
    typedef boost::asio::ip::tcp::socket                  Socket;
    typedef std::shared_ptr<boost::asio::ip::tcp::socket> SocketPtr;

public:
    XBridge();

    void run();
    void stop();

private:
    void listen();

    void accept(XBridge::SocketPtr socket,
                const boost::system::error_code & error);

private:
    std::deque<IoServicePtr>                        m_services;
    std::deque<boost::asio::io_service::work>       m_works;
    boost::thread_group                             m_threads;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
};

#endif // XBRIDGE_H
