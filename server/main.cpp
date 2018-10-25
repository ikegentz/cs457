//
// Created by ikegentz on 10/22/18.
//

#include <unistd.h>
#include <iostream>

#include "tcp_server_socket.h"

namespace IRC_Server
{
    const unsigned DEFAULT_SERVER_PORT = 1997;
    const std::string DEFAULT_CONFPATH = "/conf";
    const std::string DEFAULT_LOGPATH = "/logs";

    std::string usage()
    {
        std::string ret = "Run an IRC server.\n\n";
        ret += "USAGE: server [options]\n";
        ret += "OPTIONS\n";

        ret += "\t-p SERVER_PORT\n";
        ret += "\t\t Port on server to connect to\n";
        ret += "\t\t Default will be loaded from server config file, or "+ std::to_string(DEFAULT_SERVER_PORT) + " if no config file\n";

        ret += "\t-c CONFIG_FILEPATH\n";
        ret += "\t\t Directory for server configuration\n";
        ret += "\t\t If none specified, default config from " + DEFAULT_CONFPATH + " will be used\n";

        ret += "\t-L LOG_FILEPATH\n";
        ret += "\t\t Directory for log files\n";
        ret += "\t\t Defaults to " + DEFAULT_LOGPATH + "\n";

        return ret;
    }

    std::string err_bad_arg(char selected)
    {
        std::string ret = "UNRECOGNIZED PROGRAM ARGUMENT: -" + selected;
        return ret;
    }

    void process_and_wait()
    {
    }

}

int main(int argc, char **argv)
{
    if(argc == 1)
        std::cout << "Running with default options" << std::endl;

    int opt;
    while ((opt = getopt(argc, argv, "p:c:L:")) != -1)
    {
        switch(opt) {
            case 'p':
                std::cout << "Port argument" << std::endl;
                break;
            case 'c':
                std::cout << "Config argument" << std::endl;
                break;
            case 'L':
                std::cout << "Log argument" << std::endl;
                break;
            case 'H':
                std::cout << IRC_Server::usage() << std::endl;
                exit(0);
            default:
                std::cerr << IRC_Server::err_bad_arg(opt) << std::endl;
                std::cout << IRC_Server::usage() << std::endl;
                exit(1);
        }
    }

    std::cout << "Starting the server..." << std::endl;
    IRC_Server::TCP_Server_Socket socket1(IRC_Server::DEFAULT_SERVER_PORT);

    if(socket1.init() != 0)
    {
        std::cerr << "COULDN'T CREATE SOCKET" << std::endl;
        exit(1);
    }

    std::cout << "Socket initialized, binding..." << std::endl;
    socket1.bind_socket();

    std::cout << "Socket bound. Preparing to listen..." << std::endl;
    socket1.listen_socket();

    std::cout << "Waiting for clients to connect..." << std::endl;

    IRC_Server::process_and_wait();

    std::cout << "Shutting down server" << std::endl;

}
