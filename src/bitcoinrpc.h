//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_

#include <vector>
#include <string>

namespace rpc
{
    bool DecodeBase58Check(const char * psz, std::vector<unsigned char> & vchRet);

    typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;
    bool requestAddressBook(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::vector<AddressBookEntry> & entries);

    struct Unspent
    {
        std::string txId;
        int vout;
        double amount;
    };
    bool listUnspent(const std::string & rpcuser,
                     const std::string & rpcpasswd,
                     const std::string & rpcip,
                     const std::string & rpcport,
                     std::vector<Unspent> & entries);

    bool signRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::string & rawtx);

    bool sendRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            const std::string & rawtx);

    bool getNewAddress(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       std::string & addr);

} // namespace rpc

#endif
