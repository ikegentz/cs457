//
// Created by ikegentz on 10/22/18.
//

#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>

#include "tcp_server_socket.h"
#include "tcp_user_socket.h"

namespace IRC_Server
{
    const static unsigned DEFAULT_SERVER_PORT = 1997;
    const static std::string DEFAULT_CONFPATH = "/conf";
    const static std::string DEFAULT_LOGPATH = "/logs";
    static std::vector<std::unique_ptr<thread>> threadList;
    static bool ready = true;
    static unsigned threadID = 0;

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

    int cclient(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, int id)
    {
        std::cout << "Waiting for message from the client thread with ID " << id << "..." << std::endl;
        std::string msg;
        ssize_t val;
        bool cont = true;

        while(cont)
        {
            tie(msg, val) = clientSocket.get()->recvString();
            if(msg.substr(0,4) == "EXIT")
                cont = false;

            std::cout << "[SERVER_CLIENT_LISTENER] The client is sending message " << msg << " -- With value return = " << val << std::endl;
            std::string s = "[SERVER_CLIENT_LISTENER] The client is sending message: " + msg + "\n";
            thread childT1(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);

            childT1.join();

            // I think this means you recieved a message from another server, in which case we shut this thread down...
            if(msg.substr(0, 6) == "SERVER")
            {
                thread childTExit(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "GOODBYE EVERYONE", false);
                thread childTExit2(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "\n", false);
                ready = false;
                cont = false;
                childTExit.join();
                childTExit2.join();
            }
            else
            {
                std::cout << "Waiting for another message..." << std::endl;
            }
        }
        clientSocket.get()->sendString("goodbye");
        clientSocket.get()->closeSocket();
        return 1;
    }

    void process_and_wait(TCP_Server_Socket* serverSocketPointer)
    {
        TCP_Server_Socket serverSocket = *serverSocketPointer;
        while(ready)
        {
            shared_ptr<IRC_Server::TCP_User_Socket> clientSocket;
            int val;
            tie(clientSocket, val) = serverSocket.acceptSocket();
            std::cout << "Value for accept is: " << val << std::endl;
            std::cout << "Socket Accepted" << std::endl;

            unique_ptr<thread> t = make_unique<thread>(cclient, clientSocket, threadID);
            threadList.push_back(std::move(t));
            ++threadID;
        }

        for (auto& t : threadList)
        {
            t.get()->join();
        }

        std::cout << "Server is shutting down after one client" << std::endl;
    }
    void process_server_commands()
    {
        char input_cstr[256];
        std::cout << "\n[SERVER_COMMANDLET] Type server commands here:" << std::endl;
        std::cout << "\tEXIT - Shut down the server\n" <<
                  "\tUSERS - List currently connected users\n" <<
                  "\tCHANNELS - List channels and number of users\n" <<
                  "\tKICK - Kick a user from the server" << std::endl;

        std::string input;
        do
        {
            std::cin.getline(input_cstr, 256);
            input = std::string(input_cstr);

            // convert command to all lowercase
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);
        } while (input.find("exit") != 0); // 'exit' command was typed
        std::cout << "[SERVER_COMANDLET] Shutting down the server." <<
        " Currently connected users will have their chat session dropped!" << std::endl;
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

    std::cout << "Starting the server...\n" << std::endl;
    IRC_Server::TCP_Server_Socket socket1(IRC_Server::DEFAULT_SERVER_PORT);

    if(socket1.init() != 0)
    {
        std::cerr << "COULDN'T CREATE SOCKET" << std::endl;
        exit(1);
    }

    std::cout << "[SERVER] Socket initialized, binding..." << std::endl;
    socket1.bind_socket();

    std::cout << "[SERVER] Socket bound. Preparing to listen..." << std::endl;
    socket1.listen_socket();

    std::cout << "[SERVER] Waiting for clients to connect..." << std::endl;

    // we want to process clients, and accept commands concurrently
    std::thread clientListener(IRC_Server::process_and_wait, &socket1);
    std::thread serverCommandProcessor(IRC_Server::process_server_commands);

    // handle client connections
    clientListener.join();
    // handle user input commands
    serverCommandProcessor.join();

    std::cout << "[SERVER] Shutting down server" << std::endl;
}
