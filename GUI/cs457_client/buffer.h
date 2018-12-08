//
// Created by ikegentz on 11/1/18.
//

#ifndef CS457_BUFFER_H
#define CS457_BUFFER_H

#include <memory>
#include <cstring>

namespace Utils
{
    class Buffer
    {
    public:
        Buffer(int buf_size_);
        const char* get_buffer() const;
        char* get_buffer();
        int size() const;
        void zero_out();
        ~Buffer();

    private:
        int buf_size;
        char* stringBuffer;
    };
}

#endif //CS457_BUFFER_H
