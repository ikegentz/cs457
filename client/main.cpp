//
// Created by ikegentz on 10/22/18.
//

#include <unistd.h>
#include <iostream>
#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <limits.h>

#include "tcp_client_socket.h"
#include "client_command_processor.h"
#include "client_globals.h"

#define DEBUG true

namespace IRC_Client
{
    std::queue<std::string> message_queue;
    std::mutex message_queue_mutex;

    static bool communicator_running = true;

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

    // we only want to use the mutex lock for the pop(), so put it in a function so it goes out of scope
    std::string next_message()
    {
        std::lock_guard<std::mutex> guard(IRC_Client::message_queue_mutex);
        std::string message = message_queue.front(); // get the message
        message_queue.pop(); // remove it

        return message;
    }

    void communicate_with_server(TCPClientSocket* clientSocket)
    {
        // process any messages that haven't been sent yet
        while(communicator_running || !IRC_Client::message_queue.empty())
        {
            if(message_queue.empty())
            {
                // sleep for a bit before checking if there's something to send
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            std::string message = next_message();

            // loop of trying to send message
            ssize_t v;
            do
            {
                clientSocket->sendString(message, false);

                std::string msg;
                tie(msg, v) = clientSocket->recvString(4096, false);

                // print response from server
                if(v > 0)
                    std::cout << msg << std::endl;
            } while(v <= 0);
        }
    }

    void process_client_commands()
    {
        static bool RUNNING = true;

        char input_cstr[256];
        std::string input;
        do
        {
            std::cout << "\n\t$ ";
            std::cin.getline(input_cstr, 256);
            input = std::string(input_cstr);
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);

            std::string outgoing_msg = IRC_Client::build_outgoing_message(input, RUNNING);

            // push a message onto the queue to be sent by the socket thread
            std::lock_guard<std::mutex> guard(IRC_Client::message_queue_mutex);
            message_queue.push(outgoing_msg);

#if DEBUG == true
            std::cout << outgoing_msg << std::endl;
#endif
        } while (RUNNING);
        communicator_running = false;
    }

    void initial_connect_message(std::string nickname)
    {
        // send initial connection information
        char hostname[1024];
        char username[1024];
        gethostname(hostname, 1024);
        getlogin_r(username, LOGIN_NAME_MAX);

        std::string initial_connection = "USER " + nickname + " " + hostname + " IKE_SERVER " + " :" + username;
        std::lock_guard<std::mutex> guard(IRC_Client::message_queue_mutex);
        IRC_Client::message_queue.push(initial_connection);
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
    std::string nickname = IRC_Client::DEFAULT_USERNAME;
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

    std::cout << "[CLIENT] Connecting to server " << serverIP << " on port " << port << std::endl;

    IRC_Client::TCPClientSocket clientSocket(serverIP,port);

    int val = clientSocket.connectSocket();
    if(val == -1)
    {
        std::cerr << "[CLIENT] Couldn't connect to the server. Check hostname/IP and port" << std::endl;
        exit(1);
    }

    std::cout << "[CLIENT] You are now connected to the server" << std::endl;

    IRC_Client::initial_connect_message(nickname);

    // main loops. receive typed input
    std::thread commandListener(IRC_Client::process_client_commands);
    // communicate with the server
    std::thread communicatorThread(IRC_Client::communicate_with_server, &clientSocket);

    commandListener.join();
    communicatorThread.join();

    clientSocket.closeSocket();

    return 0;

}