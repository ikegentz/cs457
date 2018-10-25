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
    this->server_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
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
    int optval = 1;
    setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
}