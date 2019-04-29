/*
 * Headers for port/dirent.c, win32 native implementation of dirent functions
 *
 * src/include/port/win32_msvc/dirent.h
 */

#ifndef _WIN32VC_DIRENT_H
#define _WIN32VC_DIRENT_H
#include "pg_config_manual.h"
struct dirent
{
	long		d_ino;
	unsigned short d_reclen;
	unsigned short d_namlen;
	char		d_name[MAXPGPATH];
};

typedef struct DIR DIR;

DIR		   *opendir(const char *);
struct dirent *readdir(DIR *);
int			closedir(DIR *);

#endif
