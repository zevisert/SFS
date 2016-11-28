#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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
#define MIN(X, Y) (X > Y ? Y : X)

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
	prog_name = prog_name ? ++prog_name : input;

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
	boot_extra boot_calc = initialize_boot(&boot, disk);
	
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
	
	// Reserve some space for the system label, as it could be
	// stored in several places
	char label[LEN_Volume_Label + 1];
	memcpy(&label, boot.data.Volume_Label, LEN_Volume_Label);
	label[LEN_Volume_Label] = '\0';

	// We'll duplicate the fat table for ease of use
	FAT_entry* table = calloc(1, boot_calc.FAT_size* sizeof(FAT_entry));
	
	// By scanning through the FAT table and root directory
	// we'll collect the number of allocated FAT entries - to
	// calculate the free size by difference from the total, as
	// well as the number of files in the root directory.
	unsigned int num_alloced = 0;
	unsigned int num_files = 0;

	// Scan through the fat table 
	for (int i = 0; i < boot_calc.FAT_size; ++i)
	{
		load_FAT_entry(&table, disk, boot_calc.FAT1_offset, i);
		
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
				sector.raw[j].value = disk[boot_calc.root_offset + (i - 2) * 32 + j].value;
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
				for (int j = 0; j < LEN_Volume_Label; ++j)
				{
					label[j] = sector.data.Filename[j].value;
				}
			}
		}
	}

	// Done examining the FAT table, copied everything needed
	free(table);

	// Calculate the remainder (free) space using the number of allocated entries 
	// from the FAT table
	const
	unsigned int free_space = boot_calc.total_size
							- num_alloced
							* boot.data.Sectors_Per_Cluster.value
							* boot.data.Bytes_Per_Sector.value; 

	// Output the data here

	// I could copy these to real char[]'s rather than byte[]'s but it works as is
	#pragma GCC diagnostic ignored "-Wformat"	
	printf("OS Name : %." VALOF(LEN_OEM_name) "s\n", boot.data.OEM_name);
	#pragma GCC diagnostic warning "-Wformat"

	printf("Label of the disk : %s\n", label);
	printf("Total size of the disk : %d\n", boot_calc.total_size);
	printf("Free size of the disk : %d\n", free_space);
	printf("===  ===  ===  ===  ===\n");
	printf("The number of files in the root directory(not including subdirectories) : %d\n", num_files);
	printf("===  ===  ===  ===  ===\n");
	printf("Number of FAT copies : %d\n", boot.data.FATs.value);
	printf("Sectors per FAT : %d\n", boot.data.Sectors_Per_FAT.value);
}

void disklist(const byte* disk)
{
	// Boot sector is a properly aligned and packed
	// unionized structure representing the boot sector of
	// a FAT12 disk image.
	boot_sector boot;
	boot_extra boot_calc = initialize_boot(&boot, disk);

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
	
	// We'll duplicate the fat table for ease of use
	FAT_entry* table = calloc(1, boot_calc.FAT_size * sizeof(FAT_entry));
	
	// We'll also only be using short filenames for this program
	// so let's allocate some room for one
	char filename[LEN_Filename + 1];
	memset(&filename, '\0', LEN_Filename + 1);
	
	// Scan through the fat table 
	for (int i = 0; i < boot_calc.FAT_size; ++i)
	{
		load_FAT_entry(&table, disk, boot_calc.FAT1_offset, i);
		
		// Try and interpret the contents of the root directory sector for this FAT
		// entry if the entry is non-zero
		if (table[i].value != 0)
		{
			// Just like for the boot data sector this type
			// is a properly aligned and packed unionized structure
			// for interpreting a sector's data
			directory_entry sector;
		
			// Initialize the sector with the disk contents
			for (int j = 0; j < 32; ++j)
			{
				sector.raw[j].value = disk[boot_calc.root_offset + (i - 2) * 32 + j].value;
			}
			
			// Inspect the sector for files
			if (sector.raw[0].value != 0x0 &&
				sector.raw[0].value != 0xE5 &&
				(sector.data.Attributes.value & (VOL_LABEL | SYSTEM | ARCHIVE)) == 0)
			{
				if ((sector.data.Attributes.value & SUBDIR) == 0)
				{
					// This thing is a file
					printf("F %d ", sector.data.File_Size.value);
				}
				else 
				{
					// This is a directory
					printf("D ");
				}
	
				// Collect and trim padded spaces from the filename
				memcpy(&filename, sector.data.Filename, LEN_Filename);
				for (int j = LEN_Filename - 1; j > 0; --j)
				{
					if (filename[j] == ' ')
					{
						filename[j] = '\0';
					}
					else break; 
				}

				#pragma GCC diagnostic ignored "-Wformat"
				printf("%s.%." VALOF(LEN_Extension) "s ", filename, sector.data.Extension);	
				#pragma GCC diagnostic warning "-Wformat"
				printf("%d/%d/%d ", DATE(sector.data.Creation_Date.value));
				printf("%02d:%02d\n", TIME(sector.data.Creation_Time.value));
			}

		}
	}

	// Done examining the FAT table, copied everything needed
	free(table);
}

