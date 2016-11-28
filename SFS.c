#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "packed_types.h"

#include "SFS.h"

// Our 4 processes, defintions required here since they're in seperate C files
extern void diskinfo(const byte* disk);
extern void disklist(const byte* disk);
extern void diskget(const byte* disk, const char* filename);
extern void diskput(const byte* disk, FILE* file, const char* input_filename);

int main(int argc, char** argv)
{
	DISK_ACTION run_prog = checkProgram(argv[0]);

	byte* disk = NULL;
	
	if (run_prog == DISK_ACTION_NONE)
	{
		usage(run_prog);
	}

	int disk_image = 0;
	int disk_size = 0;

	if (argc >= 2 && argv[1] != NULL)
	{
		// Retrieve a file descriptor for the disk image
		disk_image = open(argv[1], O_RDWR);
		if (disk_image == -1)
		{
			char* err = strerror(errno);
			quit(err);
		}

		// Get the disk image size from the file descriptor
		struct stat disk_stat;
		if (fstat(disk_image, &disk_stat) == -1)
		{
			char* err = strerror(errno);
			quit(err);
		}

		// Cache the disk size for when we unmap
		disk_size = disk_stat.st_size;
		
		// Map the disk image to our address space
		disk = mmap(NULL, disk_size, PROT_WRITE, MAP_SHARED, disk_image, 0);
		if (disk == NULL || (int)disk == -1)
		{
			char* err = strerror(errno);
			quit(err);
		}
	}
	else usage(run_prog);

	switch (run_prog)
	{
		case DISKINFO:
			{
				diskinfo(disk);
				break;
			}

		case DISKLIST: 
			{
				disklist(disk);
				break;
			}
		case DISKGET: 
			{
				if (argc == 3 && argv[2] != NULL)
				{
					diskget(disk, argv[2]);
				}
				else usage(DISKGET);
				break;
			}

		case DISKPUT: 
			{
				if (argc == 3 && argv[2] != NULL)
				{
					// Isolate the filename itself
					char* filename = strrchr(argv[2], '/');
					filename = filename ? ++filename : argv[2];

					FILE* put_file = fopen(argv[2], "rb");
					if (put_file == NULL)
					{
						char* err = strerror(errno);
						quit(err);
					}
					else diskput(disk, put_file, filename);
					
					fclose(put_file);
				}
				else usage(DISKPUT);
				break;
			}
	
		default:
		case DISK_ACTION_NONE:
		break;
	}

	if (munmap(disk, disk_size) != 0)
	{
		// Failed to unmount the disk
		char* err = strerror(errno);
		quit(err);
	}

	close(disk_image);

	return EXIT_SUCCESS;

}

DISK_ACTION checkProgram(const char* executed_name)
{
	char* input = strdup(executed_name);
	char* prog_name = strrchr(input, '/');
	DISK_ACTION result = DISK_ACTION_NONE;
	
	// strrchr found the last forward slash
	// we want just the program name;
	prog_name = prog_name ? ++prog_name : input;

	if (strcasecmp(prog_name, "diskinfo") == 0)
		result = DISKINFO;
	else if (strcasecmp(prog_name, "disklist") == 0)
		result = DISKLIST;
	else if (strcasecmp(prog_name, "diskget") == 0)
		result = DISKGET;
	else if (strcasecmp(prog_name, "diskput") == 0)
		result = DISKPUT;

	free(input);

	return result;
}

void quit(const char* reason)
{
	if (reason != NULL)
		fprintf(stderr, "%s\n", reason);

	fprintf(stderr, "Exiting...\n");

	exit(EXIT_FAILURE);
} 

void usage(DISK_ACTION action)
{
	printf("Simple File System (FAT12) Usage:\n");
	
	switch (action) 
	{
		default:
		case DISK_ACTION_NONE:
			{
				printf("  This program suite must be executed under one of the following names:\n");
				printf("    [ ./diskinfo | ./disklist | ./diskget | ./diskput ]\n");
			}
			break;

		case DISKINFO:
			{
				printf(" diskinfo <disk>\n");
				printf("    Processes the <disk> image and displays some basic information about the image.\n");
			}
			break;

		case DISKLIST:
			{
				printf(" disklist <disk>\n");
				printf("    Displays contents of the root directory of the <disk> image.\n");
			} 
			break;

		case DISKGET:
			{
				printf("  diskget <disk> <filename>\n");
				printf("    Retrieves <filename> from the <disk> image and places it in the current working directory\n");
			}
			break;
	
		case DISKPUT:
			{
				printf(" diskput <disk> <file>\n");
				printf("    Writes a copy of <file> to the root of <disk> if enough space is available\n");
			}
			break;
	}
	printf("\n");
	exit(EXIT_FAILURE);
}


