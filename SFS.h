#pragma once

#define MIN(X, Y) (X > Y ? Y : X)

typedef enum
{
	DISKINFO,
	DISKLIST,
	DISKGET,
	DISKPUT,
	DISK_ACTION_NONE = -1
} DISK_ACTION;

void usage(DISK_ACTION action);

void quit(const char* reason);

DISK_ACTION checkProgram(const char* executed_name);
