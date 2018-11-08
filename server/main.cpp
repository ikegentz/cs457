//
// Created by ikegentz on 10/22/18.
//

#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <map>
#include <set>


#include "tcp_server_socket.h"
#include "tcp_user_socket.h"
#include "user.h"
#include "../utils/string_ops.h"

#define DEBUG false

namespace IRC_Server
{
    const static unsigned DEFAULT_SERVER_PORT = 1997;
    const static std::string DEFAULT_CONFPATH = "/conf";
    const static std::string DEFAULT_LOGPATH = "/logs";

    static std::vector<std::unique_ptr<thread>> threadList;

    static std::unordered_map<int, std::shared_ptr<IRC_Server::TCP_User_Socket>> clientSockets;
    static std::mutex clientSockets_mutex;

    static bool ready = true;
    static bool still_running = true;
    static unsigned threadID = 0;

    static std::map<std::string, IRC_Server::User> users;
    static std::mutex users_mutex;

    //             channel_name    // set of users
    static std::map<std::string, std::set<std::string>> channels;
    static std::mutex channels_mutex;

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

    void add_user_to_channel(std::string channel_name, std::string nickname)
    {
        std::lock_guard<std::mutex> guard(channels_mutex);

        // channel exists, so we add user to it
        if(channels.find(channel_name) != channels.end())
        {
            std::set<std::string> channel = channels.find(channel_name)->second;
            // sets are unique, so we can just insert here. If user is already in this channel, simply won't be inserted again
            channel.insert(nickname);
        }
        else
        {
            std::set<std::string> channel;
            channel.insert(nickname);
            channels.insert(std::pair<std::string, std::set<std::string>>(channel_name, channel));
        }
    }

    std::string user_command(std::string input, std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string ip_addr = clientSocket.get()->getAddress();
        int port = clientSocket.get()->getPort();
        std::vector<std::string> tokens;
        Utils::tokenize_line(input, tokens);

        std::string nick = tokens[1];
        std::string host = tokens[2];
        std::string srv = tokens[3];

        std::string username;
        for(unsigned i = 4; i < tokens.size(); ++i)
        {
            if(i == 4)
                tokens[i].erase(0, 1);
            username += tokens[i];
        }

        User user(nick, ip_addr, port, host);

        // user already exists. We will return and not add this user
        if(users.find(nick) != users.end())
        {
            std::cout << "[SERVER] " << user.to_string() << " tried to connect. That nickname was already in use." << std::endl <<
            "\n\t $ ";
            std::cout.flush();

            std::string to_user = "[SERVER] ERR<IN_USE> Sorry, that nickname has already been used. Please try logging in with a different username.";
            thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), to_user, true);
            sendThread.join();

