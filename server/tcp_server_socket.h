//
// Created by ikegentz on 10/25/18.
//

#ifndef CS457_TCP_SOCKET_H
#define CS457_TCP_SOCKET_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include "tcp_server_socket.h"

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
