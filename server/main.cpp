//
// Created by ikegentz on 10/22/18.
//

#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <time.h>

#include "tcp_server_socket.h"
#include "tcp_user_socket.h"
#include "user.h"
#include "server_info.h"
#include "../utils/string_ops.h"

#define DEBUG false

namespace IRC_Server
{
    static ServerInfo serverInfo;
    const static unsigned DEFAULT_SERVER_PORT = 1997;
    const static std::string DEFAULT_CONFPATH = "server.conf";
    const static std::string DEFAULT_LOGPATH = "server.log";
    const static std::string DEFAULT_DB_PATH = "db/";

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

    std::string get_current_day()
    {
        time_t theTime = time(NULL);
        struct tm *aTime = localtime(&theTime);
        int day = aTime->tm_mday;
        int month = aTime->tm_mon + 1;
        int year = aTime->tm_year + 1990;

        return std::to_string(month) + "/" + std::to_string(day) + "/" + std::to_string(year);
    }

    std::string get_current_time()
    {
        time_t theTime = time(NULL);
        struct tm *aTime = localtime(&theTime);
        int hour = aTime->tm_hour;
        int min = aTime->tm_min;
        int sec = aTime->tm_sec;

        return std::to_string(hour) + "hh:" + std::to_string(min) + "mm:" + std::to_string(sec) + "ss";
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

    void kill_user(std::string user, std::string drop_message="")
    {
        if(users.find(user) == users.end())
        {
            std::cout << "Couldn't kill " << user << ". They aren't connected" << std::endl;
            return;
        }

        std::lock_guard<std::mutex> guard(socketLookup_mutex);
        int socketFD = socketLookup.find(user)->second;

        // notify user that they were removed
        if(drop_message.size() > 1)
        {
            std::lock_guard<std::mutex> guard2(clientSockets_mutex);
            thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second.get(), drop_message, false);
            sendOthersUserThread.join();
        }

        // remove user's socket
        clientSockets.erase(socketFD);
        socketLookup.erase(user);

        // remove user from channel
        std::string curChannel = users.find(user)->second.current_channel;
        std::lock_guard<std::mutex> guard3(channels_mutex);
        channels.find(curChannel)->second.erase(user);

        threadState.find(socketFD)->second = false;

        //remove this user from the user's list
        std::lock_guard<std::mutex> guard4(users_mutex);
        users.erase(user);
    }

