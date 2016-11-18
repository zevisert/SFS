#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ASSERT

#ifdef ASSERT
	#include <assert.h>
#else
	#define assert(X) { /*nothing */ }
#endif

typedef enum {
	DISKINFO,
	DISKLIST,
	DISKGET,
	DISKPUT,
	DISK_ACTION_NONE = -1
} DISK_ACTION;

DISK_ACTION checkProgram(const char* executed_name)
{
	char* input = strdup(executed_name);
	char* prog_name = strrchr(input, '/');
	DISK_ACTION result = DISK_ACTION_NONE;
	
	// strrchr found the last forward slash
	// we want just he program name;
	++prog_name;

	if      (strcasecmp(prog_name, "diskinfo") == 0)
		result = DISKINFO;
	else if (strcasecmp(prog_name, "disklist") == 0)
		result = DISKLIST;
	else if (strcasecmp(prog_name, "diskget" ) == 0)
		result = DISKGET;
	else if (strcasecmp(prog_name, "diskput" ) == 0)
		result = DISKPUT;

	free(input);

	return result;
}


void quit(char* reason)
{
	if (reason != NULL)
		fprintf(stderr, "%s", reason);

	fprintf(stderr, "Exiting...\n");

	exit(EXIT_FAILURE);
} 

void usage(DISK_ACTION action)
{
	printf("Simple File System Usage:\n");
	
	switch (action) 
	{
		case DISK_ACTION_NONE:
		{
			printf("  This program suite must be executed under one of the following names:\n");
			printf("    [ ./diskinfo | ./disklist | ./diskget | ./diskput ]\n");
		}
		break;

		case DISKINFO:
		{
			printf(" diskinfo <disk>\n");
			printf("    Processes the <disk> image and displays some basic information about the image.\n");
		}
		break;

		case DISKLIST:
		{
			printf(" disklist <disk>\n");
			printf("    Displays contents of the root directory of the <disk> image.\n");
		} 
		break;

		case DISKGET:
		{
			printf("  diskget <disk> <filename>\n");
			printf("    Retrieves <filename> from the <disk> image and places it in the current working directory\n");
		}
		break;
	
		case DISKPUT:
		{
			printf(" diskput <disk> <file>\n");
			printf("    Writes a copy of <file> to the root of <disk> if enough space is available\n");
		}
		break;
	}
	printf("\n");
}


int main(int argc, char** argv)
{

	for(int i = 0; i < argc; ++i)
		printf(">>> %s\n", argv[i]);

	DISK_ACTION run_prog = checkProgram(argv[0]);

	usage(run_prog);
	
	return EXIT_SUCCESS;

}
