#pragma once

#include "packed_types.h"

#ifndef VALOF
	#define VALOF(X) STR(X)		  
#endif // !VALOF

#ifndef STR
	#define STR(X) #X			  
#endif // !STR

#define LEN_Filename 8
#define LEN_Extension 3


#pragma pack(push)
struct directory_entry_portions
{
	byte Filename[LEN_Filename];
	byte Extension[LEN_Extension];
	byte Attributes;
	word Reserved;
	word Creation_Time;
	word Creation_Date;
	word Last_Access_Date;
	word Ignore_in_FAT12;
	word Last_Write_Time;
	word Last_Write_Date;
	word First_Logical_Cluster;
	dword File_Size;
};
#pragma pack(pop)

typedef struct 
{
	union
	{
		byte raw[32];
		struct directory_entry_portions data;
	};
} directory_entry;

typedef enum 
{
	READ_ONLY = 0x01,
	HIDDEN    = 0x02,
	SYSTEM    = 0x04,
	VOL_LABEL = 0x08,
	SUBDIR    = 0x10,
	ARCHIVE   = 0x20
} DIR_ATTR;