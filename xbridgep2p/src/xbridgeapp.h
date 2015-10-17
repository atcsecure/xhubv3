//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridge.h"
#include "xbridgesession.h"

#include <QApplication>

#include <thread>
#include <atomic>
#include <vector>
#include <map>

#include <Ws2tcpip.h>

//*****************************************************************************
//*****************************************************************************
class XBridgeApp : public QApplication
{
    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

    Q_OBJECT

public:
    XBridgeApp(int argc, char *argv[]);
    virtual ~XBridgeApp();

signals:
    void showLogMessage(const QString & msg);

public:
    bool initDht();
    bool stopDht();

    void logMessage(const QString & msg);

    void storageStore(XBridgeSessionPtr session, const unsigned char * data);
    void storageClean(XBridgeSessionPtr session);

public slots:
    void onGenerate();
    void onDump();
    void onSearch(const std::string & id);
    void onSend(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);
    void onXChatMessageReceived(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);

public:
    static void sleep(const unsigned int umilliseconds);

private:
    void dhtThreadProc();
    void bridgeThreadProc();

private:
    std::thread       m_dhtThread;
    std::atomic<bool> m_dhtStarted;
    std::atomic<bool> m_dhtStop;

    std::atomic<bool> m_signalGenerate;
    std::atomic<bool> m_signalDump;
    std::atomic<bool> m_signalSearch;
    std::atomic<bool> m_signalSend;

    typedef std::vector<unsigned char> UcharVector;
    typedef std::pair<UcharVector, UcharVector> MessagePair;

    std::list<std::string> m_searchStrings;
    std::list<MessagePair> m_messages;

    const bool        m_ipv4;
    const bool        m_ipv6;

    sockaddr_in       m_sin;
    sockaddr_in6      m_sin6;
    unsigned short    m_dhtPort;

    std::vector<sockaddr_storage> m_nodes;

    std::thread       m_bridgeThread;
    XBridge m_bridge;

    boost::mutex m_sessionsLock;
    typedef std::map<std::vector<unsigned char>, XBridgeSessionPtr> SessionMap;
    SessionMap m_sessions;
};

#endif // XBRIDGEAPP_H
