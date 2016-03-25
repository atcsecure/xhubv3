#ifndef UICONNECTOR_H
#define UICONNECTOR_H

#include <string>
#include <boost/signals2/signal.hpp>

class uint256;
struct XBridgeTransactionDescr;

class UIConnector
{
public:
    boost::signals2::signal<void (const XBridgeTransactionDescr & tx)> NotifyXBridgePendingTransactionReceived;

    boost::signals2::signal<void (const uint256 & id, const uint256 & newid)> NotifyXBridgeTransactionIdChanged;

    boost::signals2::signal<void (const uint256 & id, const unsigned int state)> NotifyXBridgeTransactionStateChanged;

    boost::signals2::signal<void (const std::string & currency,
                                  const std::string & name,
                                  const std::string & address)> NotifyXBridgeAddressBookEntryReceived;

    boost::signals2::signal<void (const std::string str)> NotifyLogMessage;
};

extern UIConnector uiConnector;

#endif // UICONNECTOR_H