    void send_to_channel(std::string nick, std::string message, std::string channel)
    {
        if(channels.find(channel) == channels.end())
            std::cout << "[SERVER] Couldn't find that channel to relay message to " << std::endl;

        auto users_send_to = channels.find(channel)->second;
        if(users_send_to.size())

            for(std::string cur_user : users_send_to)
            {
                // don't send this message back to the same user
                if(nick.compare(cur_user) == 0)
                    continue;


                std::lock_guard<std::mutex> guard(socketLookup_mutex);
                int socketFD = socketLookup.find(cur_user)->second;

                // send to next user in the channel
                std::lock_guard<std::mutex> guard2(clientSockets_mutex);
                thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second.get(), message, false);
                sendOthersUserThread.join();
            }
    }


    void join_new_channel(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string channel_name, std::string user)
    {
        // std::lock_guard<std::mutex> guard(channels_mutex);
        // remove user from current channel
        std::string curChan = users.find(user)->second.current_channel;
        channels.find(curChan)->second.erase(user);

        std::cout << "[SERVER] " << user << " left #" << curChan << " and joined #" << channel_name << std::endl;

        // add user to new channel
        add_user_to_channel(channel_name, user);

        //set users new channel
        users.find(user)->second.current_channel = channel_name;

        std::string s = "[SERVER] You left #" + curChan + " and joined #" + channel_name + "\n";

        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, true);
        sendThread.join();

        std::string to_channel = user + " left #" + curChan + "\n";
        send_to_channel(user, to_channel, curChan);
    }

    void kick_user(std::string calling_user, std::string user)
    {
        if(calling_user != "[SERVER]")
        {
            // make sure calling user is actually part of the channel
            User kicker = users.find(calling_user)->second;
            User kickee = users.find(user)->second;

            if(kicker.current_channel != kickee.current_channel)
            {
                std::cout << "Couldn't kick " << user << ". They aren't in the same channel as the kick initiator" << std::endl;
                return;
            }
        }

        if(users.find(user) == users.end())
        {
            std::cout << "Couldn't kick " << user << ". They aren't connected" << std::endl;
            return;
        }

        int socketFD = socketLookup.find(user)->second;

        std::string s = "You've been kicked from your current channel!\n";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second, s, true);
        sendThread.join();

        join_new_channel(clientSockets.find(socketFD)->second, "general", user);

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

        kill_user(nickname);

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
        std::cout <<"[SERVER] " << nick << " on #" << cur_channel << " - " << message << std::endl;

        std::string to_send = nick + " on #" + cur_channel + ": " + message + "\n";
        send_to_channel(nick, to_send, cur_channel);
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

    void send_priv_message(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string message, std::string sending_user)
    {
        // send blank acknowledgement back to client we recieved message from
        std::string s = "\n";
        thread sendThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket.get(), s, false);
        sendThread.join();

        std::vector<std::string> tokens;
        Utils::tokenize_line(message, tokens);

        std::string to_send = sending_user + " on #" + users.find(sending_user)->second.current_channel + ": ";
        for(unsigned i = 2; i < tokens.size(); ++i)
            to_send += tokens[i] + " ";
        to_send += "\n";

        if(tokens[1].find("#") != std::string::npos)
        {
            std::string cur_channel = tokens[1].substr(1);

            std::cout << "[SERVER] Relaying private message from " << sending_user << " to #" << cur_channel << std::endl;
            send_to_channel(sending_user, to_send, cur_channel);
        }
        else
        {
            std::lock_guard<std::mutex> guard(socketLookup_mutex);

            if(socketLookup.find(tokens[1]) == socketLookup.end())
            {
                std::cout << "[SERVER] Couldn't find that user to send message to" << std::endl;
                return;
            }

            std::cout << "[SERVER] Relaying private message from " << sending_user << " to " << tokens[1] << std::endl;

            int socketFD = socketLookup.find(tokens[1])->second;
            // send to next user in the channel
            std::lock_guard<std::mutex> guard2(clientSockets_mutex);
            thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSockets.find(socketFD)->second.get(), to_send, false);
            sendOthersUserThread.join();
        }
    }

    void send_server_info(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string to_send = "[SERVER] " + serverInfo.to_string() + "\n";
        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();
    }

    void send_server_time(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string to_send = "[SERVER] " + IRC_Server::get_current_time() + "\n";
        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();
    }

    void send_user_ip(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string message)
    {
        std::vector<std::string> tokens;
        Utils::tokenize_line(message, tokens);

        std::string to_send;
        if(users.find(tokens[1]) == users.end())
            to_send = "[SERVER] Sorry, that user isn't logged in right now\n";
        else
            to_send = "[SERVER] " + tokens[1] + " - " + users.find(tokens[1])->second.ip_address + "\n";

        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();

    }

    void send_user_host_info(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket, std::string message)
    {
        std::vector<std::string> tokens;
        Utils::tokenize_line(message, tokens);

        std::string to_send;
        if(users.find(tokens[1]) == users.end())
            to_send = "[SERVER] Sorry, that user isn't logged in right now\n";
        else
            to_send = "[SERVER] " + tokens[1] + " - " + users.find(tokens[1])->second.to_string() + "\n";

        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();
    }


    void send_server_version(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string to_send = "[SERVER] " + serverInfo.version + "\n";
        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();
    }

    void send_users_info(std::shared_ptr<IRC_Server::TCP_User_Socket> clientSocket)
    {
        std::string to_send = "[SERVER] ";
        for(auto it = users.begin(); it != users.end(); ++it)
        {
            to_send += it->second.to_string() + " - ";
        }
        to_send += "\n";
        thread sendOthersUserThread(&IRC_Server::TCP_User_Socket::sendString, clientSocket, to_send, false);
        sendOthersUserThread.join();
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
            else if(msg.substr(0,8) == "USERHOST")
            {
                send_user_host_info(clientSocket, msg);
            }
            else if(msg.substr(0,6) == "USERIP")
            {
                send_user_ip(clientSocket, msg);
            }
            else if(msg.substr(0,5) == "USERS")
            {
                send_users_info(clientSocket);
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
            else if(msg.substr(0,7) == "PRIVMSG")
            {
                send_priv_message(clientSocket, msg, nickname);
            }
            else if(msg.substr(0,4) == "INFO")
            {
                send_server_info(clientSocket);
            }
            else if(msg.substr(0,4) == "KILL")
            {
                std::vector<std::string> tokens;
                Utils::tokenize_line(msg, tokens);
                kill_user(tokens[1], "You have been killed by " + nickname);
            }
            else if(msg.substr(0,4) == "KICK")
            {
                std::vector<std::string> tokens;
                Utils::tokenize_line(msg, tokens);
                kick_user(nickname, tokens[1]);
            }
            else if(msg.substr(0,4) == "TIME")
            {
                send_server_time(clientSocket);
            }
            else if(msg.substr(0,7) == "VERSION")
            {
                send_server_version(clientSocket);
            }
            else
            {
                // This is where we simply recieved a chat message. Broadcast to the channel...
                // send acknowledgement to client
                process_general_message(clientSocket, nickname, msg);
            }

        }

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
                  "\tKILL - kill a user from the server" << std::endl;

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
            else if(input.find("/kill") != std::string::npos)
            {
                if(input.size() < 6)
                    std::cout << "[SERVER] Provide a username to kill" << std::endl;
                else
                {
                    std::vector<std::string> tokens;
                    Utils::tokenize_line(input, tokens);
                    kill_user(tokens[1], "[SERVER] You have been killed!\n");
                }
            }
            else if(input.find("/kick") != std::string::npos)
            {
                if(input.size() < 6)
                    std::cout << "[SERVER] Provide a username to kill" << std::endl;
                else
                {
                    std::vector<std::string> tokens;
                    Utils::tokenize_line(input, tokens);
                    kick_user("[SERVER]", tokens[1]);
                }
            }
            else
                std::cout << "[SERVER] Unrecognized command '" << input << "'" << std::endl;

        } while ((input.find("/exit") != 0) && (input.find("/quit") != 0)); // 'exit' command was typed
        still_running = false;
    }

    void shutdown_server()
    {
        std::cout << "[SERVER_COMANDLET] Shutting down the server." <<
                  " Current client connections will be closed." << std::endl;

        // notify all clients that the server is shutting down
        std::lock_guard<std::mutex> guard(channels_mutex);
        std::string disconnectMessage = "[SERVER] Server has been shutdown. You wont' be able to communicate any longer!\n";
        for (auto it = channels.begin(); it != channels.end(); ++it)
        {
            //              this will simply be ignored
            //                   ^
            send_to_channel("BAD-USERNAME", disconnectMessage, it->first);
        }
    }

    void load_config_file(std::string config_path, int& port, std::string& db_path, std::string& log_path, bool& debug_enabled, bool& should_log)
    {
        std::ifstream in(config_path);

        if(!in) {
            std::cout << "Couldn't open config file. Running with defaults and command line args" << std::endl;
            return;
        }

        std::cout << "Loaded options from " << config_path << std::endl;

        std::string str;
        while (std::getline(in, str)) {
            // skip comments
            if((str.find("#") != std::string::npos) || (str.size() <= 1))
                continue;

            std::vector<std::string> tokens;
            Utils::tokenize_line(str, tokens);

            if(tokens.size() != 2)
                continue;

            if(tokens[0].find("port") != std::string::npos)
            {
                port = stoi(tokens[1]);
                std::cout << "Port from config file: " << tokens[1] << std::endl;
            }
            else if(tokens[0].find("default_debug_mode") != std::string::npos)
            {
                std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

                if(tokens[1].find("true") != std::string::npos)
                    debug_enabled = true;
                else debug_enabled = false;

                std::cout << "Default debug mode from config file: " << debug_enabled << std::endl;
            }
            else if(tokens[0].find("default_log_file") != std::string::npos)
            {
                log_path = tokens[1];
                std::cout << "Log path specified in config file: " << log_path << std::endl;
            }
            else if(tokens[0].compare("log") == 0)
            {
                std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

                if(tokens[1].find("true") != std::string::npos)
                    should_log = true;
                else should_log = false;

                std::cout << "Will log this sesssion? (specified in config file): " << should_log << std::endl;
            }
            else if(tokens[0].compare("dbpath") == 0)
            {
                db_path = tokens[1];
                std::cout << "Grabbing further config info from " << db_path << "*" << std::endl;
            }
        }
    }
}


