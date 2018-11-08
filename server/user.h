//
// Created by ikegentz on 11/7/18.
//

#ifndef CS457_USER_H
#define CS457_USER_H

#include <string>

namespace IRC_Server
{
    class User
    {
    public:
        User(std::string, std::string, int, std::string, std::string);
        User();
        std::string nickname;
        std::string ip_address;
        int port;
        std::string hostname;
        std::string current_channel;

        std::string to_string();
    };
}

#endif //CS457_USER_H
