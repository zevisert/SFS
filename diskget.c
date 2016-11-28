#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "directory_sector.h"
#include "FAT_entry.h"
#include "boot_sector.h"

#include "SFS.h"

void diskget(const byte* disk, const char* get_filename)
{
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
		load_FAT_entry(table, disk, boot_calc.FAT1_offset, i);
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
			for (int j = 0; j < sizeof(directory_entry); ++j)
			{
				sector.raw[j].value = disk[boot_calc.root_offset + (i - 2) * sizeof(directory_entry) + j].value;
			}

			// Inspect the sector for files
			if (sector.raw[0].value != 0x0 &&
				sector.raw[0].value != 0xE5 &&
				(sector.data.Attributes.value & (VOL_LABEL | SYSTEM | SUBDIR | ARCHIVE)) == 0)
			{
				trim_filename(filename, sector.data.Filename, sector.data.Extension);

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
						success = true;
						break;
					}
					
					quit("Failed to open or create a file in the active directory.");
				}
			}
		}
	}

	// Done examining the FAT table, copied everything needed
	free(table);

	printf("%s\n", success ? "File retrieved." : "Failed to retrieve file");
}
