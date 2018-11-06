//
// Created by ikegentz on 10/22/18.
//

#include <unistd.h>
#include <iostream>

#include "tcp_client_socket.h"


namespace IRC_Client
{
    const std::string DEFAULT_HOSTNAME = "127.0.0.1";
    const std::string DEFAULT_USERNAME = "loudmouth";
    const unsigned DEFAULT_SERVER_PORT = 1997;
    const std::string DEFAULT_LOGPATH = "client.log";
    const std::string DEFAULT_CONFIG_PATH = "client.conf";
    const std::string DEFAULT_TEST_PATH = "test.txt";

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

    std::string serverIP = IRC_Client::DEFAULT_HOSTNAME;
    int port = IRC_Client::DEFAULT_SERVER_PORT;
    std::string username = IRC_Client::DEFAULT_USERNAME;
    std::string config_filepath = IRC_Client::DEFAULT_CONFIG_PATH;
    std::string test_filepath = IRC_Client::DEFAULT_TEST_PATH;
    std::string log_filepath = IRC_Client::DEFAULT_LOGPATH;

    while ((opt = getopt(argc, argv, "h:u:p:c:t:L:")) != -1)
    {
        switch(opt) {
            case 'h':
                selected_opt = 'h';
                serverIP = optarg;
                break;
            case 'u':
                selected_opt = 'u';
                username = optarg;
                break;
            case 'p':
                selected_opt = 'p';
                port = std::stoi(optarg);
                break;
            case 'c':
                selected_opt = 'c';
                config_filepath = optarg;
                break;
            case 't':
                selected_opt = 't';
                test_filepath = optarg;
                break;
            case 'L':
                selected_opt = 'L';
                log_filepath = optarg;
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
    }
    std::cout << std::endl;

    std::cout << "[CLIENT] Starting the client..." << std::endl;

    std::cout << "HOST!!!!! " << serverIP << std::endl;

    IRC_Client::TCPClientSocket clientSocket(serverIP,port);

    int val = clientSocket.connectSocket();
    if(val == -1)
    {
        std::cerr << "[CLIENT] Couldn't connect to the server. Check hostname/IP and port" << std::endl;
        exit(1);
    }
    std::cout << "[CLIENT] You are now connected to the server" << std::endl;




    //main loop here

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