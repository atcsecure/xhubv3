//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "util/uint256.h"
#include "xbridgetransactionmember.h"

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction;
typedef boost::shared_ptr<XBridgeTransaction> XBridgeTransactionPtr;

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction
{
public:
    // see strState when editing
    enum State
    {
        trInvalid = 0,
        trNew,
        trJoined,
        trHold,
        trInitialized,
        trCreated,
        trSigned,
        trCommited,
        trConfirmed,
        trFinished,
        trCancelled,
        trDropped
    };

    enum
    {
        // ttl in seconds
        // TTL = 600
        // 150 for testing
        TTL = 150
    };

public:
    XBridgeTransaction();
    XBridgeTransaction(const uint256 & id,
                       const std::vector<unsigned char> & sourceAddr,
                       const std::string & sourceCurrency,
                       const boost::uint64_t & sourceAmount,
                       const std::vector<unsigned char> & destAddr,
                       const std::string & destCurrency,
                       const boost::uint64_t & destAmount);
    ~XBridgeTransaction();

    uint256 id() const;
    // state of transaction
    State state() const;
    // update state counter and update state
    State increaseStateCounter(State state, const std::vector<unsigned char> & from);

    static std::string strState(const State state);
    std::string strState() const;

    void updateTimestamp();

    bool isFinished() const;
    bool isValid() const;
    bool isExpired() const;

    void cancel();
    void drop();
    void finish();

    bool confirm(const uint256 & hash);

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    uint256                    firstId() const;
    std::vector<unsigned char> firstAddress() const;
    std::vector<unsigned char> firstDestination() const;
    std::string                firstCurrency() const;
    boost::uint64_t            firstAmount() const;
    std::string                firstRawPayTx() const;
    std::string                firstRawRevTx() const;
    uint256                    firstTxHash() const;

    uint256                    secondId() const;
    std::vector<unsigned char> secondAddress() const;
    std::vector<unsigned char> secondDestination() const;
    std::string                secondCurrency() const;
    boost::uint64_t            secondAmount() const;
    std::string                secondRawPayTx() const;
    std::string                secondRawRevTx() const;
    uint256                    secondTxHash() const;

    bool tryJoin(const XBridgeTransactionPtr other);

    // std::vector<unsigned char> opponentAddress(const std::vector<unsigned char> & addr);

    bool                       setRawPayTx(const std::vector<unsigned char> & addr,
                                           const std::string & rawpaytx,
                                           const std::string & rawrevtx);
    bool                       updateRawRevTx(const std::vector<unsigned char> & addr,
                                              const std::string & rawrevytx);
    bool                       setTxHash(const std::vector<unsigned char> & addr,
                                         const uint256 & hash);

public:
    boost::mutex               m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;

    State                      m_state;
    // unsigned int               m_stateCounter;
    bool                       m_firstStateChanged;
    bool                       m_secondStateChanged;

    unsigned int               m_confirmationCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    boost::uint64_t            m_sourceAmount;
    boost::uint64_t            m_destAmount;

    std::string                m_rawpaytx1;
    std::string                m_rawrevtx1;
    std::string                m_rawpaytx2;
    std::string                m_rawrevtx2;

    uint256                    m_txhash1;
    uint256                    m_txhash2;

    XBridgeTransactionMember   m_first;
    XBridgeTransactionMember   m_second;
};

#endif // XBRIDGETRANSACTION_H
