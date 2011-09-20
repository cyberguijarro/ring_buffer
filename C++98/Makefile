CPPFLAGS=-DRING_BUFFER_THREAD_SAFETY
CXXFLAGS=-g -O0 -std=c++98 -Wall -pedantic
LDFLAGS=-lrt -lstdc++

test: ring_buffer.o test.o

clean:
	$(RM) *.o *.a test
