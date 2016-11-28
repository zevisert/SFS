#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "directory_sector.h"
#include "FAT_entry.h"
#include "boot_sector.h"

#include "SFS.h"

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
		load_FAT_entry(table, disk, boot_calc.FAT1_offset, i);

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
	
	directory_entry existing_entry;
	char* filename_compare = calloc(1, LEN_Filename + 1 + LEN_Extension + 1);

	// Check if a file with this name already exists
	for (int i = boot_calc.root_offset; i < boot_calc.data_offset; i += sizeof(directory_entry))
	{
		for (int j = 0; j < sizeof(directory_entry); ++j)
		{
			existing_entry.raw[j].value = disk[i + j].value;		
		}
		
		if (existing_entry.raw[0].value != 0x0 &&
			existing_entry.raw[0].value != 0xE5 &&
			(existing_entry.data.Attributes.value & (VOL_LABEL | SYSTEM | ARCHIVE)) == 0)
		{
			// This entry represents a file
			trim_filename(filename_compare, existing_entry.data.Filename, existing_entry.data.Extension);

			if (strcasecmp(filename_compare, input_filename) == 0)
			{
				// A file with this name already exists on the disk
				free(filename_compare);
				free(table);
				fclose(file);
				quit("A file with this name already exists on the disk");
			}
		}
	}
	
	free(filename_compare);

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

			// Calculate location and size of the block write
			sector_location = boot_calc.data_offset + (i - 2) * boot.data.Bytes_Per_Sector.value;
			bytes_to_copy = MIN(file_size_remaining, boot.data.Bytes_Per_Sector.value);
			file_size_remaining -= file_size_remaining > boot.data.Bytes_Per_Sector.value ? boot.data.Bytes_Per_Sector.value : file_size_remaining;

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
			update_disk_FAT(table, disk, boot_calc.FAT1_offset, update_idx);
			update_disk_FAT(table, disk, boot_calc.FAT2_offset, update_idx);

			// With FAT tables updated, write the corresponding block of data to the data region
			memset(&block, '\0', boot.data.Bytes_Per_Sector.value);
			fread(&block, sizeof(char), bytes_to_copy, file);
			
			// Write this block to the disk
			memcpy(&disk[sector_location], block, bytes_to_copy);

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
