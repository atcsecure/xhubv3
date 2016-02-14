//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "uiconnector.h"
#include "util/util.h"
#include "util/logger.h"
#include "dht/dht.h"
#include "bitcoinrpc.h"
#include "ctransaction.h"
#include "base58.h"

//******************************************************************************
//******************************************************************************
// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp.
// Tue Nov  5 00:53:20 1985 UTC
static const unsigned int LOCKTIME_THRESHOLD = 500000000;

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
std::string txToString(const CTransaction & tx);
CTransaction txFromString(const std::string & str);

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession(const std::string & currency,
                               const std::string & address,
                               const std::string & port,
                               const std::string & user,
                               const std::string & passwd,
                               const std::string & prefix,
                               const boost::uint64_t & COIN)
    : m_currency(currency)
    , m_address(address)
    , m_port(port)
    , m_user(user)
    , m_passwd(passwd)
    , m_prefix(prefix)
    , m_COIN(COIN)
{
    init();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::init()
{
    assert(!m_processors.size());

    // process invalid
    m_processors[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSession::processAnnounceAddresses);

    // process transaction from client wallet
    m_processors[xbcTransaction]           .bind(this, &XBridgeSession::processTransaction);
    m_processors[xbcPendingTransaction]    .bind(this, &XBridgeSession::processPendingTransaction);

    // transaction processing
    {
        m_processors[xbcTransactionHold]       .bind(this, &XBridgeSession::processTransactionHold);
        m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSession::processTransactionHoldApply);

        m_processors[xbcTransactionInit]       .bind(this, &XBridgeSession::processTransactionInit);
        m_processors[xbcTransactionInitialized].bind(this, &XBridgeSession::processTransactionInitialized);

        m_processors[xbcTransactionCreate]     .bind(this, &XBridgeSession::processTransactionCreate);
        m_processors[xbcTransactionCreated]    .bind(this, &XBridgeSession::processTransactionCreated);

        m_processors[xbcTransactionSign]       .bind(this, &XBridgeSession::processTransactionSign);
        m_processors[xbcTransactionSigned]     .bind(this, &XBridgeSession::processTransactionSigned);

        m_processors[xbcTransactionCommit]     .bind(this, &XBridgeSession::processTransactionCommit);
        m_processors[xbcTransactionCommited]   .bind(this, &XBridgeSession::processTransactionCommited);

        m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSession::processTransactionCancel);
        m_processors[xbcTransactionRollback]   .bind(this, &XBridgeSession::processTransactionRollback);
        m_processors[xbcTransactionFinished]   .bind(this, &XBridgeSession::processTransactionFinished);
        m_processors[xbcTransactionDropped]    .bind(this, &XBridgeSession::processTransactionDropped);

        // m_processors[xbcTransactionConfirmed]  .bind(this, &XBridgeSession::processTransactionConfirmed);

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

    // return sendXBridgeMessage(packet);
    return processPacket(packet);
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
bool XBridgeSession::sendPacketBroadcast(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(packet);

    return true;
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

    // source
    std::vector<unsigned char> saddr(packet->data()+32, packet->data()+52);
    std::string scurrency((const char *)packet->data()+52);
    boost::uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+60));

    // destination
    std::vector<unsigned char> daddr(packet->data()+68, packet->data()+88);
    std::string dcurrency((const char *)packet->data()+88);
    boost::uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+96));

    {
        XBridgeTransactionDescr d;
        d.id           = uint256(packet->data());
        d.fromCurrency = scurrency;
        d.fromAmount   = samount;
        d.toCurrency   = dcurrency;
        d.toAmount     = damount;
        d.state        = XBridgeTransactionDescr::trPending;

        uiConnector.NotifyXBridgePendingTransactionReceived(d);
    }

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        // read packet data
        uint256 id(packet->data());

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

                    // first
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to "
                          << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

                    XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                    reply1->append(tr->firstAddress());
                    reply1->append(myaddr(), 20);
                    reply1->append(tr->firstId().begin(), 32);
                    reply1->append(transactionId.begin(), 32);

                    sendPacket(tr->firstAddress(), reply1);

                    // second
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to "
                          << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

                    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionHold));
                    reply2->append(tr->secondAddress());
                    reply2->append(myaddr(), 20);
                    reply2->append(tr->secondId().begin(), 32);
                    reply2->append(transactionId.begin(), 32);

                    sendPacket(tr->secondAddress(), reply2);
                }
            }
        }
    }

    // ..and retranslate
    sendPacketBroadcast(packet);
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionHold(XBridgePacketPtr packet)
{
    if (packet->size() != 104)
    {
        ERR() << "incorrect packet size for xbcTransactionHold " << __FUNCTION__;
        return false;
    }

    // this addr
    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

    // smart hub addr
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // read packet data
    uint256 id   (packet->data()+40);
    uint256 newid(packet->data()+72);

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_pendingTransactions.count(id))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        // remove from pending
        XBridgeTransactionDescrPtr xtx = XBridgeApp::m_pendingTransactions[id];
        XBridgeApp::m_pendingTransactions.erase(id);

        // move to processing
        XBridgeApp::m_transactions[newid] = xtx;

        xtx->state = XBridgeTransactionDescr::trHold;
    }

    uiConnector.NotifyXBridgeTransactionIdChanged(id, newid);
    uiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trHold);

    // send hold apply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHoldApply));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(newid.begin(), 32);

    if (!sendPacketBroadcast(reply))
    {
        LOG() << "error sending transaction hold reply packet " << __FUNCTION__;
        return false;
    }
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

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionInit(XBridgePacketPtr packet)
{
    if (packet->size() != 144)
    {
        ERR() << "incorrect packet size for xbcTransactionInit" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    std::vector<unsigned char> from(packet->data()+72, packet->data()+92);
    std::string                fromCurrency(reinterpret_cast<const char *>(packet->data()+92));
    boost::uint64_t            fromAmount(*reinterpret_cast<boost::uint64_t *>(packet->data()+100));

    std::vector<unsigned char> to(packet->data()+108, packet->data()+128);
    std::string                toCurrency(reinterpret_cast<const char *>(packet->data()+128));
    boost::uint64_t            toAmount(*reinterpret_cast<boost::uint64_t *>(packet->data()+136));

    // create transaction
    // without id (non this client transaction)
    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    // ptr->id           = txid;
    ptr->from         = from;
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = to;
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);
        XBridgeApp::m_transactions[txid] = ptr;
    }

    // send initialized
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionInitialized));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);

    if (!sendPacketBroadcast(reply))
    {
        LOG() << "error sending transaction hold reply packet " << __FUNCTION__;
        return false;
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

//******************************************************************************
//******************************************************************************
CScript destination(const std::vector<unsigned char> & address, const char prefix)
{
    uint160 uikey(address);
    CKeyID key(uikey);

    CBitcoinAddress baddr;
    baddr.Set(key, prefix);

    CScript addr;
    addr.SetDestination(baddr.Get());
    return addr;
}

//******************************************************************************
//******************************************************************************
CScript destination(const std::string & address)
{
    CBitcoinAddress baddr(address);

    CScript addr;
    addr.SetDestination(baddr.Get());
    return addr;
}

//******************************************************************************
//******************************************************************************
std::string txToString(const CTransaction & tx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    return HexStr(ss.begin(), ss.end());
}

//******************************************************************************
//******************************************************************************
CTransaction txFromString(const std::string & str)
{
    std::vector<char> txdata = ParseHex(str);
    CDataStream stream(txdata,
                       SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    return tx;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCreate(XBridgePacketPtr packet)
{
    if (packet->size() != 100)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id   (packet->data()+40);

    // destination address
    std::vector<unsigned char> destAddress(packet->data()+72, packet->data()+92);

    // lock time
    boost::uint32_t lockTimeTx1 = *reinterpret_cast<boost::uint32_t *>(packet->data()+92);
    boost::uint32_t lockTimeTx2 = *reinterpret_cast<boost::uint32_t *>(packet->data()+96);

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[id];
    }


    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_user, m_passwd, m_address, m_port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return false;
    }

    boost::uint64_t fee = m_COIN*XBridgeTransactionDescr::MIN_TX_FEE/XBridgeTransactionDescr::COIN;
    boost::uint64_t outAmount = m_COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN)+fee;
    boost::uint64_t inAmount  = 0;

    std::vector<rpc::Unspent> usedInTx;
    for (const rpc::Unspent & entry : entries)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount*m_COIN;

        // check amount
        if (inAmount >= outAmount)
        {
            break;
        }
    }

    // check amount
    if (inAmount < outAmount)
    {
        // no money, cancel transaction
        sendCancelTransaction(id);
        return false;
    }

    // create tx1, locked
    CTransaction tx1;

    // lock time
    {
        time_t local = time(NULL);// GetAdjustedTime();
        tx1.nLockTime = local + lockTimeTx1;
    }

    // inputs
    for (const rpc::Unspent & entry : usedInTx)
    {
        CTxIn in(COutPoint(uint256(entry.txId), entry.vout));
        tx1.vin.push_back(in);
    }

    // outputs
    tx1.vout.push_back(CTxOut(outAmount-fee, destination(destAddress, m_prefix[0])));

    if (inAmount > outAmount)
    {
        std::string addr;
        if (!rpc::getNewAddress(m_user, m_passwd, m_address, m_port, addr))
        {
            // cancel transaction
            sendCancelTransaction(id);
            return false;
        }

        // rest
        CScript script = destination(addr);
        tx1.vout.push_back(CTxOut(inAmount-outAmount, script));
    }

    // serialize
    std::string unsignedTx1 = txToString(tx1);
    std::string signedTx1 = unsignedTx1;

    if (!rpc::signRawTransaction(m_user, m_passwd, m_address, m_port, signedTx1))
    {
        // do not sign, cancel
        sendCancelTransaction(id);
        return false;
    }

    xtx->payTx = signedTx1;

    // create tx2, inputs
    CTransaction tx2;
    CTxIn in(COutPoint(tx1.GetHash(), 0));
    tx2.vin.push_back(in);

    // outputs
    {
        std::string addr;
        if (!rpc::getNewAddress(m_user, m_passwd, m_address, m_port, addr))
        {
            // cancel transaction
            sendCancelTransaction(id);
            return false;
        }

        // rest
        CScript script = destination(addr);
        tx2.vout.push_back(CTxOut(outAmount-2*fee, script));
    }

    // lock time for tx2
    {
        time_t local = time(0); // GetAdjustedTime();
        tx2.nLockTime = local + lockTimeTx2;
    }

    // serialize
    std::string unsignedTx2 = txToString(tx2);

    // store
    xtx->revTx = unsignedTx2;

    xtx->state = XBridgeTransactionDescr::trCreated;
    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(id.begin(), 32);
    reply->append(unsignedTx1);
    reply->append(unsignedTx2);

    if (!sendPacketBroadcast(reply))
    {
        ERR() << "error sending created transactions packet " << __FUNCTION__;
        return false;
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

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionSign(XBridgePacketPtr packet)
{
    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionSign" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

    size_t offset = 20;
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string rawtxpay(reinterpret_cast<const char *>(packet->data()+offset));
    offset += rawtxpay.size()+1;

    std::string rawtxrev(reinterpret_cast<const char *>(packet->data()+offset));

    // check txid
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // unserialize
    CTransaction txpay = txFromString(rawtxpay);
    CTransaction txrev = txFromString(rawtxrev);

    if (txpay.nLockTime < LOCKTIME_THRESHOLD || txrev.nLockTime < LOCKTIME_THRESHOLD)
    {
        // not signed, cancel tx
        sendCancelTransaction(txid);
        return false;
    }

    // TODO check txpay, inputs-outputs

    // sign txrevert
    if (!rpc::signRawTransaction(m_user, m_passwd, m_address, m_port, rawtxrev))
    {
        // do not sign, cancel
        sendCancelTransaction(txid);
        return false;
    }

    xtx->state = XBridgeTransactionDescr::trSigned;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionSigned));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(txToString(txrev));

    if (!sendPacketBroadcast(reply))
    {
        ERR() << "error sending created transactions packet " << __FUNCTION__;
        return false;
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

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCommit(XBridgePacketPtr packet)
{
    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionCommit" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    std::string rawtx(reinterpret_cast<const char *>(packet->data()+72));

    // send pay transaction to network
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // unserialize signed transaction
    // CTransaction txrev = txFromString(rawtx);
    xtx->revTx = rawtx;

    if (!rpc::sendRawTransaction(m_user, m_passwd, m_address, m_port, xtx->payTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id);
        return false;
    }

//    uint256 walletTxId = (static_cast<CTransaction *>(&xtx->payTx))->GetHash();
//    m_mapWalletTxToXBridgeTx[walletTxId] = txid;

    xtx->state = XBridgeTransactionDescr::trCommited;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send commit apply to hub
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommited));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
//    reply->append(walletTxId.begin(), 32);
    if (!sendPacketBroadcast(reply))
    {
        ERR() << "error sending transaction commited packet "
                 << __FUNCTION__;
        return false;
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

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirm));
            reply->append(tr->firstDestination());
            reply->append(myaddr(), 20);
            reply->append(txid.begin(), 32);
            reply->append(tr->secondTxHash().begin(), 32);

            sendPacket(tr->firstDestination(), reply);

