CC=gcc
CPP=g++
SHELL=bash

INCLUDES=-I./sqlite -I/usr/include/hiredis
LDLIBS=-l:libsqlite3.a -ldl -lpthread -lhiredis

CFLAGS=-Wall 
CPPFLAGS=${CFLAGS}

test: runner
	@cat src/lock.lua | redis-cli -h 192.168.0.211 -x FUNCTION LOAD REPLACE
	@./runner

runner: genvfs.so tests/check_genvfs.c src/genvfs.c src/genvfs.h lz4-1.9.3/lib/lz4.c
	@gcc ${CPPFLAGS} -fPIC tests/check_genvfs.c src/genvfs.c src/store.c lz4-1.9.3/lib/lz4.c -o runner -lcheck -lrt -lpthread -lm -lsubunit -lhiredis

# sqlite extension module
genvfs.so: src/genvfs.c src/genvfs.h src/store.c src/store.h lz4-1.9.3/lib/lz4.c
	@gcc ${CPPFLAGS} ${INCLUDES} -fPIC -shared src/genvfs.c src/store.c lz4-1.9.3/lib/lz4.c -o genvfs.so $(LBLIBS)

clean:
	@rm -f genvfs.so
