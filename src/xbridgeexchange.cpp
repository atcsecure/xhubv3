//*****************************************************************************
//*****************************************************************************

#include "xbridgeexchange.h"
#include "xbridgeapp.h"
#include "util/logger.h"
#include "util/settings.h"
#include "util/util.h"

#include <algorithm>

//*****************************************************************************
//*****************************************************************************
XBridgeExchange::XBridgeExchange()
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeExchange::~XBridgeExchange()
{
}

//*****************************************************************************
//*****************************************************************************
// static
XBridgeExchange & XBridgeExchange::instance()
{
    static XBridgeExchange e;
    return e;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::init()
{
    if (!settings().isExchangeEnabled())
    {
        // disabled
        return true;
    }

    Settings & s = settings();

    std::vector<std::string> wallets = s.exchangeWallets();
    for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        std::string label   = s.get<std::string>(*i + ".Title");
        std::string address = s.get<std::string>(*i + ".Address");
        std::string ip      = s.get<std::string>(*i + ".Ip");
        unsigned int port   = s.get<unsigned int>(*i + ".Port");
        std::string user    = s.get<std::string>(*i + ".Username");
        std::string passwd  = s.get<std::string>(*i + ".Password");

        if (address.empty() || ip.empty() || port == 0 ||
                user.empty() || passwd.empty())
        {
            LOG() << "read wallet " << *i << " with empty parameters>";
            continue;
        }

        std::string decoded = util::base64_decode(address);
        if (address.empty())
        {
            LOG() << "incorrect wallet address for " << *i;
            continue;
        }

        std::copy(decoded.begin(), decoded.end(), std::back_inserter(m_wallets[*i].address));
        if (m_wallets[*i].address.size() != 20)
        {
            LOG() << "incorrect wallet address size for " << *i;
            m_wallets.erase(*i);
            continue;
        }

        m_wallets[*i].title   = label;
        m_wallets[*i].ip      = ip;
        m_wallets[*i].port    = port;
        m_wallets[*i].user    = user;
        m_wallets[*i].passwd  = passwd;

        LOG() << "read wallet " << *i << " \"" << label << "\" address <" << address << ">";
    }

    if (isEnabled())
    {
        LOG() << "exchange enabled";
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::isEnabled()
{
    return m_wallets.size() > 0;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::haveConnectedWallet(const std::string & walletName)
{
    return m_wallets.count(walletName) > 0;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> XBridgeExchange::walletAddress(const std::string & walletName)
{
    if (!m_wallets.count(walletName))
    {
        ERR() << "reqyest address for unknown wallet <" << walletName
              << ">" << __FUNCTION__;
        return std::vector<unsigned char>();
    }

    return m_wallets[walletName].address;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::createTransaction(const uint256 & id,
                                        const std::vector<unsigned char> & sourceAddr,
                                        const std::string & sourceCurrency,
                                        const boost::uint64_t & sourceAmount,
                                        const std::vector<unsigned char> & destAddr,
                                        const std::string & destCurrency,
                                        const boost::uint64_t & destAmount,
                                        uint256 & transactionId)
{
    DEBUG_TRACE();

    // transactionId = id;

    XBridgeTransactionPtr tr(new XBridgeTransaction(id,
                                                    sourceAddr, sourceCurrency,
                                                    sourceAmount,
                                                    destAddr, destCurrency,
                                                    destAmount));

    LOG() << tr->hash1().ToString();
    LOG() << tr->hash2().ToString();

    if (!tr->isValid())
    {
        return false;
    }

    uint256 h = tr->hash2();

    XBridgeTransactionPtr tmp;

    {
        boost::mutex::scoped_lock l(m_pendingTransactionsLock);

        if (!m_pendingTransactions.count(h))
        {
            // new transaction or update existing (update timestamp)
            h = tr->hash1();
            m_pendingTransactions[h] = tr;
        }
        else
        {
            boost::mutex::scoped_lock l2(m_pendingTransactions[h]->m_lock);

            // found, check if expired
            if (m_pendingTransactions[h]->isExpired())
            {
                // if expired - delete old transaction
                m_pendingTransactions.erase(h);

                // create new
                h = tr->hash1();
                m_pendingTransactions[h] = tr;
            }
            else
            {
                // try join with existing transaction
                if (!m_pendingTransactions[h]->tryJoin(tr))
                {
                    LOG() << "transaction not joined";
                    // return false;

                    // create new transaction
                    h = tr->hash1();
                    m_pendingTransactions[h] = tr;
                }
                else
                {
                    LOG() << "transactions joined, new id <" << tr->id().GetHex() << ">";

                    tmp = m_pendingTransactions[h];
                }
            }
        }
    }

    if (tmp)
    {
        // new transaction id
        transactionId = tmp->id();

        // move to transactions
        {
            boost::mutex::scoped_lock l(m_transactionsLock);
            m_transactions[tmp->id()] = tmp;
        }
        {
            boost::mutex::scoped_lock l(m_pendingTransactionsLock);
            m_pendingTransactions.erase(h);
        }

    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::deletePendingTransactions(const uint256 & id)
{
    boost::mutex::scoped_lock l(m_pendingTransactionsLock);

    LOG() << "delete pending transaction <" << id.GetHex() << ">";

    m_pendingTransactions.erase(id);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::deleteTransaction(const uint256 & id)
{
    boost::mutex::scoped_lock l(m_transactionsLock);

    LOG() << "delete transaction <" << id.GetHex() << ">";

    m_transactions.erase(id);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransactionWhenHoldApplyReceived(XBridgeTransactionPtr tx,
                                                             const std::vector<unsigned char> & from)
{
    if (tx->increaseStateCounter(XBridgeTransaction::trJoined, from) == XBridgeTransaction::trHold)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransactionWhenInitializedReceived(XBridgeTransactionPtr tx,
                                                               const std::vector<unsigned char> & from)
{
    if (tx->increaseStateCounter(XBridgeTransaction::trHold, from) == XBridgeTransaction::trInitialized)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransactionWhenCreatedReceived(XBridgeTransactionPtr tx,
                                                           const std::vector<unsigned char> & from,
                                                           const std::string & rawpaytx,
                                                           const std::string & rawrevtx)
{
    if (!tx->setRawPayTx(from, rawpaytx, rawrevtx))
    {
        // wtf?
        LOG() << "unknown sender address for transaction, id <" << tx->id().GetHex() << ">";
        return false;
    }

    if (tx->increaseStateCounter(XBridgeTransaction::trInitialized, from) == XBridgeTransaction::trCreated)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransactionWhenSignedReceived(XBridgeTransactionPtr tx,
                                                          const std::vector<unsigned char> & from,
                                                          const std::string & rawrevtx)
{
    if (!tx->updateRawRevTx(from, rawrevtx))
    {
        // wtf?
        LOG() << "unknown sender address for transaction, id <" << tx->id().GetHex() << ">";
        return false;
    }

    // update transaction state
    if (tx->increaseStateCounter(XBridgeTransaction::trCreated, from) == XBridgeTransaction::trSigned)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransactionWhenCommitedReceived(XBridgeTransactionPtr tx,
                                                            const std::vector<unsigned char> & from,
                                                            const uint256 & txhash)
{
    if (!tx->setTxHash(from, txhash))
    {
        // wtf?
        LOG() << "unknown sender address for transaction, id <" << tx->id().GetHex() << ">";
        return false;
    }

    {
        boost::mutex::scoped_lock l (m_unconfirmedLock);
        m_unconfirmed[txhash] = tx->id();
    }

    // update transaction state
    if (tx->increaseStateCounter(XBridgeTransaction::trSigned, from) == XBridgeTransaction::trCommited)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeExchange::updateTransactionWhenConfirmedReceived(XBridgeTransactionPtr tx)
//{
//    // update transaction state
//    if (tx->increaseStateCounter(XBridgeTransaction::trCommited) == XBridgeTransaction::trFinished)
//    {
//        return true;
//    }

//    return false;
//}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransaction(const uint256 & hash)
{
    // DEBUG_TRACE();

    // store
    m_walletTransactions.insert(hash);

    // check unconfirmed
    boost::mutex::scoped_lock l (m_unconfirmedLock);
    if (m_unconfirmed.size())
    {
        for (std::map<uint256, uint256>::iterator i = m_unconfirmed.begin(); i != m_unconfirmed.end();)
        {
            if (m_walletTransactions.count(i->first))
            {
                LOG() << "confirm transaction, id <" << i->second.GetHex()
                      << "> hash <" << i->first.GetHex() << ">";

                XBridgeTransactionPtr tr = transaction(i->second);
                boost::mutex::scoped_lock l(tr->m_lock);

                tr->confirm(i->first);

                i = m_unconfirmed.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }


//    uint256 txid;

//    boost::mutex::scoped_lock l (m_unconfirmedLock);
//    if (m_unconfirmed.count(hash))
//    {
//        txid = m_unconfirmed[hash];

//        LOG() << "confirm transaction, id <"
//              << util::base64_encode(std::string((char *)(txid.begin()), 32))
//              << "> hash <"
//              << util::base64_encode(std::string((char *)(hash.begin()), 32))
//              << ">";

//        m_unconfirmed.erase(hash);
//    }

//    if (txid != uint256())
//    {
//        XBridgeTransactionPtr tr = transaction(txid);
//        boost::mutex::scoped_lock l(tr->m_lock);

//        tr->confirm(hash);
//    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
const XBridgeTransactionPtr XBridgeExchange::transaction(const uint256 & hash)
{
    {
        boost::mutex::scoped_lock l(m_transactionsLock);

        if (m_transactions.count(hash))
        {
            return m_transactions[hash];
        }
        else
        {
            assert(false || "cannot find transaction");

            // unknown transaction
            LOG() << "unknown transaction, id <" << hash.GetHex() << ">";
        }
    }

    // TODO not search in pending transactions
//    {
//        boost::mutex::scoped_lock l(m_pendingTransactionsLock);

//        if (m_pendingTransactions.count(hash))
//        {
//            return m_pendingTransactions[hash];
//        }
//    }

    // return XBridgeTransaction::trInvalid;
    return XBridgeTransactionPtr(new XBridgeTransaction);
}

//*****************************************************************************
//*****************************************************************************
std::list<XBridgeTransactionPtr> XBridgeExchange::pendingTransactions() const
{
    boost::mutex::scoped_lock l(m_pendingTransactionsLock);

    std::list<XBridgeTransactionPtr> list;

    for (std::map<uint256, XBridgeTransactionPtr>::const_iterator i = m_pendingTransactions.begin();
         i != m_pendingTransactions.end(); ++i)
    {
        list.push_back(i->second);
    }

    return list;
}

//*****************************************************************************
//*****************************************************************************
std::list<XBridgeTransactionPtr> XBridgeExchange::transactions(bool onlyFinished) const
{
    boost::mutex::scoped_lock l(m_transactionsLock);

    std::list<XBridgeTransactionPtr> list;

    for (std::map<uint256, XBridgeTransactionPtr>::const_iterator i = m_transactions.begin(); i != m_transactions.end(); ++i)
    {
        if (!onlyFinished)
        {
            list.push_back(i->second);
        }
        else if (i->second->isExpired() ||
                 !i->second->isValid() ||
                 i->second->isFinished() ||
                 i->second->state() == XBridgeTransaction::trConfirmed)
        {
            list.push_back(i->second);
        }
    }

    return list;
}

//*****************************************************************************
//*****************************************************************************
std::list<XBridgeTransactionPtr> XBridgeExchange::transactions() const
{
    return transactions(false);
}

//*****************************************************************************
//*****************************************************************************
std::list<XBridgeTransactionPtr> XBridgeExchange::finishedTransactions() const
{
    return transactions(true);
}

//*****************************************************************************
//*****************************************************************************
std::vector<StringPair> XBridgeExchange::listOfWallets() const
{
    // TODO only enabled wallets
//    std::vector<StringPair> result;
//    for (WalletList::const_iterator i = m_wallets.begin(); i != m_wallets.end(); ++i)
//    {
//        result.push_back(std::make_pair(i->first, i->second.title));
//    }
//    return result;

    Settings & s = settings();

    std::vector<StringPair> result;
    std::vector<std::string> wallets = s.exchangeWallets();
    for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        std::string label   = s.get<std::string>(*i + ".Title");
//        std::string address = s.get<std::string>(*i + ".Address");
//        std::string ip      = s.get<std::string>(*i + ".Ip");
//        unsigned int port   = s.get<unsigned int>(*i + ".Port");
//        std::string user    = s.get<std::string>(*i + ".Username");
//        std::string passwd  = s.get<std::string>(*i + ".Password");

//        if (address.empty() || ip.empty() || port == 0 ||
//                user.empty() || passwd.empty())
//        {
//            LOG() << "read wallet " << *i << " with empty parameters>";
//            continue;
//        }

        result.push_back(std::make_pair(*i, label));
    }
    return result;
}
