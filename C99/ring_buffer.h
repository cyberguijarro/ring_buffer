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
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stddef.h>


typedef struct _ring_buffer ring_buffer;

typedef enum {
    RING_BUFFER_SUCCESS,
    RING_BUFFER_INVALID_ADDRESS,
    RING_BUFFER_OUT_OF_MEMORY,
    RING_BUFFER_OVERFLOW,
    RING_BUFFER_UNDERFLOW,
    RING_BUFFER_CONCURRENCY_ERROR
} ring_buffer_status;

typedef void (*ring_buffer_callback)(ring_buffer* ring);


ring_buffer_status ring_buffer_create(ring_buffer** ring, size_t capacity, size_t backlog);
ring_buffer_status ring_buffer_set_read_callback(ring_buffer* ring, ring_buffer_callback callback, size_t threshold);
ring_buffer_status ring_buffer_set_write_callback(ring_buffer* ring, ring_buffer_callback callback, size_t threshold);
ring_buffer_status ring_buffer_write(ring_buffer* ring, const void* data, const size_t length);
ring_buffer_status ring_buffer_read(ring_buffer* ring, void* data, const size_t length);
ring_buffer_status ring_buffer_rewind(ring_buffer* ring, const size_t length);
ring_buffer_status ring_buffer_available(ring_buffer* ring, size_t* read, size_t* write, size_t* rewind);
ring_buffer_status ring_buffer_destroy(ring_buffer* ring);


#endif
