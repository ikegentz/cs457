//
// Created by ikegentz on 10/22/18.
//

#include <unistd.h>
#include <iostream>

#include "tcp_client_socket.h"


namespace IRC_Client
{
    const std::string DEFAULT_HOSTNAME = "localhost";
    const std::string DEFAULT_USERNAME = "chatterbox-charlie";
    const unsigned DEFAULT_SERVER_PORT = 1997;
    const std::string DEFAULT_LOGPATH = "client.log";

    std::string usage()
    {
        std::string ret = "Connect to an IRC chat server.\n\n";
        ret += "USAGE: client [options]\n";
        ret += "OPTIONS\n";

        ret += "\t-h HOSTNAME\n";
        ret += "\t\t Hostname of server to connect to\n";
        ret += "\t\t Default is " + DEFAULT_HOSTNAME + "\n";

        ret += "\t-u USERNAME\n";
        ret += "\t\t Username to connect as (i.e. your username)\n";
        ret += "\t\t Default is " + DEFAULT_USERNAME + "\n";

        ret += "\t-p SERVER_PORT\n";
        ret += "\t\t Port on server to connect to\n";
        ret += "\t\t Default will be loaded from server config file, or "+ std::to_string(DEFAULT_SERVER_PORT) + " if no config file\n";

        ret += "\t-c CONFIG_FILEPATH\n";
        ret += "\t\t File with configuration options\n";
        ret += "\t\t If none specified, default options will be used\n";

        ret += "\t-t TEST_FILEPATH\n";
        ret += "\t\t Test file to run sample commands\n";
        ret += "\t\t If none specified, will run in interactive mode\n";

        ret += "\t-L LOG_FILEPATH\n";
        ret += "\t\t File to write logs\n";
        ret += "\t\t Defaults to " + DEFAULT_LOGPATH + "\n";

        return ret;
    }

    std::string err_bad_arg(char selected)
    {
        std::string ret = "UNRECOGNIZED PROGRAM ARGUMENT: -" + selected;
        return ret;
    }

}
int main(int argc, char **argv)
{
    if(argc == 1)
        std::cout << "Running with default options" << std::endl;

    int opt;
    char selected_opt = '.';
    char* arg;


    std::cout << "ARGS: ";
    while ((opt = getopt(argc, argv, "h:u:p:c:t:L:")) != -1)
    {
        switch(opt) {
            case 'h':
                selected_opt = 'h';
                arg = optarg;
                break;
            case 'u':
                selected_opt = 'u';
                arg = optarg;
                break;
            case 'p':
                selected_opt = 'p';
                arg = optarg;
                break;
            case 'c':
                selected_opt = 'c';
                arg = optarg;
                break;
            case 't':
                selected_opt = 't';
                arg = optarg;
                break;
            case 'L':
                selected_opt = 'L';
                arg = optarg;
                break;
            case 'H':
                selected_opt = 'H';
                std::cout << IRC_Client::usage() << std::endl;
                exit(0);
            default:
                std::cerr << IRC_Client::err_bad_arg(selected_opt) << std::endl;
                std::cout << IRC_Client::usage() << std::endl;
                exit(1);
        }
        std::cout << ", " << arg;
    }
    std::cout << std::endl;

    std::cout << "Starting the client..." << std::endl;

    std::string serverIP("127.0.0.1");
    int port = 1997;
    IRC_Client::TCPClientSocket clientSocket(serverIP,port);

    int val = clientSocket.connectSocket();
    std::cout << "Client Socket Value after connect = " << val << std::endl;

    clientSocket.sendString("Hello Server. How are you? ",false);

    std::string msg;
    ssize_t v;
    tie(msg,v) =  clientSocket.recvString(4096,false);
    std::cout << "server said: " << msg << std::endl;

    std::cout << "Client will try to exit now" << std::endl;

    clientSocket.sendString("EXIT",false);

    tie(msg,v) =  clientSocket.recvString(4096,false);
    std::cout << "server said: " << msg << std::endl;

    clientSocket.closeSocket();

    return 0;

}