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


#include "ring_buffer.hpp"
#include <algorithm>
#include <cstring>

#ifdef RING_BUFFER_THREAD_SAFETY
    #include <pthread.h>
#endif


struct ring_buffer::ring_buffer_implementation {
    struct _callback {
        ring_buffer_callback callback;
        size_t threshold;
    };

#ifdef RING_BUFFER_THREAD_SAFETY
    struct locking {
        pthread_mutex_t* mutex;


        locking(ring_buffer_implementation* buffer) : mutex(&buffer->lock) { 
            if (0 != pthread_mutex_lock(mutex))
                throw ring_buffer_concurrency_error_exception();
        }


        ~locking() { 
            if (0 != pthread_mutex_unlock(mutex))
                throw ring_buffer_concurrency_error_exception();
        }


        static void prepare(ring_buffer_implementation* buffer) {
            pthread_mutexattr_t attributes;

            if ((0 != pthread_mutexattr_init(&attributes)) || (0 != pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE)) || (0 != pthread_mutex_init(&buffer->lock, &attributes)))
                throw ring_buffer_concurrency_error_exception();
        }


        static void finalize(ring_buffer_implementation* buffer) {
            if (0 != pthread_mutex_destroy(&buffer->lock))
                throw ring_buffer_concurrency_error_exception();
        }
    };


    pthread_mutex_t lock;
#else
    struct locking {
        locking(ring_buffer_implementation* buffer) { }
        static void prepare(ring_buffer_implementation* buffer) { }
        static void finalize(ring_buffer_implementation* buffer) { }
    };
#endif
    

    void* buffer;
    size_t capacity, _read, _write;
    _callback read_callback, write_callback;
    ring_buffer* parent;


    inline size_t ring_buffer_readable() { return _write - _read; }
    inline size_t ring_buffer_writable() { return capacity - ring_buffer_readable(); }


    ring_buffer_implementation(size_t capacity, ring_buffer* parent) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) : capacity(capacity), _read(0), _write(0), parent(parent) {
        locking::prepare(this);
        read_callback.callback = write_callback.callback = 0;

        if (0 == (buffer = malloc(capacity))) {
            locking::finalize(this);
            throw ring_buffer_out_of_memory_exception();
        }
    }


    ring_buffer_implementation(ring_buffer_implementation* other, ring_buffer* parent) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) : capacity(other->capacity), _read(other->_read), _write(other->_write), read_callback(other->read_callback), write_callback(other->write_callback), parent(parent) {
        locking lock(other);

        locking::prepare(this);

        if (0 != (buffer = malloc(capacity)))
            memcpy(buffer, other->buffer, capacity);
        else {
            locking::finalize(this);
            throw ring_buffer_out_of_memory_exception();
        }
    }


    void set_read_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) {
        locking lock(this);

        read_callback.callback = callback;
        read_callback.threshold = threshold;
    }


    void set_write_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) {
        locking lock(this);

        write_callback.callback = callback;
        write_callback.threshold = threshold;
    }


    void write(const void* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception) {
        if (0 != data) {
            locking lock(this);

            if (ring_buffer_writable() >= length) {
                size_t left = length;

                do {
                    size_t target = _write % capacity, size = std::min(left, capacity - target);

                    memcpy(reinterpret_cast<char*>(buffer) + target, reinterpret_cast<const char*>(data) + length - left, size);
                    left -= size;
                    _write += size;
                } while (left > 0);

                if (read_callback.callback and (ring_buffer_readable() >= read_callback.threshold))
                    read_callback.callback(parent);
            }
            else
                throw ring_buffer_overflow_exception();
        }
        else
            throw ring_buffer_invalid_address_exception();
    }


    void read(void* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception) {
        if (0 != data) {
            locking lock(this);

            if (ring_buffer_readable() >= length) {
                size_t left = length;

                do {
                    size_t target = _read % capacity, size = std::min(left, capacity - target);

                    memcpy(reinterpret_cast<char*>(data) + length - left, reinterpret_cast<const char*>(buffer) + target, size);
                    left -= size;
                    _read += size;
                } while (left > 0);

                if (write_callback.callback and (ring_buffer_writable() >= write_callback.threshold))
                    write_callback.callback(parent);
            }
            else
                throw ring_buffer_underflow_exception();
        }
        else
            throw ring_buffer_invalid_address_exception();
    }


    void get_available(size_t& read, size_t& write) throw (ring_buffer_concurrency_error_exception) {
        locking lock(this);

        read = ring_buffer_readable();
        write = ring_buffer_writable();
    }


    ~ring_buffer_implementation() throw (ring_buffer_concurrency_error_exception) {
        {
            locking lock(this);
            free(buffer);
        }

        locking::finalize(this);
    }
};


ring_buffer::ring_buffer(size_t capacity) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) : implementation(new ring_buffer_implementation(capacity, reinterpret_cast<ring_buffer*>(this))) { }
ring_buffer::ring_buffer(ring_buffer& other) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) : implementation(new ring_buffer_implementation(other.implementation, reinterpret_cast<ring_buffer*>(this))) { }
ring_buffer& ring_buffer::operator=(ring_buffer& other) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) { delete implementation; implementation = new ring_buffer_implementation(other.implementation, reinterpret_cast<ring_buffer*>(this)); return *this; }
void ring_buffer::set_read_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) { implementation->set_read_callback(callback, threshold); }
void ring_buffer::set_write_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) { implementation->set_write_callback(callback, threshold); }
void ring_buffer::write(const void* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception) { implementation->write(data, length); }
void ring_buffer::read(void* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception) { implementation->read(data, length); }
void ring_buffer::get_available(size_t& read, size_t& write) throw (ring_buffer_concurrency_error_exception) { implementation->get_available(read, write); }
ring_buffer::~ring_buffer() throw (ring_buffer_concurrency_error_exception) { delete implementation; }
