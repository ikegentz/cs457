//
// Created by ikegentz on 10/25/18.
//

#include "tcp_server_socket.h"

IRC_Server::TCP_Server_Socket::TCP_Server_Socket(unsigned _port): port(_port), address("")
{}
IRC_Server::TCP_Server_Socket::TCP_Server_Socket(unsigned _port, std::string _address): port(_port), address(_address)
{}

int IRC_Server::TCP_Server_Socket::listen_socket()
{
    return listen(this->server_socket, 14);
}

int IRC_Server::TCP_Server_Socket::bind_socket()
{
    return bind(this->server_socket,(struct sockaddr *)&this->server_address,sizeof(this->server_address));
}

int IRC_Server::TCP_Server_Socket::init()
{
    // trying to make this non-blocking so we can check if the server needs to exit
    this->server_socket = socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK,IPPROTO_TCP);
    const char * cstr = this->address.c_str();
    this->server_address = {};
    this->server_address.sin_family = AF_INET;

    int val = 0;
    if (this->address == "")
        this->addr.s_addr =  htonl(INADDR_ANY);
    else
    {
        val = inet_aton(cstr,&this->addr); // convert addr to binary form

        if(val == 0)
        {
            std::cerr << "BAD IPv4 ADDRESS" << std::endl;
            return 1;
        }

        this->server_address.sin_addr = this->addr;
        std::string addresscpy(inet_ntoa(this->addr));
        this->address = addresscpy;
    }

    this->server_address.sin_port = htons(port);

    this->set_socket_options();
    return 0;
}

void IRC_Server::TCP_Server_Socket::set_socket_options()
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // set a timeout so we don't get stuck receiving
    setsockopt(this->server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    int optval = 1;
    setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
}

std::tuple<std::shared_ptr<IRC_Server::TCP_User_Socket>,int> IRC_Server::TCP_Server_Socket::acceptSocket()
{
    std::shared_ptr<IRC_Server::TCP_User_Socket> userSocket = make_shared<IRC_Server::TCP_User_Socket>();
    socklen_t len = userSocket.get()->getLengthPointer();
    int client_fd = accept(server_socket, (struct sockaddr*)userSocket.get()->getAddressPointer(), &len);
    userSocket.get()->setSocket(client_fd);

    char userIPv4[16];
    sockaddr_in* userAddress = (sockaddr_in*) userSocket.get()->getAddressPointer();
    inet_ntop(AF_INET, &(userAddress->sin_addr), userIPv4, INET_ADDRSTRLEN);

    auto clientPort = ntohs(userAddress->sin_port);
    userSocket.get()->setUserInfoIPv4(std::string(userIPv4), clientPort);

    return make_tuple(userSocket, client_fd);
}