//
// Created by ikegentz on 11/7/18.
//

#ifndef CS457_STRING_OPS_H
#define CS457_STRING_OPS_H

#include <string>
#include <sstream>
#include <iostream>
#include <vector>

namespace Utils
{
    static void tokenize_line(std::string line, std::vector<std::string> &tokens)
    {
        std::string buffer;
        std::stringstream ss(line);

        // tokenize the line to grab model data
        while(ss >> buffer)
            tokens.push_back(buffer);
    }
}

#endif //CS457_STRING_OPS_H
