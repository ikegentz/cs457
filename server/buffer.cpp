//
// Created by ikegentz on 11/1/18.
//

#include "buffer.h"

IRC_Server::Buffer::Buffer(int buf_size_) : buf_size(buf_size_)
{
    this->stringBuffer = new char[this->buf_size];
}

IRC_Server::Buffer::~Buffer()
{
    delete[] this->stringBuffer;
}

void IRC_Server::Buffer::zero_out()
{
    memset(this->stringBuffer, 0, sizeof(char) * this->size());    //change made here. Zeros out buffer.
}

const char* IRC_Server::Buffer::get_buffer() const
{
    return this->stringBuffer;
}

char* IRC_Server::Buffer::get_buffer()
{
    return this->stringBuffer;
}

int IRC_Server::Buffer::size() const
{
    return this->buf_size;
}