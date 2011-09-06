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


struct _ring_buffer {
    unsigned char* buffer;
    size_t capacity;
    size_t read, write;
    pthread_mutex_t lock;
};


ring_buffer_status ring_buffer_create(ring_buffer** ring, size_t capacity) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if (NULL != ring) {
        struct _ring_buffer* _ring;
        
        if (NULL != (_ring = (struct _ring_buffer*)malloc(sizeof(struct _ring_buffer)))) {
            if (NULL != (_ring->buffer = (unsigned char*)malloc(capacity))) {
                if (0 == pthread_mutex_init(&_ring->lock, NULL)) {
                    _ring->capacity = capacity;
                    _ring->read = _ring->write = 0;
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


ring_buffer_status ring_buffer_write(ring_buffer* ring, const void* data, const size_t length) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != data)) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            if ((ring->capacity - (ring->write - ring->read)) >= length) {
                size_t left = length;

                while (left > 0) {
                    size_t target = ring->write % ring->capacity, size = min(left, ring->capacity - target);

                    memcpy((char*)ring->buffer + target, (const char*)data + length - left, size);
                    left -= size;
                    ring->write += size;
                }
            }
            else
                result = RING_BUFFER_OVERFLOW;
         
            pthread_mutex_unlock(&ring->lock);
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
            if ((ring->write - ring->read) >= length) {
                size_t left = length;

                while (left > 0) {
                    size_t target = ring->read % ring->capacity, size = min(left, ring->capacity - target);

                    memcpy((char*)data + length - left, (const char*)ring->buffer + target, size);
                    left -= size;
                    ring->read += size;
                }
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


ring_buffer_status ring_buffer_available(ring_buffer* ring, size_t* read, size_t* write) {
    ring_buffer_status result = RING_BUFFER_SUCCESS;

    if ((NULL != ring) && (NULL != read) && (NULL != write)) {
        if (0 == pthread_mutex_lock(&ring->lock)) {
            *read = ring->write - ring->read;
            *write = ring->capacity - *read;
        
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