void diskget(const byte* disk, const char* get_filename)
{
	// Boot sector is a properly aligned and packed
	// unionized structure representing the boot sector of
	// a FAT12 disk image.
	boot_sector boot;
	boot_extra boot_calc = initialize_boot(&boot, disk);

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
	
	// We'll duplicate the fat table for ease of use
	FAT_entry* table = calloc(1, boot_calc.FAT_size * sizeof(FAT_entry));
	
	// We'll also only be using short filenames for this program
	// so let's allocate some room for one
	char filename[LEN_Filename + 1 + LEN_Extension + 1];
	memset(&filename, '\0', LEN_Filename + 1 + LEN_Extension + 1);
	
	// Load the fat table so we can jump around later
	for (int i = 0; i < boot_calc.FAT_size; ++i)
	{
		load_FAT_entry(&table, disk, boot_calc.FAT1_offset, i);
	}

	// Scan the fat table
	for (int i = 2; i < boot_calc.FAT_size; ++i)
	{
		// Try and interpret the contents of the root directory sector for this FAT
		// entry if the entry is non-zero
		if (table[i].value != 0)
		{
		
			// Just like for the boot data sector this type
			// is a properly aligned and packed unionized structure
			// for interpreting a sector's data
			directory_entry sector;
		
			// Initialize the sector with the disk contents
			for (int j = 0; j < 32; ++j)
			{
				sector.raw[j].value = disk[boot_calc.root_offset + (i - 2) * sizeof(directory_entry) + j].value;
			}

			// Inspect the sector for files
			if (sector.raw[0].value != 0x0 &&
				sector.raw[0].value != 0xE5 &&
				(sector.data.Attributes.value & (VOL_LABEL | SYSTEM | SUBDIR | ARCHIVE)) == 0)
			{
				// Collect and trim padded spaces from the filename
				memcpy(&filename, sector.data.Filename, LEN_Filename);
				int j = LEN_Filename - 1; 
				for (; j > 0; --j)
				{
					if (filename[j] == ' ')
					{
						filename[j] = '\0';
					}
					else break; 
				}
				// Place the extension after the last padded space
				filename[++j] = '.';
				memcpy(&filename[j + 1], sector.data.Extension, LEN_Extension);

				if (strcasecmp(filename, get_filename) == 0)
				{
					// Found our file, get something ready for writing
					FILE* out = fopen(get_filename, "w+");
					if (out != NULL)
					{
						// Allocate a block as a buffer for moving data across files
						int sector_location;
						
						unsigned int file_size_remaining = sector.data.File_Size.value;
						unsigned int bytes_to_copy; 

						// Follow this FAT chain
						int chain = sector.data.First_Logical_Cluster.value;
						do 
						{
							if (file_size_remaining == 0) break;
							sector_location = boot_calc.data_offset + (chain - 2) * boot.data.Bytes_Per_Sector.value;
							bytes_to_copy = MIN(file_size_remaining, boot.data.Bytes_Per_Sector.value);
							file_size_remaining -= file_size_remaining >= boot.data.Bytes_Per_Sector.value ? boot.data.Bytes_Per_Sector.value : file_size_remaining;
							
							// Write this block to the output stream
							fwrite(&disk[sector_location], sizeof(byte), bytes_to_copy, out);
						} while (chain = table[chain].value, chain != 0xFFF);
						
						// Done reading
						fclose(out);
		
						// End the program
						break;
					}
					
					quit("Failed to open or create a file in the active directory.");
				}
			}
		}
	}


	// Done examining the FAT table, copied everything needed
	free(table);
}