int main(int argc, char **argv)
{
    int port = IRC_Server::DEFAULT_SERVER_PORT;
    std::string log_path = IRC_Server::DEFAULT_LOGPATH;
    bool debug_enabled = DEBUG;
    bool should_log = true;
    std::string db_path = IRC_Server::DEFAULT_DB_PATH;

    if(argc == 1)
        std::cout << "[SERVER] Running with default options" << std::endl;

    std::string config_file = IRC_Server::DEFAULT_CONFPATH;
    int opt;
    while ((opt = getopt(argc, argv, "p:c:L:")) != -1)
    {
        switch(opt)
        {
            case 'c':
                config_file = optarg;
                break;
        }
    }

    IRC_Server::load_config_file(config_file, port, db_path, log_path, debug_enabled, should_log);

    optind = 1;
    while ((opt = getopt(argc, argv, "p:c:L:")) != -1)
    {
        switch(opt) {
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'L':
                log_path = optarg;
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


    IRC_Server::serverInfo.start_time = IRC_Server::get_current_time() + " on " + IRC_Server::get_current_day() ;
    IRC_Server::serverInfo.patch_level = "2.2";
    IRC_Server::serverInfo.version = "1.0";

    std::cout << "Starting the server...\n" << std::endl;
    IRC_Server::TCP_Server_Socket socket1(port);

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
