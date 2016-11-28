#pragma once

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "packed_types.h"

#ifndef VALOF
	#define VALOF(X) STR(X)		  
#endif // !VALOF

#ifndef STR
	#define STR(X) #X			  
#endif // !STR

#define DATE_DAY_MASK     0x001F
#define DATE_DAY_OFFSET   0
#define DATE_MONTH_MASK   0x01E0
#define DATE_MONTH_OFFSET 5
#define DATE_YEAR_OFFSET  9
#define DATE_YEAR_MASK    0xFE00
#define DATE_YEAR_BASE    1980

#define DATE(X) \
	(((X) & DATE_DAY_MASK) >> DATE_DAY_OFFSET), \
	(((X) & DATE_MONTH_MASK) >> DATE_MONTH_OFFSET), \
	((((X) & DATE_YEAR_MASK) >> DATE_YEAR_OFFSET) + DATE_YEAR_BASE)

#define TODATE(D, M, Y) \
	((((D) << DATE_DAY_OFFSET) & DATE_DAY_MASK) | \
	 (((M) << DATE_MONTH_OFFSET) & DATE_MONTH_MASK) | \
	 ((((Y) - DATE_YEAR_BASE) << DATE_YEAR_OFFSET) & DATE_YEAR_MASK))

#define TIME_HOUR_MASK     0xF800
#define TIME_HOUR_OFFSET   11
#define TIME_MINUTE_MASK   0x07E0
#define TIME_MINUTE_OFFSET 5
#define TIME_SECOND_MASK   0x001F
#define TIME_SECOND_OFFSET 0

#define TIME(X) \
	(((X) & TIME_HOUR_MASK) >> TIME_HOUR_OFFSET), \
	(((X) & TIME_MINUTE_MASK) >> TIME_MINUTE_OFFSET)

#define TIME_S(X) \
	(((X) & TIME_HOUR_MASK) >> TIME_HOUR_OFFSET), \
	(((X) & TIME_MINUTE_MASK) >> TIME_MINUTE_OFFSET), \
	(((X) & TIME_SECOND_MASK) >> TIME_SECOND_OFFSET)
	
#define TOTIME(H, M) \
	((((H) << TIME_HOUR_OFFSET) & TIME_HOUR_MASK) | \
	(((M) << TIME_MINUTE_OFFSET) & TIME_MINUTE_MASK) )
	
#define TOTIME_S(H, M, S) \
	((((H) << TIME_HOUR_OFFSET) & TIME_HOUR_MASK) | \
	(((M) << TIME_MINUTE_OFFSET) & TIME_MINUTE_MASK) | \
	(((S) << TIME_SECOND_OFFSET) & TIME_SECOND_MASK))

#define LEN_Filename        8
#define LEN_Extension       3
#define LEN_Directory_Entry 32

#pragma pack(push, 4)
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
		byte raw[LEN_Directory_Entry];
		struct directory_entry_portions data;
	};
} directory_entry;

typedef enum 
{
	READ_ONLY = (1u << 0),
	HIDDEN    = (1u << 1),
	SYSTEM    = (1u << 2),
	VOL_LABEL = (1u << 3),
	SUBDIR    = (1u << 4),
	ARCHIVE   = (1u << 5)
} DIR_ATTR;

static inline directory_entry initialize_write_sector(FILE* file, const char* input_filename)
{
	// Return var
	directory_entry sector_info;

	// Collect some info about the file we're writing
	int file_descriptor = fileno(file);
	struct stat info;
	fstat(file_descriptor, &info);
	
	// Duplicate the filename so we maintain the const specifer
	// in the signature, but let us mess with the contents
	char* filename = strdup(input_filename);
	char* extension = strrchr(filename, '.');
	
	// If this file has an extension, put a terminating char right before it, 
	// this seperates our string into filename\0extension
	if (extension)
	{
		*extension++ = '\0';
	}

	// Preset the whole filename with spaces in our sector
	memset(&sector_info.data.Filename, ' ', LEN_Filename);
	// Overwrite spaces with the actual filename, truncating if necessary
	memcpy(&sector_info.data.Filename, filename, strnlen(filename, LEN_Filename));

	// Same for the extension, preset with spaces for padding
	memset(&sector_info.data.Extension, ' ', LEN_Extension);
	if (extension)
	{
		// Truncate if necessary
		memcpy(&sector_info.data.Extension, extension, strnlen(extension, LEN_Extension));		
	}

	free(filename);

	// These properties will always be 0 for our purposes
	sector_info.data.Attributes.value = 0;
	sector_info.data.Reserved.value = 0;
	sector_info.data.Ignore_in_FAT12.value = 0;

	// Set the First logical sector as sector 1,
	// This corresponds to FAT entry 1 => 0xFFF
	// The caller should update this field when they know the starting
	// FAT entry
	sector_info.data.First_Logical_Cluster.value = 1;
	sector_info.data.File_Size.value = info.st_size;
	
	// Creation (birth) time in UNIX isn't tracked, instead use time of last status change
	struct tm* create_time = localtime(&info.st_ctim.tv_sec);

	sector_info.data.Creation_Time.value = TOTIME_S(create_time->tm_hour, create_time->tm_min, create_time->tm_sec);
	sector_info.data.Creation_Date.value = TODATE(create_time->tm_mday, create_time->tm_mon + 1, create_time->tm_year + 1900);

	// Last access date is right now!
	time_t n; time(&n);
	struct tm* now = localtime(&n);
	
	sector_info.data.Last_Access_Date.value = TODATE(now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);
	sector_info.data.Last_Write_Time.value = TOTIME_S(now->tm_hour, now->tm_min, now->tm_sec);
	sector_info.data.Last_Write_Date.value = sector_info.data.Last_Access_Date.value;

	return sector_info;
}

static inline void trim_filename(char* buff, byte* Filename, byte* Extension)
{
	// Collect and trim padded spaces from the filename
	memcpy(buff, Filename, LEN_Filename);
	int j = LEN_Filename - 1; 
	for (; j > 0; --j)
	{
		if (buff[j] == ' ')
		{
			buff[j] = '\0';
		}
		else break; 
	}
	// Place the extension after the last padded space
	buff[++j] = '.';
	memcpy(&(buff[j + 1]), Extension, LEN_Extension);
				
	// Trim spaces from the extension if any
	j += LEN_Extension; 
	for (; j > 0; --j)
	{
		if (buff[j] == ' ')
		{
			buff[j] = '\0';
		}
		else break; 
	}
}