/*
    Copyright 2011 Emilio Guijarro

    This file is part of the Ring Buffer library.

    The Ring Buffer library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Ring Buffer library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Ring Buffer library.  If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once


#include <functional>
#include <memory>
#include <system_error>


struct ring_buffer_exception { };
struct ring_buffer_invalid_address_exception : ring_buffer_exception { };
struct ring_buffer_out_of_memory_exception : ring_buffer_exception { };
struct ring_buffer_overflow_exception : ring_buffer_exception { };
struct ring_buffer_underflow_exception : ring_buffer_exception { };

class ring_buffer {
private:
    class ring_buffer_implementation; std::unique_ptr<ring_buffer_implementation> implementation;


public:
    typedef std::function<void ()> ring_buffer_callback;


    ring_buffer(size_t capacity) throw (std::system_error, ring_buffer_out_of_memory_exception);
    ring_buffer(ring_buffer& other) throw (std::system_error, ring_buffer_out_of_memory_exception);
    ring_buffer& operator=(ring_buffer& other) throw (std::system_error, ring_buffer_out_of_memory_exception);
    void set_read_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error);
    void set_write_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error);
    void write(const void* data, size_t length) throw (std::system_error, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception);
    void read(void* data, size_t length) throw (std::system_error, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception);
    void get_available(size_t& read, size_t& write) throw (std::system_error);
    ~ring_buffer() throw (std::system_error);
};
