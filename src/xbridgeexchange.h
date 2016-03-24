//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEEXCHANGE_H
#define XBRIDGEEXCHANGE_H

#include "util/uint256.h"
#include "xbridgetransaction.h"

#include <string>
#include <set>
#include <map>
#include <list>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

//*****************************************************************************
//*****************************************************************************
typedef std::pair<std::string, std::string> StringPair;

//*****************************************************************************
//*****************************************************************************
struct WalletParam
{
    std::string                title;
    std::vector<unsigned char> address;
    std::string                ip;
    unsigned int               port;
    std::string                user;
    std::string                passwd;
};

//*****************************************************************************
//*****************************************************************************
class XBridgeExchange
{
public:
    static XBridgeExchange & instance();

protected:
    XBridgeExchange();
    ~XBridgeExchange();

public:
    bool init();

    bool isEnabled();
    bool haveConnectedWallet(const std::string & walletName);

    std::vector<unsigned char> walletAddress(const std::string & walletName);

    bool createTransaction(const uint256 & id,
                           const std::vector<unsigned char> & sourceAddr,
                           const std::string & sourceCurrency,
                           const boost::uint64_t & sourceAmount,
                           const std::vector<unsigned char> & destAddr,
                           const std::string & destCurrency,
                           const boost::uint64_t & destAmount,
                           uint256 & transactionId);
    bool deletePendingTransactions(const uint256 & id);
    bool deleteTransaction(const uint256 & id);

    bool updateTransactionWhenHoldApplyReceived(XBridgeTransactionPtr tx,
                                                const std::vector<unsigned char> & from);
    bool updateTransactionWhenInitializedReceived(XBridgeTransactionPtr tx,
                                                  const std::vector<unsigned char> & from);
    bool updateTransactionWhenCreatedReceived(XBridgeTransactionPtr tx,
                                              const std::vector<unsigned char> & from,
                                              const std::string & rawpaytx,
                                              const std::string & rawrevtx);
    bool updateTransactionWhenSignedReceived(XBridgeTransactionPtr tx,
                                             const std::vector<unsigned char> & from,
                                             const std::string & rawrevtx);
    bool updateTransactionWhenCommitedReceived(XBridgeTransactionPtr tx,
                                               const std::vector<unsigned char> & from,
                                               const uint256 & txhash);
    bool updateTransactionWhenConfirmedReceived(XBridgeTransactionPtr tx,
                                                const std::vector<unsigned char> & from);

    bool updateTransaction(const uint256 & hash);

    const XBridgeTransactionPtr transaction(const uint256 & hash);
    std::list<XBridgeTransactionPtr> pendingTransactions() const;
    std::list<XBridgeTransactionPtr> transactions() const;
    std::list<XBridgeTransactionPtr> finishedTransactions() const;

    std::vector<StringPair> listOfWallets() const;

private:
    std::list<XBridgeTransactionPtr> transactions(bool onlyFinished) const;

private:
    // connected wallets
    typedef std::map<std::string, WalletParam> WalletList;
    WalletList                               m_wallets;

    mutable boost::mutex                     m_pendingTransactionsLock;
    std::map<uint256, XBridgeTransactionPtr> m_pendingTransactions;

    mutable boost::mutex                     m_transactionsLock;
    std::map<uint256, XBridgeTransactionPtr> m_transactions;

    mutable boost::mutex                     m_unconfirmedLock;
    std::map<uint256, uint256>               m_unconfirmed;

    // TODO use deque and limit size
    std::set<uint256>                        m_walletTransactions;

    mutable boost::mutex                     m_knownTxLock;
    std::set<uint256>                        m_knownTransactions;
};

#endif // XBRIDGEEXCHANGE_H
