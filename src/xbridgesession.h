//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridge.h"
#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "FastDelegate.h"
#include "util/uint256.h"

#include <memory>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSession
        : public std::enable_shared_from_this<XBridgeSession>
        , private boost::noncopyable
{
public:
    XBridgeSession();

    void start(XBridge::SocketPtr socket);

    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);

    bool sendXBridgeMessage(XBridgePacketPtr packet);
    bool sendXBridgeMessage(const std::vector<unsigned char> & message);

    bool processPacket(XBridgePacketPtr packet);

    void sendListOfWallets();
    void sendListOfTransactions();
    void checkFinishedTransactions();
    void eraseExpiredPendingTransactions();

    void resendAddressBook();
    void sendAddressbookEntry(const std::string & currency,
                              const std::string & name,
                              const std::string & address);

private:
    void disconnect();

    void doReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset = 0);
    void onReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset,
                      const boost::system::error_code & error,
                      std::size_t transferred);

    void doReadBody(XBridgePacketPtr packet,
                    const std::size_t offset = 0);
    void onReadBody(XBridgePacketPtr packet,
                    const std::size_t offset,
                    const boost::system::error_code & error,
                    std::size_t transferred);

    const unsigned char * myaddr() const;

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

    void sendPacket(const std::vector<unsigned char> & to, XBridgePacketPtr packet);
    void sendPacketBroadcast(XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool relayPacket(XBridgePacketPtr packet);

private:
    bool processInvalid(XBridgePacketPtr packet);
    bool processZero(XBridgePacketPtr packet);
    bool processAnnounceAddresses(XBridgePacketPtr packet);
    bool processXChatMessage(XBridgePacketPtr packet);

    bool processTransaction(XBridgePacketPtr packet);
    bool processTransactionHoldApply(XBridgePacketPtr packet);
    bool processTransactionInitialized(XBridgePacketPtr packet);
    bool processTransactionCreated(XBridgePacketPtr packet);
    bool processTransactionSigned(XBridgePacketPtr packet);
    bool processTransactionCommited(XBridgePacketPtr packet);
    // bool processTransactionConfirmed(XBridgePacketPtr packet);
    bool processTransactionCancel(XBridgePacketPtr packet);

    bool finishTransaction(XBridgeTransactionPtr tr);
    bool sendCancelTransaction(const uint256 & txid);
    bool rollbackTransaction(XBridgeTransactionPtr tr);

    bool processBitcoinTransactionHash(XBridgePacketPtr packet);

    bool processAddressBookEntry(XBridgePacketPtr packet);

private:
    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
