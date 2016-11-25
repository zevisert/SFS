#pragma once

#pragma pack (push, 1)
typedef union 
{
	struct
	{
		unsigned char L : 4; 
		unsigned char H : 4;
	};
	unsigned char value;
} byte;

typedef union 
{
	struct
	{
		byte low;
		byte high;
	};
	unsigned short value : 16;
} word;

typedef union 
{
	struct
	{
		word low;
		word high;
	};
	unsigned int value : 32;
} dword;
#pragma pack(pop)

#pragma pack(push, 4)
typedef union
{
	struct
	{
		unsigned char H : 4;
		unsigned char M : 4;
		unsigned char L : 4;
	};
	unsigned int value : 12;
} FAT_entry;
#pragma pack(pop)

#define VALOF(X) STR(X)
#define STR(X) #X

#define LEN__JUMP               3
#define LEN_OEM_name            8
#define LEN_Bytes_Per_Sector    2
#define LEN_Sectors_Per_Cluster 1
#define LEN_Reserved_Sectors    2
#define LEN_FATs                1
#define LEN_Max_Root_Entries    2
#define LEN_Small_Sectors       2
#define LEN_Media_Descriptor    1
#define LEN_Sectors_Per_FAT     2
#define LEN_Sectors_Per_Track   2
#define LEN_Heads               2
#define LEN_Hidden_Sectors      4
#define LEN_Large_Sectors       4
#define LEN__IGNORE             2
#define LEN_Boot_Signature      1
#define LEN_Volume_ID           4
#define LEN_Volume_Label       11
#define LEN_File_System_Type    8

#pragma pack(push)
struct boot_sector_portions
{
	byte   _JUMP                 [3];
	byte   OEM_name              [8];
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
	byte   Volume_Label         [11];
	byte   File_System_Type      [8];
};
#pragma pack(pop)

typedef struct boot_sector_t
{
	union
	{
		byte _[62];
		struct boot_sector_portions data;
	};
} boot_sector;
