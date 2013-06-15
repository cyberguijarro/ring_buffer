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
#include <cstring>
#include <mutex>


struct ring_buffer::ring_buffer_implementation {
    struct _callback {
        ring_buffer_callback callback;
        size_t threshold;
    };


    std::unique_ptr<char[]> buffer; 
    size_t capacity, _read, _write;
    _callback read_callback, write_callback;
    std::recursive_mutex mutex;


    inline size_t ring_buffer_readable() { return _write - _read; }
    inline size_t ring_buffer_writable() { return capacity - ring_buffer_readable(); }


    ring_buffer_implementation(size_t capacity) throw (ring_buffer_out_of_memory_exception) : capacity(capacity), _read(0), _write(0) {
        try {
            buffer.reset(new char[capacity]);
        } catch (std::bad_alloc) {
            throw ring_buffer_out_of_memory_exception{};
        }
    }


    // TBD: implement using constructor delegation (N1986)
    ring_buffer_implementation(ring_buffer_implementation* other) throw (std::system_error, ring_buffer_out_of_memory_exception) : capacity(other->capacity), _read(other->_read), _write(other->_write), read_callback(other->read_callback), write_callback(other->write_callback) {
        std::lock_guard<std::recursive_mutex> lock{other->mutex};

        try {
            buffer.reset(new char[capacity]);
            memcpy(buffer.get(), other->buffer.get(), capacity);
        } catch (std::bad_alloc) {
            throw ring_buffer_out_of_memory_exception{};
        }
    }


    void set_read_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error) {
        std::lock_guard<std::recursive_mutex> lock{mutex};

        read_callback.callback = callback;
        read_callback.threshold = threshold;
    }


    void set_write_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error) {
        std::lock_guard<std::recursive_mutex> lock{mutex};

        write_callback.callback = callback;
        write_callback.threshold = threshold;
    }


    void write(const void* data, size_t length) throw (std::system_error, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception) {
        if (0 != data) { // TBD: use nullptr
            std::lock_guard<std::recursive_mutex> lock{mutex};

            if (ring_buffer_writable() >= length) {
                auto left = length;

                do {
                    auto target = _write % capacity, size = std::min(left, capacity - target);

                    memcpy(buffer.get() + target, reinterpret_cast<const char*>(data) + length - left, size);
                    left -= size;
                    _write += size;
                } while (left > 0);

                if (read_callback.callback and (ring_buffer_readable() >= read_callback.threshold))
                    read_callback.callback();
            }
            else
                throw ring_buffer_overflow_exception{};
        }
        else
            throw ring_buffer_invalid_address_exception{};
    }


    void read(void* data, size_t length) throw (std::system_error, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception) {
        if (0 != data) { // TBD: use nullptr
            std::lock_guard<std::recursive_mutex> lock{mutex};

            if (ring_buffer_readable() >= length) {
                auto left = length;

                do {
                    auto target = _read % capacity, size = std::min(left, capacity - target);

                    memcpy(reinterpret_cast<char*>(data) + length - left, buffer.get() + target, size);
                    left -= size;
                    _read += size;
                } while (left > 0);

                if (write_callback.callback and (ring_buffer_writable() >= write_callback.threshold))
                    write_callback.callback();
            }
            else
                throw ring_buffer_underflow_exception{};
        }
        else
            throw ring_buffer_invalid_address_exception{};
    }


    void get_available(size_t& read, size_t& write) throw (std::system_error) {
        std::lock_guard<std::recursive_mutex> lock{mutex};

        read = ring_buffer_readable();
        write = ring_buffer_writable();
    }
};


ring_buffer::ring_buffer(size_t capacity) throw (std::system_error, ring_buffer_out_of_memory_exception) : implementation(new ring_buffer_implementation{capacity}) { }
ring_buffer::ring_buffer(ring_buffer& other) throw (std::system_error, ring_buffer_out_of_memory_exception) : implementation(new ring_buffer_implementation{other.implementation.get()}) { }
ring_buffer& ring_buffer::operator=(ring_buffer& other) throw (std::system_error, ring_buffer_out_of_memory_exception) { implementation.reset(new ring_buffer_implementation{other.implementation.get()}); return *this; }
void ring_buffer::set_read_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error) { implementation->set_read_callback(callback, threshold); }
void ring_buffer::set_write_callback(ring_buffer_callback callback, size_t threshold) throw (std::system_error) { implementation->set_write_callback(callback, threshold); }
void ring_buffer::write(const void* data, size_t length) throw (std::system_error, ring_buffer_overflow_exception, ring_buffer_invalid_address_exception) { implementation->write(data, length); }
void ring_buffer::read(void* data, size_t length) throw (std::system_error, ring_buffer_underflow_exception, ring_buffer_invalid_address_exception) { implementation->read(data, length); }
void ring_buffer::get_available(size_t& read, size_t& write) throw (std::system_error) { implementation->get_available(read, write); }
ring_buffer::~ring_buffer() throw (std::system_error) { }
