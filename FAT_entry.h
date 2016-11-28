#pragma once

#include "packed_types.h"

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

static inline void load_FAT_entry(FAT_entry* table, const byte* disk, unsigned int FAT_offset, int entry)
{
	// Grab the two bytes that support this FAT entry
	byte a = disk[FAT_offset + 3 * entry / 2];
	byte b = disk[FAT_offset + 3 * entry / 2 + 1];
		
	// If the entry is even...
	if (entry % 2 == 0)
	{	
		// ... use the low order bits from the higher location
		table[entry].L = b.L;
		// And the whole byte in the base location
		table[entry].M = a.H;
		table[entry].H = a.L;
	}
	else
	{
		// ... use the whole byte from the high location
		table[entry].L = b.H;
		table[entry].M = b.L;
		// and the high order bits from the lower location
		table[entry].H = a.H;
	}
}

static inline void update_disk_FAT(FAT_entry* table, byte* disk, unsigned int FAT_offset, int entry)
{
	// Update the FAT entries on disk
	byte* a = &disk[FAT_offset + 3 * entry / 2];
	byte* b = &disk[FAT_offset + 3 * entry / 2 + 1];
		
	// If the entry is even...
	if (entry % 2 == 0)
	{	
		// ... use the low order bits from the higher location
		b->L = table[entry].L;
		// And the whole byte in the base location
		a->H = table[entry].M;
		a->L = table[entry].H;
	}
	else
	{
		// ... use the whole byte from the high location
		b->H = table[entry].L;
		b->L = table[entry].M;
		// and the high order bits from the lower location
		a->H = table[entry].H;
	}

}
