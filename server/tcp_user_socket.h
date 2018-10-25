//
// Created by ikegentz on 10/25/18.
//

#ifndef CS457_TCP_USER_SOCKET_H
#define CS457_TCP_USER_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string>
#include <tuple>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <memory>
#include <mutex>


namespace IRC_Server
{
    using namespace std;

    class TCP_User_Socket
    {
    public:
        TCP_User_Socket();
        void setSocket(int skct);
        struct sockaddr *getAddressPointer();
        socklen_t getLenghtPointer();
        int getSocket();
        int closeSocket();
        std::tuple <string, ssize_t> recvString(int bufferSize = 4096, bool useMutex = true);
        void setUserInfoIPv4(string clientAddress, uint16_t port);
        ssize_t sendString(const string &data, bool useMutex = true);
        string getUniqueIdentifier();

    private:
        struct sockaddr_in userAddress;
        int userSocket;
        string clientAddressIPv4;
        uint16_t clientPortIPv4;
        mutex sendMutex;
        mutex recvMutex;
    };

}
#endif //CS457_TCP_USER_SOCKET_H