//            // TODO remove this log
//            LOG() << "send xbcTransactionCommit to "
//                  << util::base64_encode(std::string((char *)&tr->secondDestination()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirm));
            reply2->append(tr->secondDestination());
            reply2->append(myaddr(), 20);
            reply2->append(txid.begin(), 32);
            reply2->append(tr->firstTxHash().begin(), 32);

            sendPacket(tr->secondDestination(), reply2);
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

    uint256 txid(packet->data());

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        e.deletePendingTransactions(txid);

        // send cancel to clients
        // XBridgeTransactionPtr tr = e.transaction(txid);
        // if (tr->state() != XBridgeTransaction::trInvalid)
        {
            // boost::mutex::scoped_lock l(tr->m_lock);
            // sendCancelTransaction(tr);
            // sendCancelTransaction(txid);
        }
    }

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // signal for gui
            uiConnector.NotifyXBridgeTransactionStateChanged(txid, XBridgeTransactionDescr::trCancelled);
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trCancelled;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

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
    XBridgeApp & app = XBridgeApp::instance();

    // send my trx
    if (XBridgeApp::m_pendingTransactions.size())
    {
        if (XBridgeApp::m_txLocker.try_lock())
        {
            // send pending transactions
            for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_pendingTransactions.begin();
                 i != XBridgeApp::m_pendingTransactions.end(); ++i)
            {
                app.sendPendingTransaction(i->second);
            }

            XBridgeApp::m_txLocker.unlock();
        }
    }

    // send exchange trx
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
void XBridgeSession::getAddressBook()
{
    XBridgeApp::instance().getAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::requestAddressBook()
{
    std::vector<rpc::AddressBookEntry> entries;
    if (!rpc::requestAddressBook(m_user, m_passwd, m_address, m_port, entries))
    {
        return;
    }

    XBridgeApp & app = XBridgeApp::instance();

    for (const rpc::AddressBookEntry & e : entries)
    {
        for (const std::string & addr : e.second)
        {
            std::vector<unsigned char> vaddr;
            if (rpc::DecodeBase58Check(addr.c_str(), vaddr))
            {
                app.storageStore(shared_from_this(), &vaddr[1]);

                uiConnector.NotifyXBridgeAddressBookEntryReceived
                        (m_currency, e.first,
                         util::base64_encode(std::string((char *)&vaddr[1], vaddr.size()-1)));
            }
        }
    }
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

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processPendingTransaction(XBridgePacketPtr packet)
{
    if (packet->size() != 64)
    {
        ERR() << "incorrect packet size " << __FUNCTION__;
        return false;
    }

    XBridgeTransactionDescr d;
    d.id           = uint256(packet->data());
    d.fromCurrency = std::string(reinterpret_cast<const char *>(packet->data()+32));
    d.fromAmount   = *reinterpret_cast<boost::uint64_t *>(packet->data()+40);
    d.toCurrency   = std::string(reinterpret_cast<const char *>(packet->data()+48));
    d.toAmount     = *reinterpret_cast<boost::uint64_t *>(packet->data()+56);
    d.state        = XBridgeTransactionDescr::trPending;

    uiConnector.NotifyXBridgePendingTransactionReceived(d);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionFinished(XBridgePacketPtr packet)
{
    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionFinished" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // signal for gui
            uiConnector.NotifyXBridgeTransactionStateChanged(txid, XBridgeTransactionDescr::trFinished);
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trFinished;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::revertXBridgeTransaction(const uint256 & id)
{
    // TODO temporary implementation
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        // search tx
        for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_transactions.begin();
             i != XBridgeApp::m_transactions.end(); ++i)
        {
            if (i->second->id == id)
            {
                xtx = i->second;
                break;
            }
        }
    }

    if (!xtx)
    {
        return false;
    }

    // rollback, commit revert transaction
    if (!rpc::sendRawTransaction(m_user, m_passwd, m_address, m_port, xtx->payTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id);
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionRollback(XBridgePacketPtr packet)
{
    if (packet->size() != 52)
    {
        ERR() << "incorrect packet size for xbcTransactionRollback" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data()+20);

    // for rollback need local transaction id
    // TODO maybe hub id?
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown tx
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    revertXBridgeTransaction(xtx->id);

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trRollback;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionDropped(XBridgePacketPtr packet)
{
    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionDropped" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 id(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // signal for gui
            uiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trDropped);
            return false;
        }

        xtx = XBridgeApp::m_transactions[id];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trDropped;
    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    return true;
}
