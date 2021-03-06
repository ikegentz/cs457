#include <netinet/in.h> 
#include <arpa/inet.h>
#include "tcp_client_socket.h"
#include <string> 
#include <cstring> 
#include <vector> 
#include <sys/fcntl.h>
#include <cstring>
#include <mutex>

#include "buffer.h"

using namespace std; 
    

IRC_Client::TCPClientSocket::TCPClientSocket(string networkAddress, uint portNumber): address(networkAddress), port(portNumber)
{
    init(); 
    setSocketOptions();
}

void IRC_Client::TCPClientSocket::init()
{
    //here I may have differences with server 
    //check after testing
    clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    const char * cstr = address.c_str();
    //int val =0;
    bzero(&serverAddress,sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    //val = inet_aton(cstr,&addr);
    inet_aton(cstr,&addr);
    serverAddress.sin_addr = addr;
    //string addresscpy(inet_ntoa(addr));
    //address = addresscpy;  
    serverAddress.sin_port = htons(port);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // set a timeout so we don't get stuck receiving
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}

void IRC_Client::TCPClientSocket::setSocketOptions()
{
    int optval = 1;
    setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR, 
	                (const void *)&optval , sizeof(int));
}

int IRC_Client::TCPClientSocket::connectSocket()
{
    return connect(clientSocket,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
}

int IRC_Client::TCPClientSocket::closeSocket()
{
    return close(clientSocket);
}

std::tuple<string,ssize_t> IRC_Client::TCPClientSocket::recvString(int bufferSize, bool useMutex)
{
    Utils::Buffer stringBuffer(bufferSize);
    stringBuffer.zero_out();

    ssize_t recvMsgSize;

    if (useMutex)
    {
        lock_guard<mutex> lock(recvMutex);
        recvMsgSize = recv(clientSocket, stringBuffer.get_buffer(), bufferSize, 0);
    }    
    else
    {
        recvMsgSize = recv(clientSocket, stringBuffer.get_buffer(), bufferSize, 0);
    }
   
    return make_tuple(string(stringBuffer.get_buffer()),recvMsgSize);
}
        

ssize_t IRC_Client::TCPClientSocket::sendString(const string & data, bool useMutex)
{
    //https://stackoverflow.com/questions/7352099/stdstring-to-char
    if (data.size() == 0) return 0;                 
    int bufferSize = data.size()+1; 
    vector<char> stringBuffer(data.c_str(), data.c_str() + data.size() + 1u);

    ssize_t rval; 
    if (useMutex)
    {
       lock_guard<mutex> lock(sendMutex);
       rval = send(clientSocket, &stringBuffer[0], bufferSize, 0);
    }
    else
    {
       rval = send(clientSocket, &stringBuffer[0], bufferSize, 0);     
    }

    return rval;
}
