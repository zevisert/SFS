#pragma once

#include "packed_types.h"

#ifndef VALOF
#define VALOF(X) STR(X)		  
#endif // !VALOF

#ifndef STR
#define STR(X) #X			  
#endif // !STR

#define LEN__JUMP               3
#define LEN_OEM_name            8
#define LEN_Volume_Label       11
#define LEN_File_System_Type    8

#pragma pack(push)
struct boot_sector_portions
{
	byte   _JUMP               [LEN__JUMP];
	byte   OEM_name            [LEN_OEM_name];
	word   Bytes_Per_Sector;
	byte   Sectors_Per_Cluster;
	word   reserved_Sectors;
	byte   FATs;
	word   Max_Root_Entries;
	word   Small_Sectors;
	byte   Media_Descriptor;
	word   Sectors_Per_FAT;
	word   Sectors_Per_Track;
	word   Heads;
	dword  Hidden_Sectors;
	dword  Large_Sectors;
	word   _IGNORE;
	byte   Boot_Signature;
	dword  Volume_ID;
	byte   Volume_Label        [LEN_Volume_Label];
	byte   File_System_Type    [LEN_File_System_Type];
	// The remainder of the boot sector is occupied by BIOS code
};
#pragma pack(pop)

typedef struct boot_sector_t
{
	union
	{
		byte raw[62];
		struct boot_sector_portions data;
	};
} boot_sector;

typedef struct 
{
	unsigned int num_sectors;
	unsigned int FAT1_offset;
	unsigned int FAT2_offset;
	unsigned int root_offset;
	unsigned int data_offset;
	unsigned int total_size;
	unsigned int FAT_size;
} boot_extra;

static inline boot_extra initialize_boot(boot_sector* boot, const byte* disk)
{
	boot_extra extra;
	
	// Initialize the boot sector struct by copying in data from the disk.
	for (int i = 0; i < 62; ++i)
	{
		boot->raw[i].value = disk[i].value;
	}

	// The number of sectors on the disk could be stored in 
	// two places. Small_Sectors or Large_Sectors iff the
	// former is equal to zero.
	extra.num_sectors = boot->data.Small_Sectors.value != 0
					  ? boot->data.Small_Sectors.value
			          : boot->data.Large_Sectors.value;

	// Calculate the location of the first FAT table
	extra.FAT1_offset = boot->data.reserved_Sectors.value
					* boot->data.Bytes_Per_Sector.value;

	// Calculate the location of the backup, second FAT table
	extra.FAT2_offset = extra.FAT1_offset
					+ boot->data.Sectors_Per_FAT.value
					* boot->data.Bytes_Per_Sector.value;

	// Calculate the location of the root directory sector
	extra.root_offset = extra.FAT1_offset
					+ boot->data.FATs.value
					* boot->data.Sectors_Per_FAT.value
					* boot->data.Bytes_Per_Sector.value;

	// Calculate the location of the start of the data region
	extra.data_offset = extra.root_offset
					+ boot->data.Max_Root_Entries.value
					* 32;
	
	// Total disk size can also be calculated at this point
	extra.total_size = extra.num_sectors
					* boot->data.Bytes_Per_Sector.value;

	// Calculate the number of entries in the FAT tables.
	// Since this is a FAT12 image, every 3 bytes makes up 2 
	// FAT table entries. The number of FAT entries is thus 
	// two thirds of the number of bytes per FAT table
	extra.FAT_size  = 2
					* (boot->data.Bytes_Per_Sector.value
					* boot->data.Sectors_Per_FAT.value)
					/ 3;

	return extra;
}



