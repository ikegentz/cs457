//
// Created by ikegentz on 11/10/18.
//

#ifndef CS457_SERVER_INFO_H
#define CS457_SERVER_INFO_H

struct ServerInfo
{
public:
    std::string version;
    std::string patch_level;
    std::string start_time;

    std::string to_string()
    {
        return "Version: " + version + "@" + patch_level + " -- Started at " + start_time;
    }
};

#endif //CS457_SERVER_INFO_H
