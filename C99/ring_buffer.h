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


ring_buffer_status ring_buffer_create(ring_buffer** ring, size_t capacity, size_t backlog);
ring_buffer_status ring_buffer_write(ring_buffer* ring, const void* data, const size_t length);
ring_buffer_status ring_buffer_read(ring_buffer* ring, void* data, const size_t length);
ring_buffer_status ring_buffer_rewind(ring_buffer* ring, const size_t length);
ring_buffer_status ring_buffer_available(ring_buffer* ring, size_t* read, size_t* write, size_t* rewind);
ring_buffer_status ring_buffer_destroy(ring_buffer* ring);


#endif
