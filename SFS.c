#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "boot_sector.h"

#define ASSERT

#ifdef ASSERT
	#include <assert.h>
#else
	#define assert(X) { /*nothing */ }
#endif

#define HIWORD(X) ((0xF0 & X) >> 4)
#define LOWORD(X) ((0x0F & X))
#define ENDIANSWAP(X) (HIWORD(X) | (LOWORD(X) << 4))

typedef enum {
	DISKINFO,
	DISKLIST,
	DISKGET,
	DISKPUT,
	DISK_ACTION_NONE = -1
} DISK_ACTION;

DISK_ACTION checkProgram(const char* executed_name)
{
	char* input = strdup(executed_name);
	char* prog_name = strrchr(input, '/');
	DISK_ACTION result = DISK_ACTION_NONE;
	
	// strrchr found the last forward slash
	// we want just he program name;
	++prog_name;

	if      (strcasecmp(prog_name, "diskinfo") == 0)
		result = DISKINFO;
	else if (strcasecmp(prog_name, "disklist") == 0)
		result = DISKLIST;
	else if (strcasecmp(prog_name, "diskget" ) == 0)
		result = DISKGET;
	else if (strcasecmp(prog_name, "diskput" ) == 0)
		result = DISKPUT;

	free(input);

	return result;
}

void quit(char* reason)
{
	if (reason != NULL)
		fprintf(stderr, "%s\n", reason);

	fprintf(stderr, "Exiting...\n");

	exit(EXIT_FAILURE);
} 

void usage(DISK_ACTION action)
{
	printf("Simple File System Usage:\n");
	
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

void diskinfo(byte* disk)
{
	boot_sector boot;
	for (int i = 0; i < 62; ++i)
	{
		boot._[i].value = disk[i].value;
	}
	
	int num_sectors = boot.data.Small_Sectors.value != 0
					? boot.data.Small_Sectors.value
					: boot.data.Large_Sectors.value;

	int FAT1_offset = boot.data.reserved_Sectors.value
					* boot.data.Bytes_Per_Sector.value;

	int FAT2_offset = FAT1_offset
					+ boot.data.Sectors_Per_FAT.value
					* boot.data.Bytes_Per_Sector.value;

	int root_offset = FAT1_offset
					+ boot.data.FATs.value
					* boot.data.Sectors_Per_FAT.value
					* boot.data.Bytes_Per_Sector.value;

	int data_offset = root_offset
					+ boot.data.Max_Root_Entries.value
					* 32;

	int total_size  = num_sectors
					* boot.data.Bytes_Per_Sector.value;

	int FAT_size    = 2
					* (boot.data.Bytes_Per_Sector.value
					* boot.data.Sectors_Per_FAT.value)
					/ 3;

	FAT_entry* table = calloc(1, FAT_size* sizeof(FAT_entry));
	
	unsigned int num_alloced = 0;
	unsigned int num_files = 0;

	for (int i = 0; i < FAT_size; ++i)
	{
		int a_offset = 3 * i;
		a_offset /= 2;

		byte a = disk[FAT1_offset + a_offset];
		byte b = disk[FAT1_offset + a_offset + 1];
		
		if (i % 2 == 0)
		{	
			table[i].L = b.L;
			table[i].M = a.H;
			table[i].H = a.L;
		}
		else
		{
			table[i].L = b.H;
			table[i].M = b.L;
			table[i].H = a.H;
		}
		
		// Increment the num_alloced counter if this FAT entry is non-zero
		// Fat entries 0 and 1 are reserved
		if (i > 1 && table[i].value != 0)
		{
			num_alloced += 1;
		
			directory_entry sector;
		
			for ( int j = 0; j < 32; ++j)
			{
				sector._[j].value = disk[root_offset + (i-2) * 32 + j].value;
			}
			
			// Collect and count things here
			if (sector._[0].value != 0x0 && sector._[0].value != 0xE5)
			if ((sector.data.Attributes.value & (READ_ONLY | HIDDEN | VOL_LABEL | SYSTEM | SUBDIR | ARCHIVE)) == 0)
			{
				// This thing is a file
				num_files += 1;
			}
			
			if (sector.data.Attributes.value == VOL_LABEL)
			{
				for (int j = 0; j < 8; ++j)
				{
					boot.data.Volume_Label[j].value = sector.data.Filename[j].value;
				}
			}
		}
	}

	free(table);

	unsigned int free_space = total_size - num_alloced * boot.data.Sectors_Per_Cluster.value * boot.data.Bytes_Per_Sector.value; 
	
	// I could copy these to real char[]'s rather than byte[]'s but it works as is
	#pragma GCC diagnostic ignored "-Wformat"	
	printf("OS Name : %." VALOF(LEN_OEM_name) "s\n", boot.data.OEM_name);
	printf("Label of the disk : %." VALOF(LEN_Volume_Label) "s\n", boot.data.Volume_Label);
	#pragma GCC diagnostic warning "-Wformat"
	printf("Total size of the disk : %d\n", total_size);
	printf("Free size of the disk : %d\n", free_space);
	printf("===  ===  ===  ===  ===\n");
	printf("The number of files in the root directory(not including subdirectories) : %d\n", num_files);
	printf("===  ===  ===  ===  ===\n");
	printf("Number of FAT copies : %d\n", boot.data.FATs.value);
	printf("Sectors per FAT : %d\n", boot.data.Sectors_Per_FAT.value);
}

void disklist(byte* disk)
{

}

void diskget(byte* disk, char* filename)
{

}

void diskput(byte* disk, FILE* file)
{

}

int main(int argc, char** argv)
{
	DISK_ACTION run_prog = checkProgram(argv[0]);

	run_prog = DISKINFO;

	byte* disk = NULL;
	
	if (run_prog == DISK_ACTION_NONE)
	{
		usage(run_prog);
	}

	int disk_image = -1;

	if (argc >= 2 && argv[1] != NULL)
	{
		disk_image = open(argv[1], O_RDWR);
		if (disk_image == -1)
		{
			char* err = strerror(errno);
			quit(err);
		}
		struct stat disk_stat;
		fstat(disk_image, &disk_stat);
		
		disk = mmap(NULL, disk_stat.st_size, PROT_WRITE, MAP_SHARED, disk_image, 0);
		if (disk == NULL)
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
					FILE* put_file = fopen(argv[2], "rb");
					if (put_file == NULL)
					{
						char* err = strerror(errno);
						quit(err);
					}
					else diskput(disk, put_file);
				}
				else usage(DISKPUT);
				break;
			}
	
		default:
		case DISK_ACTION_NONE:
		break;
	}

	return EXIT_SUCCESS;

}
