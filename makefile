CC=gcc 
CFLAGS=-std=gnu99 -Wall

HEADERS=SFS.h directory_sector.h boot_sector.h FAT_entry.h packed_types.h

all: SFS link

remake: clean all

SFS: SFS.o diskinfo.o disklist.o diskget.o diskput.o
	$(CC) diskinfo.o disklist.o diskget.o diskput.o SFS.o -o SFS

SFS.o: SFS.c $(HEADERS)
	$(CC) $(CFLAGS) -c SFS.c

diskinfo.o: diskinfo.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskinfo.c

disklist.o: disklist.c $(HEADERS)
	$(CC) $(CFLAGS) -c disklist.c

diskget.o: diskget.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskget.c

diskput.o: diskput.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskput.c

link:
	ln -sf SFS diskinfo
	ln -sf SFS disklist
	ln -sf SFS diskget
	ln -sf SFS diskput

clean:
	rm -rf *.o ./SFS
