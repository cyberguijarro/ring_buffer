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


#include <algorithm>
#include <cstring>

#ifdef RING_BUFFER_THREAD_SAFETY
    #include <pthread.h>

    #define ENTER_CRITICAL() if (0 == pthread_mutex_lock(&lock)) {
    #define EXIT_CRITICAL() pthread_mutex_unlock(&lock); } else throw ring_buffer_concurrency_error_exception()
#else
    #define pthread_mutex_init(mutex, attr) 0
    #define pthread_mutex_lock(mutex) 0
    #define pthread_mutex_unlock(mutex)
    #define pthread_mutex_destroy(mutex)
    #define pthread_mutexattr_init(attr) 0
    #define pthread_mutexattr_settype(attr, type) 0
    
    #define ENTER_CRITICAL()
    #define EXIT_CRITICAL()
#endif


struct ring_buffer_exception { };
struct ring_buffer_invalid_address_exception : ring_buffer_exception { };
struct ring_buffer_out_of_memory_exception : ring_buffer_exception { };
struct ring_buffer_overflow_exception : ring_buffer_exception { };
struct ring_buffer_underflow_exception : ring_buffer_exception { };
struct ring_buffer_concurrency_error_exception : ring_buffer_exception { };


template<typename T = void>
class generic_ring_buffer {
public:
    typedef void (*ring_buffer_callback)(generic_ring_buffer<T>* ring);


    generic_ring_buffer(size_t capacity) throw (ring_buffer_concurrency_error_exception, ring_buffer_out_of_memory_exception) : capacity(capacity), _read(0), _write(0) {
        if (NULL != (buffer = reinterpret_cast<T*>(malloc(capacity)))) {
#ifdef RING_BUFFER_THREAD_SAFETY
            pthread_mutexattr_t attributes;
#endif

            if ((0 == pthread_mutexattr_init(&attributes)) && (0 == pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE)) && (0 == pthread_mutex_init(&lock, &attributes)))
                read_callback.callback = write_callback.callback = 0;
            else {
                free(buffer);
                throw ring_buffer_concurrency_error_exception(); 
            }
        }
        else
            throw ring_buffer_out_of_memory_exception();
    }


    void set_read_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) {
        ENTER_CRITICAL();

        read_callback.callback = callback;
        read_callback.threshold = threshold;
        
        EXIT_CRITICAL();
    }


    void set_write_callback(ring_buffer_callback callback, size_t threshold) throw (ring_buffer_concurrency_error_exception) {
        ENTER_CRITICAL();

        write_callback.callback = callback;
        write_callback.threshold = threshold;
        
        EXIT_CRITICAL();
    }


    void write(const T* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception) {
        if (NULL != data) {
            ENTER_CRITICAL();

            if (ring_buffer_writable() >= length) {
                size_t left = length;

                do {
                    size_t target = _write % capacity, size = std::min(left, capacity - target);

                    memcpy(reinterpret_cast<char*>(buffer) + target, reinterpret_cast<const char*>(data) + length - left, size);
                    left -= size;
                    _write += size;
                } while (left > 0);

                if (read_callback.callback && (ring_buffer_readable() >= read_callback.threshold))
                    read_callback.callback(this);
            }
            else
                throw ring_buffer_overflow_exception();

            EXIT_CRITICAL();
        }
        else
            throw ring_buffer_invalid_address_exception();
    }


    void read(T* data, size_t length) throw (ring_buffer_concurrency_error_exception, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception) {
        if (NULL != data) {
            ENTER_CRITICAL();

            if (ring_buffer_readable() >= length) {
                size_t left = length;

                do {
                    size_t target = _read % capacity, size = std::min(left, capacity - target);

                    memcpy(reinterpret_cast<char*>(data) + length - left, reinterpret_cast<const char*>(buffer) + target, size);
                    left -= size;
                    _read += size;
                } while (left > 0);

                if (write_callback.callback && (ring_buffer_writable() >= write_callback.threshold))
                    write_callback.callback(this);
            }
            else
                throw ring_buffer_underflow_exception();

            EXIT_CRITICAL();
        }
        else
            throw ring_buffer_invalid_address_exception();
    }


    void get_available(size_t& read, size_t& write) throw (ring_buffer_concurrency_error_exception) {
        ENTER_CRITICAL();

        read = ring_buffer_readable();
        write = ring_buffer_writable();

        EXIT_CRITICAL();
    }


    ~generic_ring_buffer() throw (ring_buffer_concurrency_error_exception) {
        if (0 == pthread_mutex_lock(&lock)) {
            free(buffer);
            pthread_mutex_unlock(&lock);
            pthread_mutex_destroy(&lock); 
        }
        else
            throw ring_buffer_concurrency_error_exception();
    }


private:
    struct _callback {
        ring_buffer_callback callback;
        size_t threshold;
    };


    T* buffer;
    size_t capacity, _read, _write;
#ifdef RING_BUFFER_THREAD_SAFETY
    pthread_mutex_t lock;
#endif
    _callback read_callback, write_callback;


    inline size_t ring_buffer_readable() { return _write - _read; }
    inline size_t ring_buffer_writable() { return capacity - ring_buffer_readable(); }
};

typedef generic_ring_buffer<void> ring_buffer;
