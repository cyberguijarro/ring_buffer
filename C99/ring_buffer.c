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


#include "ring_buffer.h"

#include <stdlib.h>
#include <string.h>

#ifdef RING_BUFFER_THREAD_SAFETY
    #define __USE_UNIX98
    #include <pthread.h>

    #define ENTER_CRITICAL(ring) if (0 == pthread_mutex_lock(&ring->lock)) {
    #define EXIT_CRITICAL(ring, result) pthread_mutex_unlock(&ring->lock); } else result = RING_BUFFER_CONCURRENCY_ERROR
#else
    #define pthread_mutex_init(mutex, attr) 0
    #define pthread_mutex_lock(mutex) 0
    #define pthread_mutex_unlock(mutex)
    #define pthread_mutex_destroy(mutex)
    #define pthread_mutexattr_init(attr) 0
    #define pthread_mutexattr_settype(attr, type) 0
    
    #define ENTER_CRITICAL(ring)
    #define EXIT_CRITICAL(ring, result)
#endif


#define min(a, b) (((a) < (b)) ? (a) : (b))
            

struct _callback {
    ring_buffer_callback callback;
    size_t threshold;
};


struct _ring_buffer {
    unsigned char* buffer;
    size_t capacity, read, write;
#ifdef RING_BUFFER_THREAD_SAFETY
    pthread_mutex_t lock;
#endif
    struct _callback read_callback, write_callback;
};


ring_buffer_status ring_buffer_create(ring_buffer** ring, size_t capacity) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        struct _ring_buffer* _ring;
        
        if (NULL != (_ring = (struct _ring_buffer*)malloc(sizeof(struct _ring_buffer)))) {
            if (NULL != (_ring->buffer = (unsigned char*)malloc(capacity))) {
#ifdef RING_BUFFER_THREAD_SAFETY
                pthread_mutexattr_t attributes;
#endif

                if ((0 == pthread_mutexattr_init(&attributes)) && (0 == pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE)) && (0 == pthread_mutex_init(&_ring->lock, &attributes))) {
                    _ring->capacity = capacity;
                    _ring->read = _ring->write = 0;
                    _ring->read_callback.callback = _ring->write_callback.callback = NULL;
                    *ring = _ring;
                }
                else {
                    free(_ring->buffer);
                    free(_ring);
                    result = RING_BUFFER_CONCURRENCY_ERROR;
                }
            }
            else {
                free(_ring);
                result = RING_BUFFER_OUT_OF_MEMORY;
            }
        }
        else
            result = RING_BUFFER_OUT_OF_MEMORY;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_set_read_callback(ring_buffer* ring, ring_buffer_callback callback, size_t threshold)
{
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        ENTER_CRITICAL(ring);
        
        ring->read_callback.callback = callback;
        ring->read_callback.threshold = threshold;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_set_write_callback(ring_buffer* ring, ring_buffer_callback callback, size_t threshold) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        ENTER_CRITICAL(ring);
        
        ring->write_callback.callback = callback;
        ring->write_callback.threshold = threshold;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_write(ring_buffer* ring, const void* data, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != data)) {
        ENTER_CRITICAL(ring);

        if ((ring->capacity - (ring->write - ring->read)) >= length) {
            size_t left = length;

            do {
                size_t target = ring->write % ring->capacity, size = min(left, ring->capacity - target);

                memcpy((char*)ring->buffer + target, (const char*)data + length - left, size);
                left -= size;
                ring->write += size;
            } while (left > 0);

            if (ring->read_callback.callback && ((ring->write - ring->read) >= ring->read_callback.threshold))
                ring->read_callback.callback(ring);
        }
        else
            result = RING_BUFFER_OVERFLOW;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_read(ring_buffer* ring, void* data, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != data)) {
        ENTER_CRITICAL(ring);

        if ((ring->write - ring->read) >= length) {
            size_t left = length;

            do {
                size_t target = ring->read % ring->capacity, size = min(left, ring->capacity - target);

                memcpy((char*)data + length - left, (const char*)ring->buffer + target, size);
                left -= size;
                ring->read += size;
            } while (left > 0);

            if (ring->write_callback.callback && ((ring->capacity - (ring->write - ring->read)) >= ring->write_callback.threshold))
                ring->write_callback.callback(ring);
        }
        else
            result = RING_BUFFER_UNDERFLOW;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_get_available(ring_buffer* ring, size_t* read, size_t* write) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != read) && (NULL != write)) {
        ENTER_CRITICAL(ring);
        
        *read = ring->write - ring->read;
        *write = ring->capacity - *read;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_get_positions(ring_buffer* ring, size_t* read, size_t* write) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;
    
    if ((NULL != ring) && (NULL != read) && (NULL != write)) {
        ENTER_CRITICAL(ring);
        
        *read = ring->read;
        *write = ring->write;
        
        EXIT_CRITICAL(ring, result);
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_destroy(ring_buffer* ring) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;
    
    if (NULL != ring) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            free(ring->buffer);
            free(ring);
            pthread_mutex_unlock(&ring->lock);
            pthread_mutex_destroy(&ring->lock); 
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;

    return result;
}
