//
// Created by ikegentz on 11/7/18.
//

#include "user.h"

IRC_Server::User::User(std::string nick_, std::string ip_, int port_, std::string hostname_) : nickname(nick_), ip_address(ip_), port(port_), hostname(hostname_)
{}

std::string IRC_Server::User::to_string()
{
    std::string ret = "";
    ret += nickname + "@" + ip_address + "[" + hostname + "]:" + std::to_string(port);
    return ret;
}