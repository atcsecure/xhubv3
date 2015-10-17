//*****************************************************************************
//*****************************************************************************

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "logger.h"
#include "dht/dht.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

//******************************************************************************
//******************************************************************************
struct PrintErrorCode
{
    const boost::system::error_code & error;

    explicit PrintErrorCode(const boost::system::error_code & e) : error(e) {}

    friend std::ostream & operator<<(std::ostream & out, const PrintErrorCode & e)
    {
        return out << " ERROR <" << e.error.value() << "> " << e.error.message();
    }
};

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession()
{
    m_processors[xbcInvalid]          .bind(this, &XBridgeSession::processInvalid);
    m_processors[xbcAnnounceAddresses].bind(this, &XBridgeSession::processAnnounceAddresses);
    m_processors[xbcXChatMessage]     .bind(this, &XBridgeSession::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::start(XBridge::SocketPtr socket)
{
    DEBUG_TRACE();

    LOG() << "client connected " << socket.get();

    m_socket = socket;

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::disconnect()
{
    DEBUG_TRACE();

    m_socket->close();

    LOG() << "client disconnected " << m_socket.get();

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageClean(shared_from_this());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset)
{
    DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->header()+offset,
                                    packet->headerSize-offset),
                boost::bind(&XBridgeSession::onReadHeader,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset,
                                  const boost::system::error_code & error,
                                  std::size_t transferred)
{
    DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != packet->headerSize)
    {
        LOG() << "partially read header, read " << transferred
              << " of " << packet->headerSize << " bytes";

        doReadHeader(packet, offset + transferred);
        return;
    }

    packet->alloc();
    doReadBody(packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadBody(XBridgePacketPtr packet,
                const std::size_t offset)
{
    DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->data()+offset,
                                    packet->size()-offset),
                boost::bind(&XBridgeSession::onReadBody,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadBody(XBridgePacketPtr packet,
                                const std::size_t offset,
                                const boost::system::error_code & error,
                                std::size_t transferred = 0)
{
    DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != packet->size())
    {
        LOG() << "partially read packet, read " << transferred
              << " of " << packet->size() << " bytes";

        doReadBody(packet, offset + transferred);
        return;
    }

    if (!decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return;
    }

    if (!processPacket(packet))
    {
        ERR() << "packet processing error " << __FUNCTION__;
    }

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::encryptPacket(XBridgePacketPtr /*packet*/)
{
    DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::decryptPacket(XBridgePacketPtr /*packet*/)
{
    DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processPacket(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    XBridgeCommand c = packet->command();

    if (m_processors.count(c) == 0)
    {
        m_processors[xbcInvalid](packet);
        ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
        return false;
    }

    if (!m_processors[c](packet))
    {
        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processInvalid(XBridgePacketPtr /*packet*/)
{
    DEBUG_TRACE();
    LOG() << "xbcInvalid command processed";
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAnnounceAddresses(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be 20 bytes (160bit)
    if (packet->size() != 20)
    {
        ERR() << "invalid packet size for xbcAnnounceAddresses " << __FUNCTION__;
        return false;
    }

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXChatMessage(const std::vector<unsigned char> & message)
{
    DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket(xbcXChatMessage));
    packet->setData(message);

    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processXChatMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 20 bytes (160bit)
    if (packet->size() <= 20)
    {
        ERR() << "invalid packet size for xbcXChatMessage " << __FUNCTION__;
        return false;
    }

    // read dest address
    std::vector<unsigned char> daddr(packet->data(), packet->data() + 20);

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->onSend(daddr, std::vector<unsigned char>(packet->data(), packet->data()+packet->size()));

    return true;
}
