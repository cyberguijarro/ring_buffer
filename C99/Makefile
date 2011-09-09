CPPFLAGS=-DRING_BUFFER_THREAD_SAFETY
CFLAGS=-g -O0 -std=c99 -Wall -pedantic
LDFLAGS=-lrt

test: test.o ring_buffer.o

clean:
	$(RM) *.o test
