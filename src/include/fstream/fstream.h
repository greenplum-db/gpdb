#ifndef FSTREAM_H
#define FSTREAM_H

#include <fstream/gfile.h>
#include <sys/types.h>
/*#include "c.h"*/
#ifdef WIN32
typedef __int64 int64_t;
#endif

struct gpfxdist_t;

/* A file stream - data may come from several files */
typedef struct fstream_t fstream_t;

struct fstream_options{
    int header;
    int is_csv;
    int verbose;
    char quote;		/* quote char */
    char escape;	/* escape char */
    int eol_type;
    int bufsize;
    int forwrite;   /* true for write, false for read */
	int usesync;    /* true if writes use O_SYNC */
    struct gpfxdist_t* transform;	/* for gpfxdist transformations */
};

struct fstream_filename_and_offset{
    char 	fname[256];
    int64_t line_number; /* Line number of first line in buffer.  Zero means fstream doesn't know the line number. */
    int64_t foff;
};

// If read_whole_lines, then size must be at least the value of -m (blocksize). */
int fstream_read(fstream_t* fs, void* buffer, int size,
				 struct fstream_filename_and_offset* fo,
				 const int read_whole_lines,
				 const char *line_delim_str,
				 const int line_delim_length);
int fstream_write(fstream_t *fs,
				  void *buf,
				  int size,
				  const int write_whole_lines,
				  const char *line_delim_str,
				  const int line_delim_length);
int fstream_eof(fstream_t* fs);
int64_t fstream_get_compressed_size(fstream_t* fs);
int64_t fstream_get_compressed_position(fstream_t* fs);
const char* fstream_get_error(fstream_t* fs);
fstream_t* fstream_open(const char* path, const struct fstream_options* options,
						int* response_code, const char** response_string);
void fstream_close(fstream_t* fs);
bool_t fstream_is_win_pipe(fstream_t *fs);

#endif
