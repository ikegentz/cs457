//
// Created by ikegentz on 11/7/18.
//

#include "client_command_processor.h"
#include "client_globals.h"
#include "../utils/string_ops.h"



std::tuple<std::string, bool> IRC_Client::build_outgoing_message(std::string client_input, bool& running)
{
    std::string response = "";

    // process commands
    if(client_input[0] == '/')
    {
        std::string command = client_input.substr(1, client_input.find(" "));
        bool should_send = true;

        std::cout << "COMMAND " << command << std::endl;

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
        else if(command.find("privmsg") != std::string::npos)
        {
           response = privmsg_command(client_input, should_send);
        }
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
        else if(command.find("info") != std::string::npos)
        {
            response = info_command();
            should_send = true;
        }
        else if(command.find("time") != std::string::npos)
        {
            response = time_command();
            should_send = true;
        }
        else if(command.find("userip") != std::string::npos)
        {
            response = user_ip_command(client_input, should_send);
        }
        else if(command.find("userhost") != std::string::npos)
        {
            response = user_host_command(client_input, should_send);
        }
        else if(command.find("version") != std::string::npos)
        {
            should_send = true;
            response = version_command();
        }
        else if(command.find("users") != std::string::npos)
        {
            should_send = true;
            response = users_command();
        }
        else if(command.find("kill") != std::string::npos)
        {
            response = kill_command(client_input, should_send);
        }
        else if(command.find("kick") != std::string::npos)
        {
            response = kick_command(client_input, should_send);
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

std::string IRC_Client::privmsg_command(std::string input, bool& should_send)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);

    std::string ret = "PRIVMSG";

    if(tokens.size() < 3)
    {
        std::cout << "[CLIENT] USAGE: /PRIVMSG <target> message\n" <<
        "\tWhere <target> is either a username or a #channel" << std::endl;
        should_send = false;
        return "";
    }

    for(unsigned i = 1; i < tokens.size(); ++i)
        ret += " " + tokens[i];
    ret += "\n";

    return ret;
}

std::string IRC_Client::info_command()
{
    return "INFO";
}

std::string IRC_Client::time_command()
{
    return "TIME";
}

std::string IRC_Client::user_ip_command(std::string input, bool& should_send)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);

    if(tokens.size() != 2)
    {
        std::cout << "[CLIENT] Specify a user for this command" << std::endl;
        std::cout << "\t/USERIP <user>" << std::endl;
        should_send = false;
        return "";
    }

    should_send = true;
    return "USERIP " + tokens[1];
}

std::string IRC_Client::user_host_command(std::string input, bool& should_send)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);

    if(tokens.size() != 2)
    {
        std::cout << "[CLIENT] Specify a user for this command" << std::endl;
        std::cout << "\t/USERHOST <user>" << std::endl;
        should_send = false;
        return "";
    }

    should_send = true;
    return "USERHOST " + tokens[1];
}

std::string IRC_Client::version_command()
{
    return "VERSION";
}

std::string IRC_Client::users_command()
{
    return "USERS";
}

std::string IRC_Client::kill_command(std::string input, bool& should_send)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);

    if(tokens.size() != 2)
    {
        std::cout << "[CLIENT] Specify a user for this command" << std::endl;
        std::cout << "\t/KILL <user>" << std::endl;
        should_send = false;
        return "";
    }

    should_send = true;
    return "KILL " + tokens[1];
}

std::string IRC_Client::kick_command(std::string input, bool& should_send)
{
    std::vector<std::string> tokens;
    Utils::tokenize_line(input, tokens);

    if(tokens.size() != 2)
    {
        std::cout << "[CLIENT] Specify a user for this command" << std::endl;
        std::cout << "\t/KICK <user>" << std::endl;
        should_send = false;
        return "";
    }

    should_send = true;
    return "KICK " + tokens[1];
}

std::string IRC_Client::help_command()
{
    std::string ret = "\nSupported Commands: \n";
    ret += "\tPRIVMSG\n";
    ret += "\tHELP\n";
    ret += "\tJOIN\n";
    ret += "\tLIST\n";
    ret += "\tQUIT\n";
    ret += "\tINFO\n";
    ret += "\tTIME\n";
    ret += "\tUSERIP\n";
    ret += "\tUSERS\n";
    ret += "\tUSERHOST\n";
    ret += "\tVERSION\n";
    return ret;
}