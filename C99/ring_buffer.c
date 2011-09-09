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
    #include <pthread.h>
#else
    #define pthread_mutex_t void*
    #define pthread_mutex_init(mutex, attr) 0
    #define pthread_mutex_lock(mutex) 0
    #define pthread_mutex_unlock(mutex)
    #define pthread_mutex_destroy(mutex)
#endif


#define min(a, b) (((a) < (b)) ? (a) : (b))


struct _callback {
    ring_buffer_callback callback;
    size_t threshold;
};


struct _ring_buffer {
    unsigned char* buffer;
    size_t capacity, backlog;
    size_t read, write, rewind;
    pthread_mutex_t lock;
    struct _callback read_callback, write_callback;
};


ring_buffer_status ring_buffer_create(ring_buffer** ring, size_t capacity, size_t backlog) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        struct _ring_buffer* _ring;
        
        if (NULL != (_ring = (struct _ring_buffer*)malloc(sizeof(struct _ring_buffer)))) {
            if (NULL != (_ring->buffer = (unsigned char*)malloc(capacity))) {
                if (0 == pthread_mutex_init(&_ring->lock, NULL)) {
                    _ring->capacity = capacity;
                    _ring->read = _ring->write = _ring->rewind = 0;
                    _ring->backlog = backlog;
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
        if (0 == pthread_mutex_lock(&ring->lock)) {
            ring->read_callback.callback = callback;
            ring->read_callback.threshold = threshold;
            pthread_mutex_unlock(&ring->lock);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_set_write_callback(ring_buffer* ring, ring_buffer_callback callback, size_t threshold) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            ring->write_callback.callback = callback;
            ring->write_callback.threshold = threshold;
            pthread_mutex_unlock(&ring->lock);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_write(ring_buffer* ring, const void* data, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != data)) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            int notify = 0;

            if ((ring->capacity - ring->backlog + ring->rewind - (ring->write - ring->read)) >= length) {
                size_t left = length;

                do {
                    size_t target = ring->write % ring->capacity, size = min(left, ring->capacity - target);

                    memcpy((char*)ring->buffer + target, (const char*)data + length - left, size);
                    left -= size;
                    ring->write += size;
                } while (left > 0);
                
                if ((ring->write - ring->read) >= ring->read_callback.threshold)
                    notify = 1;
            }
            else
                result = RING_BUFFER_OVERFLOW;
         
            pthread_mutex_unlock(&ring->lock);

            if (ring->read_callback.callback && notify)
                ring->read_callback.callback(ring);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_read(ring_buffer* ring, void* data, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != data)) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            int notify = 0;

            if ((ring->write - ring->read) >= length) {
                size_t left = length;

                do {
                    size_t target = ring->read % ring->capacity, size = min(left, ring->capacity - target);

                    memcpy((char*)data + length - left, (const char*)ring->buffer + target, size);
                    left -= size;
                    ring->read += size;
                } while (left > 0);
                
                if (ring->rewind < length)
                    ring->rewind = 0;
                else
                    ring->rewind -= length;

                if ((ring->capacity - ring->backlog + ring->rewind - (ring->write - ring->read)) >= ring->write_callback.threshold)
                    notify = 1;
            }
            else
                result = RING_BUFFER_UNDERFLOW;

            pthread_mutex_unlock(&ring->lock);

            if (ring->write_callback.callback && notify)
                ring->write_callback.callback(ring);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_rewind(ring_buffer* ring, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            if ((length <= ring->backlog) && (length <= ring->read)) {
                ring->read -= length;
                ring->rewind += length;
            }
            else
                result = RING_BUFFER_UNDERFLOW;

            pthread_mutex_unlock(&ring->lock);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
    }
    else
        result = RING_BUFFER_INVALID_ADDRESS;
    
    return result;
}


ring_buffer_status ring_buffer_available(ring_buffer* ring, size_t* read, size_t* write, size_t* rewind) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != read) && (NULL != write)) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            *read = ring->write - ring->read;
            *write = ring->capacity - ring->backlog + ring->rewind - *read;
            *rewind = min(ring->read, ring->backlog - ring->rewind);
            
            pthread_mutex_unlock(&ring->lock);
        }
        else
            result = RING_BUFFER_CONCURRENCY_ERROR;
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
