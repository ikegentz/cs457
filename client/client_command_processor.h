//
// Created by ikegentz on 11/7/18.
//

#ifndef CS457_CLIENT_COMMAND_PROCESSOR_H
#define CS457_CLIENT_COMMAND_PROCESSOR_H

#include <string>
#include <iostream>



namespace IRC_Client
{
    std::string build_outgoing_message(std::string client_input, bool& running);

    std::string list_command();
    std::string join_command();
    std::string quit_command();
    std::string cprivmsg_command();
}

#endif //CS457_CLIENT_COMMAND_PROCESSOR_H
