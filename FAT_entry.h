#pragma once

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
