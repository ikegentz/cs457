#include "mainwindow.h"
#include "tcp_client_socket.h"
#include "client_globals.h"
#include "string_ops.h"
#include "client_command_processor.h"

#include <QApplication>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <fstream>
#include <chrono>         // std::chrono::seconds
#include <limits.h>

#define DEBUG false

namespace IRC_Client
{
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

    std::string initial_connect_message(std::string nickname)
    {
        // send initial connection information
        char hostname[1024];
        char username[1024];
        gethostname(hostname, 1024);
        getlogin_r(username, LOGIN_NAME_MAX);

        std::string initial_connection = "USER " + nickname + " " + hostname + " IKE_SERVER " + " :" + username;
        return initial_connection;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    int opt;
    char selected_opt = '.';

    std::string serverIP = IRC_Client::DEFAULT_HOSTNAME;
    int port = IRC_Client::DEFAULT_SERVER_PORT;
    std::string nickname = IRC_Client::DEFAULT_USERNAME;
    std::string config_filepath = IRC_Client::DEFAULT_CONFIG_PATH;
    std::string test_filepath = IRC_Client::NO_TEST;
    bool debug_enabled = DEBUG;

    // check only for config file first, so other options can override it
    while ((opt = getopt(argc, argv, "h:u:p:c:t:L:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                selected_opt = 'c';
                config_filepath = optarg;
                break;
        }
    }

    // load options from config file (if found)
    w.load_config_file(config_filepath, serverIP, port, nickname, debug_enabled);

    // reset for the main parse
    optind = 1;
    while ((opt = getopt(argc, argv, "h:u:p:c:t:L:")) != -1)
    {
        switch(opt) {
            case 'h':
                selected_opt = 'h';
                serverIP = optarg;
                break;
            case 'u':
                selected_opt = 'u';
                nickname = optarg;
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
                w.log_path = optarg;
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

    w.print_and_log("[CLIENT] Starting the client..." );

    w.print_and_log("[CLIENT] Connecting to server " + serverIP + " on port " + std::to_string(port) );

    IRC_Client::TCPClientSocket clientSocket(serverIP,port);

    int val = clientSocket.connectSocket();
    if(val == -1)
    {
        w.print_and_log("[CLIENT] Couldn't connect to the server. Check hostname/IP and port" );
        exit(1);
    }

    //check initial connection
    std::string message = IRC_Client::initial_connect_message(nickname);
    clientSocket.sendString(message, false);
    ssize_t v;
    std::string msg;
    tie(msg, v) = clientSocket.recvString(4096, false);

    // username already taken
    if(msg.find("IN_USE") != std::string::npos)
    {
        w.print_and_log(msg );
        clientSocket.closeSocket();
        exit(0);
    }

    w.print_and_log(msg );

    // communicate with the server
    std::thread communicatorThread(&MainWindow::communicate_with_server, &w, &clientSocket);

    int guiRet = 0;
    // run test file automatically
    if(test_filepath.compare(IRC_Client::NO_TEST) != 0)
    {

        std::thread testingThread(&MainWindow::run_test_file, &w, test_filepath);
        testingThread.join();
    }
    // bring up the GUI
    else
    {
        w.show();
        guiRet = a.exec();
    }

    // need to join() for server communications AFTER we launch either testingThread or commandListener
     communicatorThread.join();

     clientSocket.closeSocket();

     w.print_and_log("[CLIENT] Terminated session!");

     return guiRet;
}
