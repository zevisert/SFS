#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "packed_types.h"
#include "boot_sector.h"
#include "FAT_entry.h"
#include "directory_sector.h"

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
	// we want just the program name;
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

void quit(const char* reason)
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

/* DISK INFO 
 * Scan over the boot and root directories and gather some common statistics about the disk.
 * @param const byte* : disk - A pointer to a memory mapped array representing a FAT12 disk image
 * @returns void - Collected information is printed to the console as this routine is completed.
 */ 
void diskinfo(const byte* disk)
{
	// Boot sector is a properly aligned and packed
	// unionized structure representing the boot sector of
	// a FAT12 disk image.
	boot_sector boot;

	// Initialize the boot sector struct by copying in data from the disk.
	for (int i = 0; i < 62; ++i)
	{
		boot.raw[i].value = disk[i].value;
	}
	
	// Assert that we're working with a FAT12 disk by
	// looking for the FAT12 label in the File System field

	// Start by copying and null-terminating the string
	char FS_type[LEN_File_System_Type + 1];
	memcpy(&FS_type, boot.data.File_System_Type, LEN_File_System_Type);
	FS_type[LEN_File_System_Type] = '\0';
	
	if (strstr(FS_type, "FAT12") == NULL)
	{
		// Didn't find it... 
		quit("Disk doesn't list file system type as \"FAT12\"");
	}
	
	// The number of sectors on the disk could be stored in 
	// two places. Small_Sectors or Large_Sectors iff the
	// former is equal to zero.
	const 
	int num_sectors = boot.data.Small_Sectors.value != 0
					? boot.data.Small_Sectors.value
					: boot.data.Large_Sectors.value;

	// Calculate the location of the first FAT table
	const 
	int FAT1_offset = boot.data.reserved_Sectors.value
					* boot.data.Bytes_Per_Sector.value;

	// Calculate the location of the backup, second FAT table
	const
	int FAT2_offset = FAT1_offset
					+ boot.data.Sectors_Per_FAT.value
					* boot.data.Bytes_Per_Sector.value;

	// Calculate the location of the root directory sector
	const
	int root_offset = FAT1_offset
					+ boot.data.FATs.value
					* boot.data.Sectors_Per_FAT.value
					* boot.data.Bytes_Per_Sector.value;

	// Calculate the location of the start of the data region
	const
	int data_offset = root_offset
					+ boot.data.Max_Root_Entries.value
					* 32;

	// Total disk size can also be calculated at this point
	const
	int total_size  = num_sectors
					* boot.data.Bytes_Per_Sector.value;

	// Calculate the number of entries in the FAT tables.
	// Since this is a FAT12 image, every 3 bytes makes up 2 
	// FAT table entries. The number of FAT entries is thus 
	// two thirds of the number of bytes per FAT table
	const
	int FAT_size    = 2
					* (boot.data.Bytes_Per_Sector.value
					* boot.data.Sectors_Per_FAT.value)
					/ 3;

	// We'll duplicate the fat table for ease of use
	FAT_entry* table = calloc(1, FAT_size* sizeof(FAT_entry));
	
	// By scanning through the FAT table and root directory
	// we'll collect the number of allocated FAT entries - to
	// calculate the free size by difference from the total, as
	// well as the number of files in the root directory.
	unsigned int num_alloced = 0;
	unsigned int num_files = 0;

	// Scan through the fat table 
	for (int i = 0; i < FAT_size; ++i)
	{
		// Grab the two bytes that support this FAT entry
		byte a = disk[FAT1_offset + 3 * i / 2];
		byte b = disk[FAT1_offset + 3 * i / 2 + 1];
		
		// If the entry is even...
		if (i % 2 == 0)
		{	
			// ... use the low order bits from the higher location
			table[i].L = b.L;
			// And the whole byte in the base location
			table[i].M = a.H;
			table[i].H = a.L;
		}
		else
		{
			// ... use the whole byte from the high location
			table[i].L = b.H;
			table[i].M = b.L;
			// and the high order bits from the lower location
			table[i].H = a.H;
		}
		
		// Try and interpret the contents of the root directory sector for this FAT
		// entry if the entry is non-zero
		if (table[i].value != 0)
		{
			// The FAT entry is non-zero, so the corresponding data region is allocated
			num_alloced += 1;
		
			// Just like for the boot data sector this type
			// is a properly aligned and packed unionized structure
			// for interpreting a sector's data
			directory_entry sector;
		
			// Initialize the sector with the disk contents
			for (int j = 0; j < 32; ++j)
			{
				sector.raw[j].value = disk[root_offset + (i - 2) * 32 + j].value;
			}
			
			// Inspect the sector for files
			if (sector.raw[0].value != 0x0 &&
				sector.raw[0].value != 0xE5 &&
				(sector.data.Attributes.value & (VOL_LABEL | SYSTEM | SUBDIR | ARCHIVE)) == 0)
			{
					// This thing is a file
					num_files += 1;
			}
			
			// Check for volume labels in the root directory,
			// so long as the boot sector didn't have one
			if (sector.data.Attributes.value == VOL_LABEL)
			{
				// Found a volume label, we'll store in the boot sector's heap memory
				for (int j = 0; j < 8; ++j)
				{
					boot.data.Volume_Label[j].value = sector.data.Filename[j].value;
				}
			}
		}
	}

	// Done examining the FAT table, copied everything needed
	free(table);

	// Calculate the remainder (free) space using the number of allocated entries 
	// from the FAT table
	const
	unsigned int free_space = total_size
							- num_alloced
							* boot.data.Sectors_Per_Cluster.value
							* boot.data.Bytes_Per_Sector.value; 

	/////////////////////////
	//    OUTPUT   INFO    //
	/////////////////////////

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

void disklist(const byte* disk)
{

}

void diskget(const byte* disk, const char* filename)
{

}

void diskput(const byte* disk, const FILE* file)
{

}

int main(int argc, char** argv)
{
	DISK_ACTION run_prog = checkProgram(argv[0]);

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
