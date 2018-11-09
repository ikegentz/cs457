//
// Created by ikegentz on 11/1/18.
//

#include "buffer.h"

Utils::Buffer::Buffer(int buf_size_) : buf_size(buf_size_)
{
    this->stringBuffer = new char[this->buf_size];
}

Utils::Buffer::~Buffer()
{
    delete[] this->stringBuffer;
}

void Utils::Buffer::zero_out()
{
    memset(this->stringBuffer, 0, sizeof(char) * this->size());    //change made here. Zeros out buffer.
}

const char* Utils::Buffer::get_buffer() const
{
    return this->stringBuffer;
}

char* Utils::Buffer::get_buffer()
{
    return this->stringBuffer;
}

int Utils::Buffer::size() const
{
    return this->buf_size;
}