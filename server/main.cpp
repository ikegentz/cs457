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

    static std::unordered_map<std::string, int> socketLookup;
    static std::mutex socketLookup_mutex;

    static bool ready = true;
    static bool still_running = true;
    static unsigned threadID = 0;

    static std::map<std::string, IRC_Server::User> users;
    static std::mutex users_mutex;

    static std::map<int, bool> threadState;

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

    void kick_user(std::string user)
    {
        if(users.find(user) == users.end())
        {
            std::cout << "Couldn't kick " << user << ". They aren't connected" << std::endl;
            return;
        }

        std::lock_guard<std::mutex> guard(socketLookup_mutex);
        int socketFD = socketLookup.find(user)->second;

        std::string to_send = "[SERVER] You've been dropped\n";

        // send to next user in the channel
        std::lock_guard<std::mutex> guard2(clientSockets_mutex);
        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second.get(), to_send, false);
        sendOthersUserThread.join();

        // remove user's socket
        int fdLookup = socketLookup.find(user)->second;
        clientSockets.erase(fdLookup);

        // remove user from channel
        std::string curChannel = users.find(user)->second.current_channel;
        std::lock_guard<std::mutex> guard3(channels_mutex);
        channels.find(curChannel)->second.erase(user);

        threadState.find(fdLookup)->second = false;

        //remove this user from the user's list
        std::lock_guard<std::mutex> guard4(users_mutex);
        users.erase(user);
    }

    void add_user_to_channel(std::string channel_name, std::string nickname)
    {
        std::lock_guard<std::mutex> guard(channels_mutex);

        // channel exists, so we add user to it
        if(channels.find(channel_name) != channels.end())
        {
            // get the iterator
            auto channel = channels.find(channel_name);

            // sets are unique, so we can just insert here. If user is already in this channel, simply won't be inserted again
            channel->second.insert(nickname);

            std::cout << "[SERVER] User " << nickname << " joined channel #" << channel_name << ". Number of users in channel: " << channels.find(channel_name)->second.size() << std::endl;
        }
        else
        {
            std::set<std::string> channel;
            channel.insert(nickname);
            channels.insert(std::pair<std::string, std::set<std::string>>(channel_name, channel));
            std::cout << "[SERVER] User " << nickname << " joined channel #" << channel_name << ". Number of users in channel: " << channel.size() << std::endl;

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

        User user(nick, ip_addr, port, host, "general");

        // user already exists. We will return and not add this user
        if(users.find(nick) != users.end())
        {
            std::cout << "[SERVER] " << user.to_string() << " tried to connect. That nickname was already in use." << std::endl;

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
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "[SERVER] PONG\n", true);
        sendThread.join();
    }

    void quit_command(bool& cont, std::string nickname, std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::cout << "\tRecieved the 'QUIT' command from the client. Closing this client's connection..." << std::endl;
        cont = false;
        std::string s = "You are being disconnected per your request\n";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);
        sendThread.join();

        std::lock_guard<std::mutex> guard2(clientSockets_mutex);
        clientSocket.get()->closeSocket();

        kick_user(nickname);

        std::cout << "\tSuccessfully closed the client\n";
    }

    void server_shutdown_command(bool& ready, bool& cont, bool& still_running, std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::cout << "\tDetected another server. Shutting down to avoid cyclic behavior..." << std::endl;
        thread childTExit(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), "GOODBYE EVERYONE\n", false);
        ready = false;
        cont = false;
        still_running = false;
        childTExit.join();
    }

    void process_general_message(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string nick, std::string message)
    {
        // send blank acknowledgement back to client we recieved message from
        std::string s = "\n";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, false);
        sendThread.join();

        std::string cur_channel = users.find(nick)->second.current_channel;
        auto users_send_to = channels.find(cur_channel)->second;
        std::cout <<"[SERVER] " << nick << " on #" << cur_channel << " - " << message << std::endl;
        std::cout << "[SERVER] Relaying to " << users_send_to.size()-1 << " users on #" << cur_channel << std::endl;


        for(std::string cur_user : users_send_to)
        {
            // don't send this message back to the same user
            if(nick.compare(cur_user) == 0)
                continue;


            std::lock_guard<std::mutex> guard(socketLookup_mutex);
            int socketFD = socketLookup.find(cur_user)->second;

            std::string to_send = nick + " on #" + cur_channel + ": " + message + "\n";

            // send to next user in the channel
            std::lock_guard<std::mutex> guard2(clientSockets_mutex);
            thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second.get(), to_send, false);
            sendOthersUserThread.join();
        }
    }

    void join_new_channel(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string channel_name, std::string user)
    {
       // std::lock_guard<std::mutex> guard(channels_mutex);
        // remove user from current channel
        std::string curChan = users.find(user)->second.current_channel;
        channels.find(curChan)->second.erase(user);

        std::cout << user << " left #" << curChan << " and joined #" << channel_name << std::endl;

        // add user to new channel
        add_user_to_channel(channel_name, user);

        //set users new channel
        users.find(user)->second.current_channel = channel_name;

        std::string s = "[SERVER] You left #" + curChan + " and joined #" + channel_name + "\n";

        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);
        sendThread.join();
    }

    void client_list_command(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string s = "[SERVER] All channels on the server - ";
        for(auto it=channels.begin(); it != channels.end(); it++)
        {
            s += "#" + it->first + ", ";
        }
        s += "\n";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);
        sendThread.join();
    }

    int cclient(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, int threadID)
    {
        std::string msg;
        ssize_t val;
        //bool cont = true;
        std::string nickname;

        // don't block, so that we can check read values and exit properly

        while(threadState.find(threadID)->second)
        {
            // check if the server is still running
            if(!still_running)
                threadState.find(threadID)->second = false;

            tie(msg, val) = clientSocket.get()->recvString();

            // we will have a timeout if we dont' hear anything. In that case, continue on and try again
            if(val <= 0)
                continue;

            if(msg.substr(0,4) == "QUIT")
            {
                quit_command(threadState.find(threadID)->second, nickname, clientSocket);
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
                server_shutdown_command(ready, threadState.find(threadID)->second, still_running, clientSocket);
            }
            else if(msg.substr(0, 4) == "USER")
            {
                nickname = user_command(msg, clientSocket);
                std::lock_guard<std::mutex> guard(socketLookup_mutex);
                socketLookup.insert(std::pair<std::string, int>(nickname, threadID));
            }
            else if(msg.substr(0,4) == "PING")
            {
                ping_command(clientSocket);
            }
            else if(msg.substr(0,4) == "JOIN")
            {
                std::vector<std::string> tokens;
                Utils::tokenize_line(msg, tokens);
                join_new_channel(clientSocket, tokens[1], nickname);
            }
            else if(msg.substr(0,4) == "LIST")
            {
                client_list_command(clientSocket);
            }
            else
            {
                // This is where we simply recieved a chat message. Broadcast to the channel...
                // send acknowledgement to client
                process_general_message(clientSocket, nickname, msg);
            }

        }

        // TODO this cleanup doesn't work quite right :/
        clientSocket.get()->sendString("Goodbye!");
        clientSocket.get()->closeSocket();

        // remove user's socket
        int fdLookup = socketLookup.find(nickname)->second;
        clientSockets.erase(fdLookup);

        // remove user from channel
        std::string curChannel = users.find(nickname)->second.current_channel;
        std::lock_guard<std::mutex> guard3(channels_mutex);
        channels.find(curChannel)->second.erase(nickname);

        threadState.find(fdLookup)->second = false;

        //remove this user from the user's list
        std::lock_guard<std::mutex> guard4(users_mutex);
        users.erase(nickname);

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

            std::cout << "[SERVER] Making new thread to handle this connection with ID: " << threadID << std::endl << std::endl;

            std::lock_guard<std::mutex> guard(clientSockets_mutex);
            clientSockets[threadID] = clientSocket;

            unique_ptr<thread> t = make_unique<thread>(cclient, clientSockets.find(threadID)->second, threadID);
            threadList.push_back(std::move(t));
            threadState.insert(std::pair<int, bool>(threadID, true));
            ++threadID;
        }

        quitThreads:;
        for (auto& t : threadList)
        {
            t.get()->join();
        }

        std::cout << "[SERVER] Server shutting down, all clients disconnected" << std::endl;
    }

    void server_users_command()
    {
        std::cout << "[SERVER] All users currently connected: " << std::endl;

        std::lock_guard<std::mutex> guard(users_mutex);
        for(auto it = users.begin(); it != users.end(); ++it)
        {
            std::cout << "\t" << it->first << "@" << it->second.ip_address << ":" << std::to_string(it->second.port) << std::endl;
        }
    }

    void server_channels_command()
    {
        std::cout << "[SERVER] List of channels: " << std::endl;

        std::lock_guard<std::mutex> guard(channels_mutex);
        for(auto it = channels.begin(); it != channels.end(); ++it)
        {
            std::cout << "\t" << it->first << std::endl;
        }
    }

    void process_server_commands()
    {
        std::cout << "\n[SERVER] Control the server" << std::endl;
        std::cout << "\tEXIT - Shut down the server\n" <<
                  "\tUSERS - List currently connected users\n" <<
                  "\tCHANNELS - List channels and number of users\n" <<
                  "\tKICK - Kick a user from the server" << std::endl;

        std::cout << "\n\tType server commands here:" << std::endl;


        char input_cstr[256];
        std::string input;
        do
        {
            std::cin.getline(input_cstr, 256);
            input = std::string(input_cstr);

            // convert command to all lowercase
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);

            if(input.find("/users") != std::string::npos)
                server_users_command();
            else if(input.find("/channels") != std::string::npos)
                server_channels_command();
            else if(input.find("/kick") != std::string::npos)
            {
                if(input.size() < 6)
                    std::cout << "[SERVER] Provide a username to kick" << std::endl;
                else
                {
                    std::vector<std::string> tokens;
                    Utils::tokenize_line(input, tokens);
                    kick_user(tokens[1]);
                }
            }
            else
                std::cout << "[SERVER] Unrecognized command '" << input << "'" << std::endl;

        } while (input.find("/exit") != 0); // 'exit' command was typed
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
