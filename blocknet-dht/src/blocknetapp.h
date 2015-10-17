//*****************************************************************************
//*****************************************************************************
#ifndef BLOCKNETAPP_H
#define BLOCKNETAPP_H

#include <QApplication>

#include <thread>
#include <atomic>
#include <vector>
#include <map>

#include <Ws2tcpip.h>

//*****************************************************************************
//*****************************************************************************
class BlocknetApp : public QApplication
{
    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

    Q_OBJECT

public:
    BlocknetApp(int argc, char *argv[]);
    virtual ~BlocknetApp();

signals:
    void showLogMessage(const QString & msg);

public:
    bool initDht();
    bool stopDht();

    void logMessage(const QString & msg);

public slots:
    void onGenerate();
    void onDump();
    void onSearch(const std::string & id);
    void onSend(const std::string & id, const std::string & message);

public:
    static void sleep(const unsigned int umilliseconds);

private:
    void dhtThreadProc();

private:
    std::thread       m_dhtThread;
    std::atomic<bool> m_dhtStarted;
    std::atomic<bool> m_dhtStop;

    std::atomic<bool> m_signalGenerate;
    std::atomic<bool> m_signalDump;
    std::atomic<bool> m_signalSearch;
    std::atomic<bool> m_signalSend;

    typedef std::pair<std::string, std::string> stringpair;

    std::list<std::string> m_searchStrings;
    std::list<stringpair>  m_messages;

    const bool        m_ipv4;
    const bool        m_ipv6;

    sockaddr_in       m_sin;
    sockaddr_in6      m_sin6;
    unsigned short    m_dhtPort;

    std::vector<sockaddr_storage> m_nodes;
};

#endif // BLOCKNETAPP_H
