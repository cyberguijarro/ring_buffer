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


#include <cassert>
#include <cstdlib>

#include "ring_buffer.hpp"


static void simple() {
    try {
        ring_buffer buffer{6};
        unsigned char foo1 = 0xDE;
        unsigned short foo2 = 0xDEAD;
        unsigned int foo4 = 0xDEADFACE;
        size_t read, write;

        buffer.get_available(read, write);
        assert((read == 0) && (write == 6));

        buffer.write(&foo1, 1);
        buffer.write(&foo2, 2);
        try { buffer.write(&foo4, 4); assert(false); } catch (ring_buffer_overflow_exception) { }
        buffer.get_available(read, write);
        assert((read == 3) && (write == 3));

        buffer.read(&foo1, 1);
        assert(foo1 == 0xDE);
        buffer.get_available(read, write);
        assert((read == 2) && (write == 4));
        
        buffer.write(&foo4, 4);
        buffer.get_available(read, write);
        assert((read == 6) && (write == 0));
        
        buffer.read(&foo2, 2);
        buffer.read(&foo4, 4);
        try { buffer.read(&foo4, 4); assert(false); } catch (ring_buffer_underflow_exception) { }
        buffer.get_available(read, write);
        assert((read == 0) && (write == 6));
        
        buffer.write(&foo2, 2);
        buffer.write(&foo4, 4);
        buffer.get_available(read, write);
        assert((read == 6) && (write == 0));
        
        ring_buffer other(buffer);
        
        other.read(&foo2, 2);
        other.get_available(read, write);
        assert((read == 4) && (write == 2));

        other.write(&foo1, 1);
        other.get_available(read, write);
        assert((read == 5) && (write == 1));
        
        other.read(&foo2, 2);
        other.get_available(read, write);
        assert((read == 3) && (write == 3));
    } catch (ring_buffer_exception) {
        assert(false);
    }
}


static void async() {
    try {
        ring_buffer buffer{8};
        size_t callback_read = 0, callback_write = 0;
        unsigned int foo = 0xDEADFACE;

        buffer.write(&foo, 1);
        assert((callback_read == 0) && (callback_write == 0));

        buffer.set_read_callback([&]() { buffer.get_available(callback_read, callback_write); } , 4);
        buffer.write(&foo, 1);
        assert((callback_read == 0) && (callback_write == 0));
        buffer.write(&foo, 4);
        assert((callback_read == 6) && (callback_write == 2));
        buffer.set_read_callback(NULL, 0);

        buffer.set_write_callback([&]() { buffer.get_available(callback_read, callback_write); }, 4);
        buffer.read(&foo, 1);
        assert((callback_read == 6) && (callback_write == 2));
        buffer.read(&foo, 4);
        assert((callback_read == 1) && (callback_write == 7));
        buffer.set_write_callback(NULL, 0);
    } catch (ring_buffer_exception) {
        assert(false);
    }
}


static unsigned char write_counter = 0;
static unsigned char read_counter = 0;


static void sync(unsigned char value) {
    write_counter = read_counter = value;
}


static void revert(size_t length) {
    write_counter -= length;
}


static void produce(void* buffer, size_t length) {
    for (size_t i = 0; i < length; i++)
        ((unsigned char*)buffer)[i] = write_counter++;    
}


static void verify(void* buffer, size_t length) {
    for (size_t i = 0; i < length; i++)
        assert(((unsigned char*)buffer)[i] == read_counter++);
}


static void sequential(const size_t byte_count, const size_t ring_buffer_size, const size_t max_block_size) {
    try {
        ring_buffer buffer{ring_buffer_size};
        void* temp_buffer = malloc(max_block_size);
        size_t count = 0;

        sync(0);

        while (count < byte_count) {
            size_t length;
            bool proceed = true;

            // Fill the buffer until an overflow is detected
            while (proceed) {
                length = rand() % max_block_size;
                produce(temp_buffer, length);

                try { buffer.write(temp_buffer, length); } catch (ring_buffer_exception) { proceed = 0; }
            };

            // Discard last (not inserted) buffer
            revert(length);
            proceed = true;

            // Read all written data breaking randomly
            while (proceed) {
                length = rand() % max_block_size;

                try {
                    buffer.read(temp_buffer, length);
                } catch (ring_buffer_underflow_exception) {
                    size_t dummy;

                    buffer.get_available(length, dummy);
                    buffer.read(temp_buffer, length);
                    proceed = false;
                }
                catch (ring_buffer_exception) {
                    proceed = false;
                }

                verify(temp_buffer, length);
                count += length;
            }
        }

        free(temp_buffer);
    } catch (ring_buffer_exception) {
        assert(false);
    }
}


static void interleaved(const size_t byte_count, const size_t ring_buffer_size, const size_t max_block_size) {
    try {
        ring_buffer buffer{ring_buffer_size};
        void* temp_buffer = malloc(max_block_size);
        size_t count = 0;

        sync(0);

        while (count < byte_count) {
            size_t length = rand() % max_block_size;

            produce(temp_buffer, length);

            try {
                buffer.write(temp_buffer, length);
            } catch (ring_buffer_overflow_exception) {
                revert(length);
            }

            length = rand() % max_block_size;

            try {
                buffer.read(temp_buffer, length);
            } catch (ring_buffer_underflow_exception) {
                continue;
            }

            verify(temp_buffer, length);
            count += length;
        }

        free(temp_buffer);
    } catch (ring_buffer_exception) {
        assert(false);
    }
}


static void huge() {
    try {
        const size_t buffer_size = 1024*1024;
        ring_buffer buffer{buffer_size};
        const size_t temp_buffer_size = 1024*1024;
        void* temp_buffer = malloc(temp_buffer_size);

        for (int i = 0; i <= 4096; i++) {
            buffer.write(temp_buffer, temp_buffer_size);
            buffer.read(temp_buffer, temp_buffer_size);
        }

        free(temp_buffer);
    } catch (ring_buffer_exception) {
        assert(false);
    }
}


int main() {
    simple();

    async();

    sequential(1024*1024*16, 1024, 16);
    sequential(1024*1024*16, 1024, 512);
    sequential(1024*1024*16, 1024, 1024);

    interleaved(1024*1024*16, 1024, 16);
    interleaved(1024*1024*16, 1024, 512);
    interleaved(1024*1024*16, 1024, 1024);
    
    huge();

    return 0;   
}
