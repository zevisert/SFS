CC=gcc 
CFLAGS=-std=gnu99 -Wall

HEADERS=SFS.h directory_sector.h boot_sector.h FAT_entry.h packed_types.h

all: Build SFS  link

remake: clean all

SFS: SFS.o diskinfo.o disklist.o diskget.o diskput.o
	$(CC) Build/diskinfo.o Build/disklist.o Build/diskget.o Build/diskput.o Build/SFS.o -o SFS

SFS.o: SFS.c $(HEADERS)
	$(CC) $(CFLAGS) -c SFS.c -o Build/SFS.o

diskinfo.o: diskinfo.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskinfo.c -o Build/diskinfo.o

disklist.o: disklist.c $(HEADERS)
	$(CC) $(CFLAGS) -c disklist.c -o Build/disklist.o

diskget.o: diskget.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskget.c -o Build/diskget.o

diskput.o: diskput.c $(HEADERS)
	$(CC) $(CFLAGS) -c diskput.c -o Build/diskput.o

Build:
	mkdir Build

link:
	ln -sf SFS diskinfo
	ln -sf SFS disklist
	ln -sf SFS diskget
	ln -sf SFS diskput

clean:
	rm -rf Build/ ./SFS diskinfo disklist diskget diskput
