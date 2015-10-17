//*****************************************************************************
//*****************************************************************************

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/util.h"
#include "util/logger.h"
#include "dht/dht.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

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
    // process invalid
    m_processors[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSession::processAnnounceAddresses);

    // process transaction from client wallet
    m_processors[xbcTransaction]           .bind(this, &XBridgeSession::processTransaction);

    // transaction processing
    {
        m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSession::processTransactionHoldApply);
        m_processors[xbcTransactionInitialized].bind(this, &XBridgeSession::processTransactionInitialized);
        m_processors[xbcTransactionCreated]    .bind(this, &XBridgeSession::processTransactionCreated);
        m_processors[xbcTransactionSigned]     .bind(this, &XBridgeSession::processTransactionSigned);
        m_processors[xbcTransactionCommited]   .bind(this, &XBridgeSession::processTransactionCommited);
        // m_processors[xbcTransactionConfirmed]  .bind(this, &XBridgeSession::processTransactionConfirmed);
        m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSession::processTransactionCancel);

        // wallet received transaction
        m_processors[xbcReceivedTransaction]   .bind(this, &XBridgeSession::processBitcoinTransactionHash);
    }

    m_processors[xbcAddressBookEntry].bind(this, &XBridgeSession::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage].bind(this, &XBridgeSession::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::start(XBridge::SocketPtr socket)
{
    // DEBUG_TRACE();

    LOG() << "client connected " << socket.get();

    m_socket = socket;

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::disconnect()
{
    // DEBUG_TRACE();

    m_socket->close();

    LOG() << "client disconnected " << m_socket.get();

    XBridgeApp & app = XBridgeApp::instance();
    app.storageClean(shared_from_this());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset)
{
    // DEBUG_TRACE();

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
    // DEBUG_TRACE();

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

    if (!checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        disconnect();
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
    // DEBUG_TRACE();

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
    // DEBUG_TRACE();

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

    if (!processPacket(packet))
    {
        ERR() << "packet processing error " << __FUNCTION__;
    }

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
const unsigned char * XBridgeSession::myaddr() const
{
    XBridgeApp & app = XBridgeApp::instance();
    return app.myid();
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendPacket(const std::vector<unsigned char> & to,
                                XBridgePacketPtr packet)
{
    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(to, packet->body());
}

//*****************************************************************************
// return true if packet not for me, relayed
//*****************************************************************************
bool XBridgeSession::relayPacket(XBridgePacketPtr packet)
{
    if (packet->size() < 20)
    {
        return false;
    }

    // check address
    XBridgeApp & app = XBridgeApp::instance();
    if (memcmp(packet->data(), app.myid(), 20) != 0)
    {
        // not for me, retranslate packet
        std::vector<unsigned char> addr(packet->data(), packet->data() + 20);
        app.onSend(addr, packet);
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processPacket(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    if (!decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_processors.count(c) == 0)
    {
        m_processors[xbcInvalid](packet);
        // ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
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
bool XBridgeSession::processInvalid(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    LOG() << "xbcInvalid instead of " << packet->command();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processZero(XBridgePacketPtr /*packet*/)
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAnnounceAddresses(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be 20 bytes (160bit)
    if (packet->size() != 20)
    {
        ERR() << "invalid packet size for xbcAnnounceAddresses " << __FUNCTION__;
        return false;
    }

    XBridgeApp & app = XBridgeApp::instance();
    app.storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool XBridgeSession::checkXBridgePacketVersion(XBridgePacketPtr packet)
{
    if (packet->version() != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(XBridgePacketPtr packet)
{
    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(const std::vector<unsigned char> & message)
{
    // DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket());
    // packet->setData(message);
    packet->copyFrom(message);

    return sendXBridgeMessage(packet);
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
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

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(daddr, std::vector<unsigned char>(packet->header(), packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
void XBridgeSession::sendPacketBroadcast(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(packet);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransaction(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be 104 bytes
    if (packet->size() != 104)
    {
        ERR() << "invalid packet size for xbcTransaction " << __FUNCTION__;
        return false;
    }

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        // read packet data
        uint256 id(packet->data());

        // source
        std::vector<unsigned char> saddr(packet->data()+32, packet->data()+52);
        std::string scurrency((const char *)packet->data()+52);
        boost::uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+60));

        // destination
        std::vector<unsigned char> daddr(packet->data()+68, packet->data()+88);
        std::string dcurrency((const char *)packet->data()+88);
        boost::uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+96));

        LOG() << "received transaction " << util::base64_encode(std::string((char *)id.begin(), 32)) << std::endl
              << "    from " << util::base64_encode(std::string((char *)&saddr[0], 20)) << std::endl
              << "             " << scurrency << " : " << samount << std::endl
              << "    to   " << util::base64_encode(std::string((char *)&daddr[0], 20)) << std::endl
              << "             " << dcurrency << " : " << damount << std::endl;

        if (!e.haveConnectedWallet(scurrency) || !e.haveConnectedWallet(dcurrency))
        {
            LOG() << "no active wallet for transaction "
                  << util::base64_encode(std::string((char *)id.begin(), 32));
        }
        else
        {
            // float rate = (float) destAmount / sourceAmount;
            uint256 transactionId;
            if (e.createTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, transactionId))
            {
                // check transaction state, if trNew - do nothing,
                // if trJoined = send hold to client
                XBridgeTransactionPtr tr = e.transaction(transactionId);

                boost::mutex::scoped_lock l(tr->m_lock);

                if (tr && tr->state() == XBridgeTransaction::trJoined)
                {
                    // send hold to clients
                    XBridgeApp & app = XBridgeApp::instance();

                    // first
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to "
                          << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

                    XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                    reply1->append(tr->firstAddress());
                    reply1->append(app.myid(), 20);
                    reply1->append(tr->firstId().begin(), 32);
                    reply1->append(transactionId.begin(), 32);

                    app.onSend(tr->firstAddress(),
                               std::vector<unsigned char>(reply1->header(),
                                                          reply1->header()+reply1->allSize()));

                    // second
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to "
                          << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

                    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionHold));
                    reply2->append(tr->secondAddress());
                    reply2->append(app.myid(), 20);
                    reply2->append(tr->secondId().begin(), 32);
                    reply2->append(transactionId.begin(), 32);

                    app.onSend(tr->secondAddress(),
                               std::vector<unsigned char>(reply2->header(),
                                                          reply2->header()+reply2->allSize()));
                }
            }
        }
    }

    // ..and retranslate
    sendPacketBroadcast(packet);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be eq 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply " << __FUNCTION__;
        return false;
    }

    // check is for me
    if (relayPacket(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (e.updateTransactionWhenHoldApplyReceived(tr, from))
    {
        if (tr->state() == XBridgeTransaction::trHold)
        {
            // send initialize transaction command to clients

            // field length must be 8 bytes
            std::string firstCurrency = tr->firstCurrency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->secondCurrency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << util::base64_encode(std::string((char *)&tr->firstDestination()[0], 20));

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(tr->firstDestination());
            reply1->append(myaddr(), 20);
            reply1->append(id.begin(), 32);
            reply1->append(tr->firstAddress());
            reply1->append(fc);
            reply1->append(tr->firstAmount());
            reply1->append(tr->firstDestination());
            reply1->append(sc);
            reply1->append(tr->secondAmount());

            sendPacket(tr->firstDestination(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << util::base64_encode(std::string((char *)&tr->secondDestination()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(tr->secondDestination());
            reply2->append(myaddr(), 20);
            reply2->append(id.begin(), 32);
            reply2->append(tr->secondAddress());
            reply2->append(sc);
            reply2->append(tr->secondAmount());
            reply2->append(tr->secondDestination());
            reply2->append(fc);
            reply2->append(tr->firstAmount());

            sendPacket(tr->secondDestination(), reply2);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionInitialized(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be eq 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply " << __FUNCTION__;
        return false;
    }

    // check is for me
    if (relayPacket(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (e.updateTransactionWhenInitializedReceived(tr, from))
    {
        if (tr->state() == XBridgeTransaction::trInitialized)
        {
            // send create transaction command to clients

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

            // send xbcTransactionCreate
            // with nLockTime == TTL*2 for first client,
            // with nLockTime == TTL*4 for second
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionCreate));
            reply1->append(tr->firstAddress());
            reply1->append(myaddr(), 20);
            reply1->append(id.begin(), 32);
            reply1->append(tr->secondDestination());
            reply1->append((boost::uint32_t)(XBridgeTransaction::TTL * 2));
            reply1->append((boost::uint32_t)24*60*60);

            sendPacket(tr->firstAddress(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreate));
            reply2->append(tr->secondAddress());
            reply2->append(myaddr(), 20);
            reply2->append(id.begin(), 32);
            reply2->append(tr->firstDestination());
            reply2->append((boost::uint32_t)(XBridgeTransaction::TTL * 4));
            reply2->append((boost::uint32_t)48*60*60);

            sendPacket(tr->secondAddress(), reply2);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCreated(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        ERR() << "invalid packet size for xbcTransactionCreated " << __FUNCTION__;
        return false;
    }

    // check is for me
    if (relayPacket(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    size_t offset = 20;

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string rawpaytx(reinterpret_cast<const char *>(packet->data()+offset));
    offset += rawpaytx.size()+1;

    std::string rawrevtx(reinterpret_cast<const char *>(packet->data()+offset));

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (e.updateTransactionWhenCreatedReceived(tr, from, rawpaytx, rawrevtx))
    {
        if (tr->state() == XBridgeTransaction::trCreated)
        {
            // send packets for sign transaction

            // TODO remove this log
            LOG() << "send xbcTransactionSign to "
                  << util::base64_encode(std::string((char *)&tr->firstDestination()[0], 20));

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionSign));
            reply->append(tr->firstDestination());
            reply->append(myaddr(), 20);
            reply->append(txid.begin(), 32);
            reply->append(tr->secondRawPayTx());
            reply->append(tr->secondRawRevTx());

            sendPacket(tr->firstDestination(), reply);

            // TODO remove this log
            LOG() << "send xbcTransactionSign to "
                  << util::base64_encode(std::string((char *)&tr->secondDestination()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionSign));
            reply2->append(tr->secondDestination());
            reply2->append(myaddr(), 20);
            reply2->append(txid.begin(), 32);
            reply2->append(tr->firstRawPayTx());
            reply2->append(tr->firstRawRevTx());

            sendPacket(tr->secondDestination(), reply2);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionSigned(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        ERR() << "invalid packet size for xbcTransactionSigned " << __FUNCTION__;
        return false;
    }

    // check is for me
    if (relayPacket(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);

    std::string rawtx(reinterpret_cast<const char *>(packet->data()+72));

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (e.updateTransactionWhenSignedReceived(tr, from, rawtx))
    {
        if (tr->state() == XBridgeTransaction::trSigned)
        {
            // send signed transactions to clients

            // TODO remove this log
            LOG() << "send xbcTransactionCommit to "
                  << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommit));
            reply->append(tr->firstAddress());
            reply->append(myaddr(), 20);
            reply->append(txid.begin(), 32);
            reply->append(tr->firstRawRevTx());

            sendPacket(tr->firstAddress(), reply);

            // TODO remove this log
            LOG() << "send xbcTransactionCommit to "
                  << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCommit));
            reply2->append(tr->secondAddress());
            reply2->append(myaddr(), 20);
            reply2->append(txid.begin(), 32);
            reply2->append(tr->secondRawRevTx());

            sendPacket(tr->secondAddress(), reply2);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCommited(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be == 104 bytes
    if (packet->size() != 104)
    {
        ERR() << "invalid packet size for xbcTransactionCommited " << __FUNCTION__;
        return false;
    }

    // check is for me
    if (relayPacket(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);
    uint256 txhash(packet->data()+72);

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (e.updateTransactionWhenCommitedReceived(tr, from, txhash))
    {
        if (tr->state() == XBridgeTransaction::trCommited)
        {
            // transaction commited, wait for confirm
            LOG() << "commit transaction, wait for confirm. id <"
                  << txid.GetHex() << ">" << std::endl
                  << "    hash <" << tr->firstTxHash().GetHex() << ">" << std::endl
                  << "    hash <" << tr->secondTxHash().GetHex() << ">";

            // send confirm request to clients

//            // TODO remove this log
//            LOG() << "send xbcTransactionCommit to "
//                  << util::base64_encode(std::string((char *)&tr->firstDestination()[0], 20));

//            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirm));
//            reply->append(tr->firstDestination());
//            reply->append(myaddr(), 20);
//            reply->append(txid.begin(), 32);
//            reply->append(tr->secondTxHash().begin(), 32);

//            sendPacket(tr->firstDestination(), reply);

//            // TODO remove this log
//            LOG() << "send xbcTransactionCommit to "
//                  << util::base64_encode(std::string((char *)&tr->secondDestination()[0], 20));

//            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirm));
//            reply2->append(tr->secondDestination());
//            reply2->append(myaddr(), 20);
//            reply2->append(txid.begin(), 32);
//            reply2->append(tr->firstTxHash().begin(), 32);

//            sendPacket(tr->secondDestination(), reply2);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeSession::processTransactionConfirmed(XBridgePacketPtr packet)
//{
//    DEBUG_TRACE();

//    // size must be == 72 bytes
//    if (packet->size() != 72)
//    {
//        ERR() << "invalid packet size for xbcTransactionCommited " << __FUNCTION__;
//        return false;
//    }

//    // check is for me
//    if (relayPacket(packet))
//    {
//        return true;
//    }

//    XBridgeExchange & e = XBridgeExchange::instance();

//    uint256 txid(packet->data()+40);

//    XBridgeTransactionPtr tr = e.transaction(txid);
//    boost::mutex::scoped_lock l(tr->m_lock);

//    tr->updateTimestamp();

//    if (e.updateTransactionWhenConfirmedReceived(tr))
//    {
//        if (tr->state() == XBridgeTransaction::trFinished)
//        {
//            // send transaction state to clients
//            std::vector<std::vector<unsigned char> > rcpts;
//            rcpts.push_back(tr->firstAddress());
//            rcpts.push_back(tr->firstDestination());
//            rcpts.push_back(tr->secondAddress());
//            rcpts.push_back(tr->secondDestination());

//            foreach (const std::vector<unsigned char> & to, rcpts)
//            {
//                // TODO remove this log
//                LOG() << "send xbcTransactionFinished to "
//                      << util::base64_encode(std::string((char *)&to[0], 20));

//                XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
//                reply->append(to);
//                reply->append(txid.begin(), 32);

//                sendPacket(to, reply);
//            }
//        }
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCancel(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 32 bytes
    if (packet->size() != 32)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction " << __FUNCTION__;
        return false;
    }

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        uint256 txid(packet->data());

        e.deletePendingTransactions(txid);

        // send cancel to clients
        // XBridgeTransactionPtr tr = e.transaction(txid);
        // if (tr->state() != XBridgeTransaction::trInvalid)
        {
            // boost::mutex::scoped_lock l(tr->m_lock);
            // sendCancelTransaction(tr);
            sendCancelTransaction(txid);
        }
    }

    // ..and retranslate
    sendPacketBroadcast(packet);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::finishTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "finish transaction <" << tr->id().GetHex() << ">";

    if (tr->state() != XBridgeTransaction::trConfirmed)
    {
        ERR() << "finished unconfirmed transaction <" << tr->id().GetHex() << ">";
        return false;
    }

//    std::vector<std::vector<unsigned char> > rcpts;
//    rcpts.push_back(tr->firstAddress());
//    rcpts.push_back(tr->firstDestination());
//    rcpts.push_back(tr->secondAddress());
//    rcpts.push_back(tr->secondDestination());

//    foreach (const std::vector<unsigned char> & to, rcpts)
    {
        // TODO remove this log
//        LOG() << "send xbcTransactionFinished to "
//              << util::base64_encode(std::string((char *)&to[0], 20));

        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
        // reply->append(to);
        reply->append(tr->id().begin(), 32);

        // sendPacket(to, reply);
        sendPacketBroadcast(reply);
    }

    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendCancelTransaction(const uint256 & txid)
{
    LOG() << "cancel transaction <" << txid.GetHex() << ">";

    // if (tr->state() != XBridgeTransaction::trNew)
    {
        // std::vector<std::vector<unsigned char> > rcpts;
        // rcpts.push_back(tr->firstAddress());
        // rcpts.push_back(tr->firstDestination());
        // rcpts.push_back(tr->secondAddress());
        // rcpts.push_back(tr->secondDestination());

        // foreach (const std::vector<unsigned char> & to, rcpts)
        {
            // TODO remove this log
            // LOG() << "send xbcTransactionCancel to "
            //       << util::base64_encode(std::string((char *)&to[0], 20));

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
            // reply->append(to);
            reply->append(txid.begin(), 32);

            // sendPacket(to, reply);
            sendPacketBroadcast(reply);
        }
    }

    // tr->cancel();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::rollbackTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "rollback transaction <" << tr->id().GetHex() << ">";

    std::vector<std::vector<unsigned char> > rcpts;

    if (tr->state() >= XBridgeTransaction::trSigned)
    {
        rcpts.push_back(tr->firstAddress());
        rcpts.push_back(tr->secondAddress());
    }

    std::vector<std::vector<unsigned char> >::const_iterator i = rcpts.begin();
    for (; i != rcpts.end(); ++i)
    {
        const std::vector<unsigned char> & to = *i;

        // TODO remove this log
        LOG() << "send xbcTransactionRollback to "
              << util::base64_encode(std::string((char *)&to[0], 20));

        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRollback));
        reply->append(to);
        reply->append(tr->id().begin(), 32);

        sendPacket(to, reply);
    }

    sendCancelTransaction(tr->id());
    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processBitcoinTransactionHash(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 32 bytes (256bit)
    if (packet->size() != 32)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction " << __FUNCTION__;
        return false;
    }

    static XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    uint256 id(packet->data());
//    // LOG() << "received transaction <" << id.GetHex() << ">";

    e.updateTransaction(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAddressBookEntry(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    std::string currency(reinterpret_cast<const char *>(packet->data()));
    std::string name(reinterpret_cast<const char *>(packet->data()+currency.length()+1));
    std::string address(reinterpret_cast<const char *>(packet->data()+currency.length()+name.length()+2));

    XBridgeApp::instance().storeAddressBookEntry(currency, name, address);

    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfWallets()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::vector<StringPair> wallets = e.listOfWallets();
    std::vector<std::string> list;
    for (std::vector<StringPair>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        list.push_back(i->first + '|' + i->second);
    }

    XBridgePacketPtr packet(new XBridgePacket(xbcExchangeWallets));
    packet->setData(boost::algorithm::join(list, "|"));

    sendPacket(std::vector<unsigned char>(), packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        XBridgePacketPtr packet(new XBridgePacket(xbcPendingTransaction));

        // field length must be 8 bytes
        std::vector<unsigned char> fc(8, 0);
        std::string tmp = ptr->firstCurrency();
        std::copy(tmp.begin(), tmp.end(), fc.begin());

        // field length must be 8 bytes
        std::vector<unsigned char> tc(8, 0);
        tmp = ptr->secondCurrency();
        std::copy(tmp.begin(), tmp.end(), tc.begin());

        packet->append(ptr->id().begin(), 32);
        // packet->append(ptr->firstAddress());
        packet->append(fc);
        packet->append(ptr->firstAmount());
        // packet->append(ptr->firstDestination());
        packet->append(tc);
        packet->append(ptr->secondAmount());
        // packet->append(static_cast<boost::uint32_t>(ptr->state()));

        sendPacket(std::vector<unsigned char>(), packet);
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::eraseExpiredPendingTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        if (ptr->isExpired())
        {
            LOG() << "transaction expired <" << ptr->id().GetHex() << ">";
            e.deletePendingTransactions(ptr->hash1());
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::checkFinishedTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.finishedTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        uint256 txid = ptr->id();

        if (ptr->state() == XBridgeTransaction::trConfirmed)
        {
            // send finished
            LOG() << "confirmed transaction <" << txid.GetHex() << ">";
            finishTransaction(ptr);
        }
        else if (ptr->state() == XBridgeTransaction::trCancelled)
        {
            // drop cancelled tx
            LOG() << "drop cancelled transaction <" << txid.GetHex() << ">";
            ptr->drop();
        }
        else if (ptr->state() == XBridgeTransaction::trFinished)
        {
            // delete finished tx
            LOG() << "delete finished transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (ptr->state() == XBridgeTransaction::trDropped)
        {
            // delete dropped tx
            LOG() << "delete dropped transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (!ptr->isValid())
        {
            // delete invalid tx
            LOG() << "delete invalid transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else
        {
            LOG() << "timeout transaction <" << txid.GetHex() << ">"
                  << " state " << ptr->strState();

            // send rollback
            rollbackTransaction(ptr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::resendAddressBook()
{
    XBridgeApp::instance().resendAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendAddressbookEntry(const std::string & currency,
                                          const std::string & name,
                                          const std::string & address)
{
    if (m_socket->is_open())
    {
        XBridgePacketPtr p(new XBridgePacket(xbcAddressBookEntry));
        p->append(currency);
        p->append(name);
        p->append(address);

        sendXBridgeMessage(p);
    }
}
