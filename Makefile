CC=gcc

LDLIBS=-ldl -lpthread

CFLAGS=-Wall 
CPPFLAGS=${CFLAGS}
HIREDIS=hiredis-1.0.2/alloc.c hiredis-1.0.2/dict.c hiredis-1.0.2/net.c hiredis-1.0.2/sds.c hiredis-1.0.2/async.c hiredis-1.0.2/hiredis.c hiredis-1.0.2/read.c hiredis-1.0.2/sockcompat.c

test: runner
	@cat src/lock.lua | redis-cli -h 192.168.0.211 -x FUNCTION LOAD REPLACE
	@./runner

runner: genvfs.so tests/check_genvfs.c src/genvfs.c src/genvfs.h lz4-1.9.3/lib/lz4.c ${HIREDIS}
	@gcc ${CPPFLAGS} -fPIC tests/check_genvfs.c src/genvfs.c src/store.c lz4-1.9.3/lib/lz4.c ${HIREDIS} -o runner -lcheck -lrt -lpthread -lm -lsubunit -lhiredis

# sqlite extension module
genvfs.so: src/genvfs.c src/genvfs.h src/store.c src/store.h lz4-1.9.3/lib/lz4.c ${HIREDIS}
	@gcc ${CPPFLAGS} ${INCLUDES} -fPIC -shared src/genvfs.c src/store.c lz4-1.9.3/lib/lz4.c ${HIREDIS} -o genvfs.so $(LBLIBS)

sqlite3:
	@gcc -DHAVE_READLINE=1 -DSQLITE_ENABLE_BATCH_ATOMIC_WRITE=1 sqlite/shell.c sqlite/sqlite3.c -lpthread -ldl -lm -lreadline -o sqlite3

clean:
	@rm -f genvfs.so sqlite3
