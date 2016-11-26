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




