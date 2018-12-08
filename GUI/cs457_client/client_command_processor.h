//
// Created by ikegentz on 11/7/18.
//

#ifndef CS457_CLIENT_COMMAND_PROCESSOR_H
#define CS457_CLIENT_COMMAND_PROCESSOR_H

#include <string>
#include <iostream>
#include <tuple>


namespace IRC_Client
{
    std::tuple<std::string, bool> build_outgoing_message(std::string client_input, bool &running);

    std::string list_command();

    std::string join_command(std::string);

    std::string quit_command();

    std::string privmsg_command(std::string, bool &);

    std::string help_command();

    std::string ping_command();

    std::string info_command();

    std::string time_command();

    std::string user_ip_command(std::string input, bool &should_send);

    std::string version_command();

    std::string users_command();

    std::string kill_command(std::string input, bool&);

    std::string kick_command(std::string, bool&);

    std::string user_host_command(std::string input, bool &should_send);
}

#endif //CS457_CLIENT_COMMAND_PROCESSOR_H
