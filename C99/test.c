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


#include <assert.h>
#include <stdlib.h>
#include "ring_buffer.h"


static void simple() {
    ring_buffer* buffer;
    unsigned char foo1 = 0xDE;
    unsigned short foo2 = 0xDEAD;
    unsigned int foo4 = 0xDEADFACE;
    size_t read, write;

    assert(RING_BUFFER_SUCCESS == ring_buffer_create(&buffer, 6));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 0) && (write == 6));

    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo1, 1));
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo2, 2));
    assert(RING_BUFFER_OVERFLOW == ring_buffer_write(buffer, &foo4, 4));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 3) && (write == 3));

    assert((RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo1, 1)) && (foo1 == 0xDE));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 2) && (write == 4));

    assert((RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo4, 4)));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 6) && (write == 0));

    assert((RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo2, 2)) && (foo2 == 0xDEAD));
    assert((RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo4, 4)) && (foo4 == 0xDEADFACE));
    assert((RING_BUFFER_UNDERFLOW == ring_buffer_read(buffer, &foo4, 4)) && (foo4 == 0xDEADFACE));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 0) && (write == 6));
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo2, 2));
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo4, 4));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 6) && (write == 0));
    
    assert((RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo2, 2)) && (foo2 == 0xDEAD));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 4) && (write == 2));
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo1, 1));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 5) && (write == 1));

    assert((RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo2, 2)) && (foo2 == 0xFACE));
    assert((RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &read, &write)) && (read == 3) && (write == 3));

    assert(RING_BUFFER_SUCCESS == ring_buffer_destroy(buffer));
}


size_t callback_read = 0, callback_write = 0;


static void callback(ring_buffer* ring) {
    assert(RING_BUFFER_SUCCESS == ring_buffer_get_available(ring, &callback_read, &callback_write));
}


static void async() {
    ring_buffer* buffer;
    unsigned int foo = 0xDEADFACE;

    assert(RING_BUFFER_SUCCESS == ring_buffer_create(&buffer, 8));

    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo, 1));
    assert((callback_read == 0) && (callback_write == 0));
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_set_read_callback(buffer, callback, 4));
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo, 1));
    assert((callback_read == 0) && (callback_write == 0));
    assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, &foo, 4));
    assert((callback_read == 6) && (callback_write == 2));
    assert(RING_BUFFER_SUCCESS == ring_buffer_set_read_callback(buffer, NULL, 0));
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_set_write_callback(buffer, callback, 4));
    assert(RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo, 1));
    assert((callback_read == 6) && (callback_write == 2));
    assert(RING_BUFFER_SUCCESS == ring_buffer_read(buffer, &foo, 4));
    assert((callback_read == 1) && (callback_write == 7));
    assert(RING_BUFFER_SUCCESS == ring_buffer_set_write_callback(buffer, NULL, 0));
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_destroy(buffer));
}


static unsigned char write_counter = 0;
static unsigned char read_counter = 0;


static void sync() {
    write_counter = read_counter = 0;
}


static void revert(size_t length) {
    write_counter -= length;
}


static void produce(void* buffer, size_t length) {
    for (int i = 0; i < length; i++)
        ((unsigned char*)buffer)[i] = write_counter++;    
}


static void verify(void* buffer, size_t length) {
    for (int i = 0; i < length; i++)
        assert(((unsigned char*)buffer)[i] == read_counter++);
}


static void sequential(const size_t byte_count, const size_t ring_buffer_size, const size_t max_block_size) {
    ring_buffer* buffer;
    void* temp_buffer = malloc(max_block_size);
    size_t count = 0;

    assert(RING_BUFFER_SUCCESS == ring_buffer_create(&buffer, ring_buffer_size));
    sync();

    while (count < byte_count) {
        ring_buffer_status status;
        size_t length;

        // Fill the buffer until an overflow is detected
        do {
            length = rand() % max_block_size;;
            produce(temp_buffer, length);
            status = ring_buffer_write(buffer, temp_buffer, length);
        } while (status == RING_BUFFER_SUCCESS);

        // Discard last (not inserted) buffer
        revert(length);

        // Read all written data breaking randomly
        do {
            length = rand() % max_block_size;
            status = ring_buffer_read(buffer, temp_buffer, length);

            if (status == RING_BUFFER_UNDERFLOW) {
                size_t dummy;

                assert(RING_BUFFER_SUCCESS == ring_buffer_get_available(buffer, &length, &dummy));
                assert(RING_BUFFER_SUCCESS == ring_buffer_read(buffer, temp_buffer, length));
            }
            
            verify(temp_buffer, length);
            count += length;
        } while (status == RING_BUFFER_SUCCESS);
    }

    assert(RING_BUFFER_SUCCESS == ring_buffer_destroy(buffer));
    free(temp_buffer);
}


static void interleaved(const size_t byte_count, const size_t ring_buffer_size, const size_t max_block_size) {
    ring_buffer* buffer;
    void* temp_buffer = malloc(max_block_size);
    size_t count = 0;

    assert(RING_BUFFER_SUCCESS == ring_buffer_create(&buffer, ring_buffer_size));
    sync();

    while (count < byte_count) {
        ring_buffer_status status;
        size_t length;

        length = rand() % max_block_size;
        produce(temp_buffer, length);
        status = ring_buffer_write(buffer, temp_buffer, length);
        
        if (status == RING_BUFFER_OVERFLOW)
            revert(length);

        length = rand() % max_block_size;
        status = ring_buffer_read(buffer, temp_buffer, length);

        if (status != RING_BUFFER_UNDERFLOW) {
            verify(temp_buffer, length);
            count += length;
        }
    }

    assert(RING_BUFFER_SUCCESS == ring_buffer_destroy(buffer));
    free(temp_buffer);
}


static void huge() {
    const size_t buffer_size = 1024*1024;
    ring_buffer* buffer;
    const size_t temp_buffer_size = 1024*1024;
    void* temp_buffer = malloc(temp_buffer_size);

    assert(RING_BUFFER_SUCCESS == ring_buffer_create(&buffer, buffer_size));

    for (int i = 0; i <= 4096; i++) {
        assert(RING_BUFFER_SUCCESS == ring_buffer_write(buffer, temp_buffer, temp_buffer_size));
        assert(RING_BUFFER_SUCCESS == ring_buffer_read(buffer, temp_buffer, temp_buffer_size));
    }
    
    assert(RING_BUFFER_SUCCESS == ring_buffer_destroy(buffer));
    free(temp_buffer);
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
