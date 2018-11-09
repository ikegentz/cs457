//
// Created by ikegentz on 10/25/18.
//

#include "tcp_user_socket.h"
#include "../utils/buffer.h"
#include <cstring>
#include <sys/fcntl.h>
#include <mutex>

using namespace std;

IRC_Server::TCP_User_Socket::TCP_User_Socket() {}

void IRC_Server::TCP_User_Socket::setSocket(int sckt)
{
    userSocket=sckt;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // set a timeout so we don't get stuck receiving
    setsockopt(userSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}


socklen_t IRC_Server::TCP_User_Socket::getLengthPointer()
{
    socklen_t len = sizeof(userAddress);
    return len;
}

int IRC_Server::TCP_User_Socket::getSocket()
{
    return userSocket;
}

int IRC_Server::TCP_User_Socket::closeSocket()
{
    return close(userSocket);
}

void IRC_Server::TCP_User_Socket::setUserInfoIPv4(string address, uint16_t port)
{
    clientAddressIPv4 = address;
    clientPortIPv4 = port;
}

std::tuple<string,ssize_t> IRC_Server::TCP_User_Socket::recvString(int bufferSize, bool useMutex)
{
    Utils::Buffer stringBuffer(bufferSize);
    stringBuffer.zero_out();

    ssize_t recvMsgSize;

    if (useMutex)
    {
        lock_guard<mutex> lock(recvMutex);
        recvMsgSize = recv(userSocket, stringBuffer.get_buffer(), bufferSize, 0);
    }
    else
    {
        recvMsgSize = recv(userSocket, stringBuffer.get_buffer(), bufferSize, 0);
    }



    return make_tuple(string(stringBuffer.get_buffer()),recvMsgSize);
}


ssize_t IRC_Server::TCP_User_Socket::sendString(const string & data, bool useMutex)
{
    //https://stackoverflow.com/questions/7352099/stdstring-to-char
    if (data.size() == 0) return 0;
    int bufferSize = data.size()+1;
    vector<char> stringBuffer(data.c_str(), data.c_str() + data.size() + 1u);

    ssize_t rval;
    if (useMutex)
    {
        lock_guard<mutex> lock(sendMutex);
        rval = send(userSocket, &stringBuffer[0], bufferSize, 0);
    }
    else
    {
        rval = send(userSocket, &stringBuffer[0], bufferSize, 0);
    }

    return rval;
}

std::string IRC_Server::TCP_User_Socket::getAddress()
{
    return clientAddressIPv4;
}

int IRC_Server::TCP_User_Socket::getPort()
{
    return clientPortIPv4;
}

string IRC_Server::TCP_User_Socket::getUniqueIdentifier()
{
    //this unique identifier is arbitrary
    //and it may not be useful for chat program (see other classes)
    //it has not been tested to be unique but for now, we don't need it.
    string identifier  = "[" + clientAddressIPv4 + "," + to_string(clientPortIPv4) + "]";
    return identifier;
}

struct sockaddr * IRC_Server::TCP_User_Socket::getAddressPointer()
{
    return ((struct sockaddr *) &userAddress);
}