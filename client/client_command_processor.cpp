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
            response = join_command();
        }
        else if(command.find("quit") != std::string::npos)
        {
            response = quit_command();
            running = false;
        }
        else if(command.find("cprivmsg") != std::string::npos)
        {
            response = cprivmsg_command();
        }
        else if(command.find("help") != std::string::npos)
        {
            response = help_command();
            should_send = false;
        }
        else
        {
            response = "[CLIENT] Unrecognized command '" + command + "'";
            should_send = false;
        }

        return std::make_tuple(response, should_send);
    }

    //process general message
    response = client_input;
    return std::make_tuple(response, true);
}

std::string IRC_Client::list_command()
{
    return "LIST";
}

std::string IRC_Client::join_command()
{
    return "JOIN";
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
    ret += "\tCPRIVMSG\n";
    ret += "\tHELP\n";
    ret += "\tJOIN\n";
    ret += "\tLIST\n";
    ret += "\tQUIT\n";
    return ret;
}