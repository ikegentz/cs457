#include "mainwindow.h"
#include "tcp_client_socket.h"
#include "client_globals.h"
#include "string_ops.h"
#include "client_command_processor.h"

#include <QApplication>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <iostream>
#include <fstream>
#include <chrono>         // std::chrono::seconds
#include <limits.h>

#define DEBUG false

namespace IRC_Client
{
    std::queue<std::string> message_queue;
    std::mutex message_queue_mutex;

    static bool communicator_running = true;
    static bool should_log = true;
    static std::string log_path = DEFAULT_LOGPATH;

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

    int load_config_file(std::string config_filepath, std::string& serverIP, int& port, std::string& nickname, bool& debug_enabled)
    {
        std::ifstream in(config_filepath);

        if(!in) {
            std::cout << "[CLIENT] Couldn't open config file. Running with defaults and command line args" << std::endl;
            return 1;
        }

        std::cout << "[CLIENT] Loaded options from " << config_filepath << std::endl;

        std::string str;
        while (std::getline(in, str)) {
            // skip comments
            if((str.find("#") != std::string::npos) || (str.size() <= 1))
                continue;

            std::vector<std::string> tokens;
            Utils::tokenize_line(str, tokens);

            if(tokens.size() != 2)
                continue;

            if(tokens[0].find("last_server_used") != std::string::npos)
            {
                serverIP = tokens[1];
                std::cout << "[CLIENT] Server IP from config file: " << serverIP << std::endl;
            }
            else if(tokens[0].find("port") != std::string::npos)
            {
                port = stoi(tokens[1]);
                std::cout << "[CLIENT] Port from config file: " << tokens[1] << std::endl;
            }
            else if(tokens[0].find("default_debug_mode") != std::string::npos)
            {
                std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

                if(tokens[1].find("true") != std::string::npos)
                    debug_enabled = true;
                else debug_enabled = false;

                std::cout << "[CLIENT] Default debug mode from config file: " << debug_enabled << std::endl;
            }
            else if(tokens[0].find("default_log_file") != std::string::npos)
            {
                log_path = tokens[1];
                std::cout << "[CLIENT] Log path specified in config file: " << log_path << std::endl;
            }
            else if(tokens[0].compare("log") == 0)
            {
                std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

                if(tokens[1].find("true") != std::string::npos)
                    should_log = true;
                else should_log = false;

                std::cout << "[CLIENT] Will log this sesssion? (specified in config file): " << should_log << std::endl;
            }
            else if(tokens[0].find("nickname") != std::string::npos)
            {
                nickname = tokens[1];
                std::cout << "[CLIENT] Nickname specified in config file: " << nickname << std::endl;
            }
        }
        return 0;
    }

    std::string err_bad_arg(char selected)
    {
        std::string ret = "UNRECOGNIZED PROGRAM ARGUMENT: -" + selected;
        return ret;
    }

    void print_and_log(std::string output)
    {
        if(should_log)
        {
            std::fstream fs;
            fs.open (log_path, std::fstream::out | std::fstream::app);

            // append to log file
            fs << output << std::endl;
            fs.close();
        }

        // do the actual print to terminal
        std::cout << output << std::endl;
    }

    void log_only(std::string output)
    {
        if(should_log)
        {
            std::fstream fs;
            fs.open (log_path, std::fstream::out | std::fstream::app);

            // append to log file
            fs << output << std::endl;
            fs.close();
        }
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
            // check if there's any new messages from the server
            ssize_t v;
            std::string msg;
            // this includes a timeout so we don't get stuck here for too long if we need to send something
            tie(msg, v) = clientSocket->recvString(4096, false);

            // print response from server, as long as it isn't just an empty acknowledgement
            if(v > 0 && msg != "\n")
                print_and_log(msg);



            if(!message_queue.empty())
            {
                std::string message = next_message();
                clientSocket->sendString(message, false);
            }
        }
    }

    void process_client_commands()
    {
        static bool RUNNING = true;

        char input_cstr[256];
        std::string input;
        do
        {
            std::cin.getline(input_cstr, 256);
            input = std::string(input_cstr);
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);

            std::string outgoing_msg;
            bool should_send;
            tie(outgoing_msg, should_send) = IRC_Client::build_outgoing_message(input, RUNNING);

            // push a message onto the queue to be sent by the socket thread
            if(should_send)
            {
                std::lock_guard <std::mutex> guard(IRC_Client::message_queue_mutex);
                message_queue.push(outgoing_msg);
                log_only(outgoing_msg);
            }

#if DEBUG == true
            print_and_log(outgoing_msg );
#endif
        } while (RUNNING);
        communicator_running = false;
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

    void run_test_file(std::string test_filepath)
    {
        std::ifstream in(test_filepath);

        if(!in) {
            std::cout << "[CLIENT] Couldn't open test file" << std::endl;
            return;
        }

        std::cout << "[CLIENT] Running test file " << test_filepath << std::endl;

        bool RUNNING = true;
        std::string str;
        do
        {
            while (std::getline(in, str))
            {
                std::string outgoing_msg;
                bool should_send;
                tie(outgoing_msg, should_send) = IRC_Client::build_outgoing_message(str, RUNNING);

                // push a message onto the queue to be sent by the socket thread
                if (should_send)
                {
                    std::lock_guard <std::mutex> guard(IRC_Client::message_queue_mutex);
                    message_queue.push(outgoing_msg);
                    print_and_log(outgoing_msg);
                }

                // sleep for a while since we want to simulate an actual user running commands
                std::this_thread::sleep_for(std::chrono::seconds(4));
            }
        }while(RUNNING);
        communicator_running = false;
    }

}




int main(int argc, char *argv[])
{
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
    IRC_Client::load_config_file(config_filepath, serverIP, port, nickname, debug_enabled);

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
                IRC_Client::log_path = optarg;
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

    IRC_Client::print_and_log("[CLIENT] Starting the client..." );

    IRC_Client::print_and_log("[CLIENT] Connecting to server " + serverIP + " on port " + std::to_string(port) );

    IRC_Client::TCPClientSocket clientSocket(serverIP,port);

    int val = clientSocket.connectSocket();
    if(val == -1)
    {
        IRC_Client::print_and_log("[CLIENT] Couldn't connect to the server. Check hostname/IP and port" );
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
        IRC_Client::print_and_log(msg );
        clientSocket.closeSocket();
        exit(0);
    }

    IRC_Client::print_and_log(msg );

    // communicate with the server
    std::thread communicatorThread(IRC_Client::communicate_with_server, &clientSocket);

    // listen for terminal input input
    if(test_filepath.compare(IRC_Client::NO_TEST) == 0)
    {
        std::thread commandListener(IRC_Client::process_client_commands);
        commandListener.join();
    }
    // run the test file automatically
    else
    {
        std::thread testingThread(IRC_Client::run_test_file, test_filepath);
        testingThread.join();
    }

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    int guiRet = a.exec();

    // need to join() for server communications AFTER we launch either testingThread or commandListener
     communicatorThread.join();

     clientSocket.closeSocket();

     IRC_Client::print_and_log("[CLIENT] Terminated session!");

     return guiRet;
}
