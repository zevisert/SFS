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

