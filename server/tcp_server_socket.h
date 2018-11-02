//
// Created by ikegentz on 10/25/18.
//

#ifndef CS457_TCP_SERVER_SOCKET_H
#define CS457_TCP_SOCKET_SERVER_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include <memory>

#include "tcp_user_socket.h"

namespace IRC_Server
{
    class TCP_Server_Socket
    {
    public:
        TCP_Server_Socket(unsigned portNumber);
        TCP_Server_Socket(unsigned portNumber, std::string address);
        int init();
        int bind_socket();
        int listen_socket();
        std::tuple<std::shared_ptr<IRC_Server::TCP_User_Socket>,int> acceptSocket();

    private:
        void set_socket_options();

        uint port;
        std::string address;
        int server_socket;

        struct sockaddr_in server_address;
        struct in_addr addr;
    };
}

#endif //CS457_TCP_SOCKET_H
