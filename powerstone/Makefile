CC=gcc
CFLAGS=-fno-builtin -Wno-implicit-int

SRC=adpcm.c blit.c crc.c fir.c pocsag.c ucbqsort.c bcnt.c engine.c g3fax.c qurt.c
OBJ:=$(patsubst %.c, %, $(SRC))

.PHONY: all

.c.o:
	$(CC) $(CFLAGS) $*.c -o $*

all: $(OBJ)
	tar cfz powerstone.tar.gz $(OBJ)

clean:
	rm -rf $(OBJ) powerstone.tar.gz
