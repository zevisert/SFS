CC=gcc 
CFLAGS=-std=gnu99 -Wall

all: SFS link

remake: clean all

SFS: SFS.c
	$(CC) $(CFLAGS) -o SFS SFS.c

link:
	ln -sf SFS diskinfo
	ln -sf SFS disklist
	ln -sf SFS diskget
	ln -sf SFS diskput

clean:
	rm -rf *.o ./SFS