void diskput(byte* disk, FILE* file, const char* input_filename)
{
	// Used for logging a status message at completion
	bool success = false;

	// Boot sector is a properly aligned and packed
	// unionized structure representing the boot sector of
	// a FAT12 disk image.
	boot_sector boot;
	boot_extra boot_calc = initialize_boot(&boot, disk);

	// Assert that we're working with a FAT12 disk by
	// looking for the FAT12 label in the File System field

	// Start by copying and null-terminating the string
	char FS_type[LEN_File_System_Type + 1];
	memcpy(&FS_type, boot.data.File_System_Type, LEN_File_System_Type);
	FS_type[LEN_File_System_Type] = '\0';
	
	if (strstr(FS_type, "FAT12") == NULL)
	{
		// Didn't find it...
		fclose(file);
		quit("Disk doesn't list file system type as \"FAT12\"");
	}
	
	// We'll duplicate the fat table for ease of use
	FAT_entry* table = calloc(1, boot_calc.FAT_size* sizeof(FAT_entry));
	
	// We'll also only be using short filenames for this program
	// so let's allocate some room for one
	char filename[LEN_Filename + 1 + LEN_Extension + 1];
	memset(&filename, '\0', LEN_Filename + 1 + LEN_Extension + 1);
	
	unsigned int num_alloced = 0; 
	// Load the fat table so we can jump around later
	for (int i = 0; i < boot_calc.FAT_size; ++i)
	{
		load_FAT_entry(&table, disk, boot_calc.FAT1_offset, i);

		// The FAT entry is non-zero, so the corresponding data region is allocated
		num_alloced += (table[i].value != 0) ? 1 : 0;
	}
	
	// Calculate the remainder (free) space using the number of allocated entries 
	// from the FAT table
	const
	unsigned int free_space = boot_calc.total_size
							- num_alloced
							* boot.data.Sectors_Per_Cluster.value
							* boot.data.Bytes_Per_Sector.value;

	directory_entry write_sector = initialize_write_sector(file, input_filename);

	// If the file size we're trying to place exceeds the free space, warn and halt
	if (write_sector.data.File_Size.value > free_space)
	{
		fclose(file);
		free(table);
		quit("Cannot write file to disk, insufficient free space.");
	}

	unsigned int file_size_remaining = write_sector.data.File_Size.value;
	unsigned int sector_location;
	unsigned int bytes_to_copy;
	bool found_first = false;

	char block[boot.data.Bytes_Per_Sector.value];
	
	// Scan the fat table
	for (int i = 2; file_size_remaining > 0 && i < boot_calc.FAT_size; ++i)
	{
		// Check each entry for an empty identifier, meaning we can write here
		if (table[i].value == 0)
		{
			// Cache the value of the first logical cluster
			if (found_first == false)
			{
				write_sector.data.First_Logical_Cluster.value = i;
				found_first = true;
				
				// Find a free directory entry
				for (int k = 2; boot_calc.root_offset + (k - 2) * sizeof(directory_entry) < boot_calc.data_offset; ++k)
				{
					byte first_byte = disk[boot_calc.root_offset + (k - 2) * sizeof(directory_entry)];
					if (first_byte.value == 0x00 || first_byte.value == 0xE5)
					{
						// Overwrite the a free directory entry with our directory info.
						for (int j = 0; j < sizeof(directory_entry); ++j)
						{
							disk[boot_calc.root_offset + (k - 2) * sizeof(directory_entry) + j].value = write_sector.raw[j].value;
						}
						break;
					}
				}
			}

			// Write the corresponding block of data to the data region
			sector_location = boot_calc.data_offset + (i - 2) * boot.data.Bytes_Per_Sector.value;
			bytes_to_copy = MIN(file_size_remaining, boot.data.Bytes_Per_Sector.value);
			file_size_remaining -= file_size_remaining > boot.data.Bytes_Per_Sector.value ? boot.data.Bytes_Per_Sector.value : file_size_remaining;

			memset(&block, NULL, boot.data.Bytes_Per_Sector.value);
			fread(&block, sizeof(char), bytes_to_copy, file);
			
			// Write this block to the disk
			memcpy(&disk[sector_location], block, bytes_to_copy);

			// Location of the next FAT entry
			int next = 0xFFF;
			int update_idx = i;
			// Do we need another sector to complete the write?
			if (file_size_remaining > 0)
			{
				// Seek to the next free FAT entry
				for (next = i + 1; table[next].value != 0 && next < boot_calc.FAT_size; ++next)
					;

				// Continue the next round from next, use -1 here because loop will auto-increment i
				i = next - 1;
			}

			// Point this FAT entry to the next one
			table[update_idx].value = next;

			// Propigate the changes to the two tables on the disk			
			update_disk_FAT(&table, disk, boot_calc.FAT1_offset, update_idx);
			update_disk_FAT(&table, disk, boot_calc.FAT2_offset, update_idx);

			// There's no more data, next is 0xFFF
			if (next == 0xFFF)
			{
				success = true;
				break;				
			}
		}
	}

	printf("%s\n", success ? "File written." : "Failed to write file to disk.");

	// Done examining the FAT, copied everything needed
	free(table);
}

int main(int argc, char** argv)
{
	DISK_ACTION run_prog = checkProgram(argv[0]);

	byte* disk = NULL;
	
	if (run_prog == DISK_ACTION_NONE)
	{
		usage(run_prog);
	}

	int disk_image;
	int disk_size = 0;

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
		disk_size = disk_stat.st_size;
		
		disk = mmap(NULL, disk_size, PROT_WRITE, MAP_SHARED, disk_image, 0);
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

	return EXIT_SUCCESS;

}
