//
// Created by ikegentz on 11/7/18.
//

#include "client_command_processor.h"
#include "client_globals.h"


std::tuple<std::string, bool> IRC_Client::build_outgoing_message(std::string client_input, bool& running)
{
    std::string response = "";

    // process commands
    if(client_input[0] == '/')
    {
        std::string command = client_input.substr(1, client_input.find(" "));
        bool should_send = true;

        if(command.find("list") != std::string::npos)
        {
            response = list_command();
        }
        else if(command.find("join") != std::string::npos)
        {
            if(client_input.size() < 6)
            {
                std::cout << "[CLIENT] Provide an channel for the JOIN command" << std::endl;
                should_send = false;
                response = "";
            }
            else
                response = join_command(client_input);
        }
        else if(command.find("quit") != std::string::npos)
        {
            should_send = true;
            response = quit_command();
            running = false;
        }
      //  else if(command.find("cprivmsg") != std::string::npos)
     //   {
     //      response = cprivmsg_command();
    //    }
        else if(command.find("help") != std::string::npos)
        {
            std::cout << help_command() << std::endl;
            should_send = false;
        }
        else if(command.find("ping") != std::string::npos)
        {
            response = ping_command();
            should_send = true;
        }
        else
        {
            std::cout << "[CLIENT] Unrecognized command '" + command + "'" << std::endl;
            should_send = false;
        }

        return std::make_tuple(response, should_send);
    }

    //process general message
    response = client_input;
    return std::make_tuple(response, true);
}
std::string IRC_Client::ping_command()
{
    return "PING";
}

std::string IRC_Client::list_command()
{
    return "LIST";
}

std::string IRC_Client::join_command(std::string input)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);
    return "JOIN " + tokens[1];
}

std::string IRC_Client::quit_command()
{
    return "QUIT";
}

std::string IRC_Client::cprivmsg_command()
{
    return "CPRIVMSG";
}

std::string IRC_Client::help_command()
{
    std::string ret = "\nSupported Commands: \n";
    //ret += "\tCPRIVMSG\n";
    ret += "\tHELP\n";
    ret += "\tJOIN\n";
    ret += "\tLIST\n";
    ret += "\tQUIT\n";
    return ret;
}