            return nick;
        }

        // nickname hasn't been used yet so we will add the user
        std::lock_guard<std::mutex> guard(users_mutex);
        users.insert(std::pair<std::string, User>(nick, user));

        add_user_to_channel("general", nick);

        std::string to_user = "[SERVER] Welcome to the server! You've been added to the #general channel.";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), to_user, true);
        sendThread.join();

        return nick;
    }

    void ping_command(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "[SERVER] PONG", true);
        sendThread.join();
    }

    int cclient(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string msg;
        ssize_t val;
        bool cont = true;
        std::string nickname;

        // don't block, so that we can check read values and exit properly

        while(cont)
        {
            // check if the server is still running
            if(!still_running)
                cont = false;

            tie(msg, val) = clientSocket.get()->recvString();

            // we will have a timeout if we dont' hear anything. In that case, continue on and try again
            if(val > 0)
                std::cout << "\n[CLIENT_LISTENER] " << nickname << ": " << msg << std::endl;
            else
                continue;


            if(msg.substr(0,4) == "QUIT")
            {
                std::cout << "\tRecieved the 'QUIT' command from the client. Closing this client's connection..." << std::endl;
                cont = false;
                std::string s = "You are being disconnected per your request\n";
                thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);
                sendThread.join();

                //remove this user from the user's list
                std::lock_guard<std::mutex> guard(users_mutex);
                users.erase(nickname);

                //TODO clean up channel that user was part of

                std::lock_guard<std::mutex> guard2(clientSockets_mutex);
                clientSocket.get()->closeSocket();
                std::cout << "\tSuccessfully closed the client\n" <<
                "\n\t $ ";
                std::cout.flush();

                return 0;
            }
            else if(msg.substr(0, 6) == "SERVER")
            {
                /*
                 *    The server message is used to tell a server that the other end of a
                       new connection is a server. This message is also used to pass server
                       data over whole net.  When a new server is connected to net,
                       information about it be broadcast to the whole network.  <hopcount>
                       is used to give all servers some internal information on how far away
                       all servers are.  With a full server list, it would be possible to
                       construct a map of the entire server tree, but hostmasks prevent this
                       from being done.

                       The SERVER message must only be accepted from either (a) a connection
                       which is yet to be registered and is attempting to register as a
                       server, or (b) an existing connection to another server, in  which
                       case the SERVER message is introducing a new server behind that
                       server.

                       Most errors that occur with the receipt of a SERVER command result in
                       the connection being terminated by the destination host (target
                       SERVER).  Error replies are usually sent using the "ERROR" command
                       rather than the numeric since the ERROR command has several useful
                       properties which make it useful here.

                       If a SERVER message is parsed and attempts to introduce a server
                       which is already known to the receiving server, the connection from
                       which that message must be closed (following the correct procedures),
                       since a duplicate route to a server has formed and the acyclic nature
                       of the IRC tree broken.
                 */
                // See above comment (might be collapsed) why we exit here. In a nutshell, this creates a cycle, so
                // one server must shut down
                std::cout << "\tDetected another server. Shutting down to avoid cyclic behavior..." << std::endl;
                thread childTExit(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "GOODBYE EVERYONE\n", false);
                ready = false;
                cont = false;
                still_running = false;
                childTExit.join();
            }
            else if(msg.substr(0, 4) == "USER")
            {
                nickname = user_command(msg, clientSocket);
            }
            else if(msg.substr(0,4) == "PING")
            {
                ping_command(clientSocket);
            }
            else
            {
                // This is where we simply recieved a chat message. Broadcast to the channel...
                // send acknowledgement to client
                std::string s = "\n";
                thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, false);
                sendThread.join();

                // send chat message to channel
                std::cout << "\n\t $ ";
                std::cout.flush();
            }

        }
        clientSocket.get()->sendString("Server shutting down. Goodbye!");
        clientSocket.get()->closeSocket();

        //remove this user from the user's list
        std::lock_guard<std::mutex> guard(users_mutex);
        users.erase(nickname);

        //TODO clean up channel that user was part of
        return 1;
    }

    void process_and_wait(TCP_Server_Socket* serverSocketPointer)
    {
        TCP_Server_Socket serverSocket = *serverSocketPointer;
        while(ready)
        {
            shared_ptr<IRC_Server::TCP_User_Socket> clientSocket;
            int val;

            // TODO should use poll() for acceptSocket() to avoid busy waiting. For now... meh it works
            do
            {
                // made this non-blocking so we could check if server is still running
                tie(clientSocket, val) = serverSocket.acceptSocket();

                // check if the server is still running
                if(!still_running)
                    goto quitThreads;
            } while(val == -1);

            std::cout << "[CLIENT_LISTENER] Making new thread to handle this connection with ID: " << threadID << std::endl << std::endl;

            std::lock_guard<std::mutex> guard(clientSockets_mutex);
            clientSockets[threadID] = clientSocket;

            unique_ptr<thread> t = make_unique<thread>(cclient, clientSockets.find(threadID)->second);
            threadList.push_back(std::move(t));
            ++threadID;
        }

        quitThreads:;
        for (auto& t : threadList)
        {
            t.get()->join();
        }

        std::cout << "[CLIENT_LISTENER] Server shutting down, all clients disconnected" << std::endl;
    }

    void process_server_commands()
    {
        std::cout << "\n[COMMANDLET] Control the server" << std::endl;
        std::cout << "\tEXIT - Shut down the server\n" <<
                  "\tUSERS - List currently connected users\n" <<
                  "\tCHANNELS - List channels and number of users\n" <<
                  "\tKICK - Kick a user from the server" << std::endl;

        std::cout << "\n\tType server commands here:";


        char input_cstr[256];
        std::string input;
        do
        {
            std::cout << "\n\t$ ";
            std::cin.getline(input_cstr, 256);
            input = std::string(input_cstr);

            // convert command to all lowercase
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);
        } while (input.find("exit") != 0); // 'exit' command was typed
        still_running = false;
    }

    void shutdown_server()
    {
        std::cout << "[SERVER_COMANDLET] Shutting down the server." <<
                  " Current client connections will be closed." << std::endl;

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

    // handle user input commands, wait until user decides to exit
    serverCommandProcessor.join();

    IRC_Server::shutdown_server();

    // We probably won't get here if exit is typed, since we forcefully shutdown threads
    clientListener.join();

    std::cout << "[SERVER] Shutting down server" << std::endl;
}
