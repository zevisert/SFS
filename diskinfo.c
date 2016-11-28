#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_sector.h"
#include "FAT_entry.h"
#include "directory_sector.h"

#include "SFS.h"

/* DISK INFO 
 * Scan over the boot and root directories and gather some common statistics about the disk.
 * @param const byte* : disk - A pointer to a memory mapped array representing a FAT12 disk image
 * @returns void - Collected information is printed to the console as this routine is completed.
 *               - Otherwise the program prints an error to the console and exits with EXIT_FAILURE.
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
	for (int FAT_idx = 0; FAT_idx < boot_calc.FAT_size; ++FAT_idx)
	{
		load_FAT_entry(table, disk, boot_calc.FAT1_offset, FAT_idx);
		
		// Try and interpret the contents of the root directory sector for this FAT
		// entry if the entry is non-zero
		if (table[FAT_idx].value != 0)
		{
			// The FAT entry is non-zero, so the corresponding data region is allocated
			num_alloced += 1;
		
			// Just like for the boot data sector this type
			// is a properly aligned and packed unionized structure
			// for interpreting a sector's data
			directory_entry sector;
		
			// Initialize the sector with the disk contents
			for (int i = 0; i < sizeof(directory_entry); ++i)
			{
				sector.raw[i].value = disk[boot_calc.root_offset + (FAT_idx - 2) * sizeof(directory_entry) + i].value;
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
				for (int i = 0; i < LEN_Volume_Label; ++i)
				{
					label[i] = sector.data.Filename[i].value;
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