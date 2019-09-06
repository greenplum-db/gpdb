#include "c.h"


#ifndef FRONTEND
#include "storage/fd.h"
#endif

#ifdef WIN32
/* exclude transformation features on windows for now */
#undef GPFXDIST
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <fstream/gfile.h>
#ifdef GPFXDIST
#include <gpfxdist.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>   /* for flock */
#include <unistd.h>

#ifdef WIN32
#include <io.h>
#define snprintf _snprintf
#else
#define O_BINARY 0
#endif

#ifndef S_IRUSR					/* XXX [TRH] should be in a header */
#define S_IRUSR		 S_IREAD
#define S_IWUSR		 S_IWRITE
#define S_IXUSR		 S_IEXEC
#endif 

#define COMPRESSION_BUFFER_SIZE		(1<<14)


static int
nothing_close(gfile_t *fd)
{
	return 0;
}

static ssize_t
read_and_retry(gfile_t *fd, void *ptr, size_t size)
{
	ssize_t i = 0;

	do
		i = read(fd->fd.filefd, ptr, size);
	while (i<0 && errno==EINTR);

	if (i > 0)
		fd->compressed_position += i;
	return i;
}

static ssize_t
write_and_retry(gfile_t *fd, void *ptr, size_t size)
{
	ssize_t i = 0;

	do
		i = write(fd->fd.filefd, ptr, size);
	while (i<0 && errno==EINTR);

	if (i > 0)
		fd->compressed_position += i;
	return i;
}

static int
closewinpipe(gfile_t*fd)
{
	assert(fd->is_win_pipe);
#ifdef WIN32
	CloseHandle(fd->fd.pipefd);
#endif
	return 0;
}

static ssize_t
readwinpipe(gfile_t* fd, void* ptr, size_t size)
{
	long i = 0;
	
	assert(fd->is_win_pipe);
#ifdef WIN32
	do
		ReadFile(fd->fd.pipefd, ptr, size, (PDWORD)&i, NULL);
	while (i < 0 && errno == EINTR);
#endif

	if (i > 0)
		fd->compressed_position += i;

	return i;
}

static ssize_t
writewinpipe(gfile_t* fd, void* ptr, size_t size)
{
	long i = 0;

	assert(fd->is_win_pipe);
#ifdef WIN32
	do
		WriteFile(fd->fd.pipefd, ptr, size, (PDWORD)&i, NULL);
	while (i < 0 && errno == EINTR);
#endif
	if (i > 0)
		fd->compressed_position += i;

	return i;
}

#ifdef HAVE_LIBBZ2
static void *
bz_alloc(void *a, int b, int c)
{
	return gfile_malloc(b * c);
}

static void
bz_free(void *a,void *b)
{
	gfile_free(b);
}

struct bzlib_stuff
{
	bz_stream s;
	int in_size, out_size, eof;
	char in[COMPRESSION_BUFFER_SIZE];
	char out[COMPRESSION_BUFFER_SIZE];
};

static ssize_t 
bz_file_read(gfile_t *fd, void *ptr, size_t len)
{
	struct bzlib_stuff *z = fd->u.bz;
	
	for (;;)
	{
		int e;
		int s = z->s.next_out - z->out - z->out_size;
		
		if (s > 0 || z->eof)
		{
			if (s > len)
				s = len;
			memcpy(ptr, z->out + z->out_size, s);
			z->out_size += s;
			
			return s;
		}
		
		z->out_size = 0;
		z->s.next_out = z->out;
		
		while (z->in_size < sizeof z->in)
		{
			s = read_and_retry(fd, z->in + z->in_size, sizeof z->in
					- z->in_size);
			if (s == 0)
				break;
			if (s < 0)
				return -1;
			z->in_size += s;
		}
		
		z->s.avail_in = s = z->in + z->in_size - z->s.next_in;
		z->s.avail_out = sizeof z->out;
		e = BZ2_bzDecompress(&z->s);
		
		if (e == BZ_STREAM_END)
			z->eof = 1;
		else if (e)
			return -1;
		
		if (z->s.avail_out == sizeof z->out && z->s.avail_in == s)
			return -1;
		
		if (z->s.next_in == z->in + z->in_size)
		{
			z->s.next_in = z->in;
			z->in_size = 0;
		}
	}
}

static int
bz_file_close(gfile_t *fd)
{
	int e = BZ2_bzDecompressEnd(&fd->u.bz->s);
	
	gfile_free(fd->u.bz);
	
	return e;
}

static int bz_file_open(gfile_t *fd)
{
	if (!(fd->u.bz = gfile_malloc(sizeof *fd->u.bz)))
	{
		gfile_printf_then_putc_newline("Out of memory");
		return 1;
	}
	
	memset(fd->u.bz, 0, sizeof *fd->u.bz);
	fd->u.bz->s.bzalloc = bz_alloc;
	fd->u.bz->s.bzfree = bz_free;
	
	if (BZ2_bzDecompressInit(&fd->u.bz->s, 0, 0))
	{
		gfile_printf_then_putc_newline("BZ2_bzDecompressInit failed");
		return 1;
	}
	
	fd->u.bz->s.next_out = fd->u.bz->out;
	fd->u.bz->s.next_in = fd->u.bz->in;
	fd->read = bz_file_read;
	fd->close = bz_file_close;
	
	return 0;
}
#endif

#ifdef HAVE_LIBZ
/* GZ */
struct zlib_stuff
{
	z_stream s;
	int in_size, out_size, eof;
	Byte in[COMPRESSION_BUFFER_SIZE];
	Byte out[COMPRESSION_BUFFER_SIZE];
};

static ssize_t
gz_file_read(gfile_t* fd, void* ptr, size_t len)
{
	struct zlib_stuff* z = fd->u.z;
	
	for (;;)
	{
		int	e;
		int	flush = Z_NO_FLUSH;
		
		/*
		 * 'out' is our output buffer.
		 * 'next_out' is a pointer to the next byte in 'out'
		 * 'out_size' is num bytes currently in 'out'
		 * 
		 * if s is >0 we have data in 'out' that we didn't write
		 * yet, write it and return.  
		 */
		int s = z->s.next_out - (z->out + z->out_size);
		
		if (s > 0 || z->eof)
		{
			if (s > len)
				s = len;
			memcpy(ptr, z->out + z->out_size, s);
			z->out_size += s;
			return s;
		}
		
		/* ok, wrote all 'out' data. reset back to beginning of 'out' */
		z->out_size = 0;
		z->s.next_out = z->out;
		
		/*
		 * Fill up our input buffer from the input file.
		 */
		while (z->in_size < sizeof z->in)
		{
			s = read_and_retry(fd, z->in + z->in_size, sizeof z->in - z->in_size);
			
			if (s == 0)
			{
				/* no more data to read */
				
				if (z->in + z->in_size == z->s.next_in)
					flush = Z_FINISH;
				break;
			}
			if (s < 0)
			{
				/* read error */
				return -1;
			}
				
			z->in_size += s;
		}
		
		/* number of bytes available at next_in */
		z->s.avail_in = (z->in + z->in_size) - z->s.next_in;
		
		/* remaining free space at next_out */ 
		z->s.avail_out = sizeof z->out;
		
		/* decompress */ 
		e = inflate(&z->s, flush);
		
		if (e == Z_STREAM_END && z->s.avail_in == 0)
		{
			/* we're done decompressing all we have */
			z->eof = 1;
		}
		else if(e == Z_STREAM_END && z->s.avail_in > 0)
		{
			/* 
			 * we're done decompressing a chunk, but there's more
			 * input. we need to reset state. see MPP-8012 for info 
			 */
			if(inflateReset(&z->s))
				return -1;
		}
		else if (e)
		{
			return -1;			
		}
		
		/* if no more data available for decompression reset input buf */
		if (z->s.next_in == (z->in + z->in_size))
		{
			z->s.next_in = z->in;
			z->in_size = 0;
		}
	}
}

static int 
gz_file_write_one_chunk(gfile_t *fd, int do_flush)
{
	/*
	 * 0 - means we are ok
	 */
	int ret = 0, have;
	struct zlib_stuff* z = fd->u.z;
	
	do 
	{
		int ret1;
		
		z->s.avail_out = COMPRESSION_BUFFER_SIZE;
		z->s.next_out = z->out;
		ret1 = deflate(&(z->s), do_flush);    /* no bad return value */
		assert(ret1 != Z_STREAM_ERROR);  /* state not clobbered */
		have = COMPRESSION_BUFFER_SIZE - z->s.avail_out;
		
		if ( write_and_retry(fd, z->out, have) != have ) 
		{
			/*
			 * presently gfile_close calls gz_file_close only for the on_write case so we don't need
			 * to handle inflateEnd here
			 */
			(void)deflateEnd(&(z->s));
			ret = -1;
			break;
		}
		
	} while (COMPRESSION_BUFFER_SIZE == have);	
	/*
	 * if the deflate engine filled all the output buffer, it may have more data, so we must try again
	 */
	
	return ret;
}

static ssize_t
gz_file_write(gfile_t *fd, void *ptr, size_t size)
{
	int ret;
	
	
	size_t left_to_compress = size;
	size_t one_iter_compress;
	struct zlib_stuff* z = fd->u.z;
		
	do
	{
		/*
		 * we do not wish that the size of the input buffer to  the deflate engine, will be greater
		 * than the recomended COMPRESSION_BUFFER_SIZE.
		 */
		one_iter_compress = (left_to_compress > COMPRESSION_BUFFER_SIZE) ? COMPRESSION_BUFFER_SIZE : left_to_compress;
			
		z->s.avail_in = one_iter_compress;
		z->s.next_in = (Byte*)((Byte*)ptr + (size - left_to_compress));
		
		ret = gz_file_write_one_chunk(fd, Z_NO_FLUSH);
		if (0 != ret)
		{
			return ret;
		}
				
		left_to_compress -= one_iter_compress; 
	} while( left_to_compress > 0 );

		
	return size;
}

static int
gz_file_close(gfile_t *fd)
{
	int e = 0;
	
	if ( fd->is_write == TRUE ) /* writing, or in other words compressing */
	{
		e = gz_file_write_one_chunk(fd, Z_FINISH);
		if (0 != e)
		{
			return e;
		}

		e = deflateEnd(&fd->u.z->s);
	}
	else /* reading, that is inflating */
	{
		e = inflateEnd(&fd->u.z->s);
	}
	
	gfile_free(fd->u.z);
	return e;
}

static voidpf
z_alloc(voidpf a, uInt b, uInt c)
{
	return gfile_malloc(b * c);
}

static void z_free(voidpf a, voidpf b)
{
	gfile_free(b);
}

static int
gz_file_open(gfile_t *fd)
{
	if (!(fd->u.z = gfile_malloc(sizeof *fd->u.z)))
	{
		gfile_printf_then_putc_newline("Out of memory");
		return 1;
	}
	
	memset(fd->u.z, 0, sizeof *fd->u.z);
	fd->u.z->s.zalloc = z_alloc;
	fd->u.z->s.zfree = z_free;
	fd->u.z->s.opaque = Z_NULL;

	fd->u.z->s.next_out = fd->u.z->out;
	fd->u.z->s.next_in = fd->u.z->in;
	fd->read = gz_file_read;
	fd->write = gz_file_write;
	fd->close = gz_file_close;
	
	if ( fd->is_write == FALSE )/* for read */  
	{
		/*
		 * reading a compressed file
		 */		
		if (inflateInit2(&fd->u.z->s,31))
		{
			gfile_printf_then_putc_newline("inflateInit2 failed");
			return 1;
		}
	}
	else 
	{
		/*
		 * writing a compressed file
		 */
		if ( Z_OK !=
			 deflateInit2(&fd->u.z->s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) )
		{
			gfile_printf_then_putc_newline("deflateInit2 failed");
			return 1;			
		}
		 
	}

	return 0;
}
#endif

#ifdef GPFXDIST
/*
 * subprocess support
 */

static void
subprocess_open_errfn(apr_pool_t *pool, apr_status_t status, const char *desc)
{
	char errbuf[256];
	fprintf(stderr, "subprocess: %s: %s\n", desc, apr_strerror(status, errbuf, sizeof(errbuf)));
}

static int 
subprocess_open_failed(int* response_code, const char** response_string, char* reason)
{
	*response_code   = 500;
	*response_string = reason;
	gfile_printf_then_putc_newline("%s", *response_string);
	return 1;
}

static int 
subprocess_open(gfile_t* fd, const char* fpath, int for_write, int* rcode, const char** rstring)
{
	apr_pool_t*     mp     = fd->transform->mp;
	char*           cmd    = fd->transform->cmd;
	apr_procattr_t* pattr;
	char**          tokens;
	apr_status_t    rv;
	apr_file_t*     errfile;

	/* tokenize command string */
	if ((rv = apr_tokenize_to_argv(cmd, &tokens, mp)) != APR_SUCCESS) 
	{
		return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_tokenize_to_argv failed");
	}

	/* replace %FILENAME% with path to input or output file */
	{
		char** p;
		for (p = tokens; *p; p++) 
		{
			if (0 == strcasecmp(*p, "%FILENAME%"))
				*p = (char*) fpath;
		}
	}

	/* setup apr subprocess attribute structure */
	if ((rv = apr_procattr_create(&pattr, mp)) != APR_SUCCESS) 
	{
		return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_create failed");
	}

	/* setup child stdin/stdout depending on the direction of transformation */
	if (for_write) 
	{
		/* writable external table, so child will be reading from standard input */

		if ((rv = apr_procattr_io_set(pattr, APR_FULL_BLOCK, APR_NO_PIPE, APR_NO_PIPE)) != APR_SUCCESS) 
		{
			return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_io_set (full,no,no) failed");
		}
		fd->transform->for_write = 1;
	} 
	else 
	{
		/* readable external table, so child will be writing to standard output */

		if ((rv = apr_procattr_io_set(pattr, APR_NO_PIPE, APR_FULL_BLOCK, APR_NO_PIPE)) != APR_SUCCESS) 
		{
			return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_io_set (no,full,no) failed");
		}
		fd->transform->for_write = 0;
	}

	/* setup child stderr */
	if (fd->transform->errfile)
	{
		/* redirect stderr to a file to be sent to server when we're finished */

		errfile = fd->transform->errfile;

		if ((rv = apr_procattr_child_err_set(pattr, errfile, NULL)) !=  APR_SUCCESS)
		{
			return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_child_err_set failed");
		}
	} 

	/* more APR complexity: setup error handler for when child doesn't spawn properly */
	if ((rv = apr_procattr_child_errfn_set(pattr, subprocess_open_errfn)) != APR_SUCCESS) 
	{
		return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_child_errfn_set failed");
	}

	/* don't run the child via an operating system shell */
	if ((rv = apr_procattr_cmdtype_set(pattr, APR_PROGRAM_ENV)) != APR_SUCCESS) 
	{
		return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_procattr_cmdtype_set failed");
	}

	/* finally... start the child process */
	if ((rv = apr_proc_create(&fd->transform->proc, tokens[0], (const char* const*)tokens, NULL, pattr, mp)) != APR_SUCCESS) 
	{
		return subprocess_open_failed(rcode, rstring, "subprocess_open: apr_proc_create failed");
	}

	return 0;
}


static ssize_t 
read_subprocess(gfile_t *fd, void *ptr, size_t len)
{
	apr_size_t      nbytes = len;
	apr_status_t    rv;

	rv = apr_file_read(fd->transform->proc.out, ptr, &nbytes);
	if (rv == APR_SUCCESS)
		return nbytes;
	
	if (rv == APR_EOF)
		return 0;

	return -1;
}

static ssize_t
write_subprocess(gfile_t *fd, void *ptr, size_t size)
{
	apr_size_t      nbytes = size;

	apr_file_write(fd->transform->proc.in, ptr, &nbytes);
	return nbytes;
}

static int
close_subprocess(gfile_t *fd)
{
	int             st;
	apr_exit_why_e  why;
	apr_status_t    rv;
    
	if (fd->transform->for_write)
	    apr_file_close(fd->transform->proc.in);
	else
	    apr_file_close(fd->transform->proc.out);
        
	rv = apr_proc_wait(&fd->transform->proc, &st, &why, APR_WAIT);
	if (APR_STATUS_IS_CHILD_DONE(rv)) 
	{
		gfile_printf_then_putc_newline("close_subprocess: done: why = %d, exit status = %d", why, st);
		return st;
	} 
	else 
	{
		gfile_printf_then_putc_newline("close_subprocess: notdone");
		return 1;
	}
}
#endif


/*
 * public interface
 */

int 
gfile_open_flags(int writing, int usesync)
{
	if (writing)
	{
		if (usesync)
			return GFILE_OPEN_FOR_WRITE_SYNC;
		else
			return GFILE_OPEN_FOR_WRITE_NOSYNC;
	}
	return GFILE_OPEN_FOR_READ;
}


int gfile_open(gfile_t* fd, const char* fpath, int flags, int* response_code, const char** response_string, struct gpfxdist_t* transform)
{
	const char* s = strrchr(fpath, '.');
	bool_t is_win_pipe = FALSE;
#ifndef WIN32
	struct 		stat sta;
#endif
	off_t ssize = 0;

	memset(fd,0,sizeof*fd);

#ifdef WIN32
#if !defined(S_ISDIR)
#define S_IFDIR  _S_IFDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISFIFO)
#define S_IFIFO _S_IFIFO
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#define strcasecmp stricmp
#endif 


	/*
	 * check for subprocess and/or named pipe
	 */

#ifdef GPFXDIST
	fd->transform = transform;

	if (fd->transform)
	{
		/* caller wants a subprocess. nothing to do here just yet. */
		gfile_printf_then_putc_newline("looks like a subprocess");
	}
	else 
#endif
#ifdef WIN32
	/* is this a windows named pipe, of the form \\<host>\... */
	if (strlen(fpath) > 2)
	{
		if (fpath[0] == '\\' && fpath[1] == '\\')
		{
			is_win_pipe = TRUE;
			gfile_printf_then_putc_newline("looks like a windows pipe");
		}
	}

	if (is_win_pipe)
	{
		/* Try and open it as a windows named pipe */
		HANDLE pipe = CreateFile(fpath, 
								 (flags != GFILE_OPEN_FOR_READ ? GENERIC_WRITE : GENERIC_READ),
								 0, /* no sharing */
								 NULL, /* default security */
								 OPEN_EXISTING, /* file must exist */
								 0, /* default attributes */
								 NULL /* no template */);
		gfile_printf_then_putc_newline("trying to connect to pipe");
		if (pipe != INVALID_HANDLE_VALUE)
		{
			is_win_pipe = TRUE;
			fd->is_win_pipe = TRUE;
			fd->fd.pipefd = pipe;
			gfile_printf_then_putc_newline("connected to pipe");
		}
		else
		{
			LPSTR msg;

			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						  FORMAT_MESSAGE_FROM_SYSTEM,
				   		  NULL, GetLastError(),
						  MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
						  (LPSTR) & msg, 0, NULL);
			gfile_printf_then_putc_newline("could not create pipe: %s", msg);

			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				*response_code = 500;
				*response_string = "could not connect to pipe";
			}
			else
			{
				*response_code = 501;
				*response_string = "pipe is busy, close the pipe and try again";
			}
			return 1;
		}
	}

#else

	if (!is_win_pipe && (flags == GFILE_OPEN_FOR_READ))
	{
		if (stat(fpath, &sta))
		{
			if(errno == EOVERFLOW)
			{
				/*
				 * ENGINF-176
				 * 
				 * Some platforms don't support stat'ing of "large files"
				 * accurately (files over 2GB) - SPARC for example. In these
				 * cases the storage size of st_size is too small and the
				 * file size will overflow. Therefore, we look for cases where
				 * overflow had occurred, and resume operation. At least we
				 * know that the file does exist and that's the main goal of
				 * stat'ing here anyway. we set the size to 0, similarly to
				 * the winpipe path, so that negative sizes won't be used.
				 * 
				 * TODO: there may be side effects to setting the size to 0,
				 * need to double check.
				 * 
				 * TODO: this hack could possibly now be removed after enabling
				 * largefiles via the build process with compiler flags.
				 */
				sta.st_size = 0;
			}
			else
			{
				gfile_printf_then_putc_newline("gfile stat %s failure: %s", fpath, strerror(errno));
				*response_code = 404;
				*response_string = "file not found";
				return 1;				
			}
		}
		if (S_ISDIR(sta.st_mode))
		{
			gfile_printf_then_putc_newline("gfile %s is a directory", fpath);
			*response_code = 403;
			*response_string = "Reading a directory is forbidden.";
			return 1;
		}
		ssize = sta.st_size;
	}

#endif

	fd->compressed_size = ssize;

#ifdef GPFXDIST
	if (fd->transform)
	{
		/* CR-2173 nothing to do here if transformation is requested */
	}
	else
#endif
	if (!fd->is_win_pipe)
	{
		int syncFlag = 0;
		int openFlags;
		mode_t openMode;

#ifndef WIN32
		/*
		 * MPP-13817 (support opening files without O_SYNC)
		 */
		if (flags & GFILE_OPEN_FOR_WRITE_SYNC)
		{
			/*
			 * caller explicitly requested O_SYNC
			 */
			syncFlag = O_SYNC;
		}
		else if ((stat(fpath, &sta) == 0) && S_ISFIFO(sta.st_mode))
		{
			/*
			 * use O_SYNC since we're writing to another process via a pipe
			 */
			syncFlag = O_SYNC;
		}
#endif
		if (flags != GFILE_OPEN_FOR_READ)
		{
			openFlags = O_WRONLY | O_CREAT | O_BINARY | O_APPEND | syncFlag;
			openMode = S_IRUSR | S_IWUSR;
		}
		else
		{
			openFlags = O_RDONLY | O_BINARY;
			openMode = 0;
		}

		do
		{
#ifdef FRONTEND
			fd->fd.filefd = open(fpath, openFlags, openMode);
#else
			fd->fd.filefd = OpenTransientFile((char *) fpath, openFlags, openMode);
#endif
		}
		while (fd->fd.filefd < 0 && errno == EINTR);
	}

	if (!fd->is_win_pipe && -1 == fd->fd.filefd) 
	{
		static char buf[256];
		gfile_printf_then_putc_newline("gfile open (for %s) failed %s: %s",
									   ((flags == GFILE_OPEN_FOR_READ) ? "read" : 
										((flags == GFILE_OPEN_FOR_WRITE_SYNC) ? "write (sync)" : "write")),
					  				  fpath, strerror(errno));
		*response_code = 404;
		snprintf(buf, sizeof buf, "file open failure %s: %s", fpath, 
				strerror(errno));
		*response_string = buf;
		return 1;
	}

#if !defined(WIN32) && !defined(_AIX)
	if (!is_win_pipe && (flags == GFILE_OPEN_FOR_READ))
	{
		/* Restrict only one reader session for each PIPE */
		if (S_ISFIFO(sta.st_mode))
		{
			if (flock (fd->fd.filefd, LOCK_EX | LOCK_NB) != 0)
			{
				fd->held_pipe_lock = FALSE;
				gfile_printf_then_putc_newline("gfile %s is a pipe", fpath);
				*response_code = 404;
				*response_string = "Multiple reader to a pipe is forbidden.";
				return 1;
			}
			else
			{
				fd->held_pipe_lock = TRUE;
			}
		}
	}
#endif

	/*
	 * prepare to use the appropriate i/o routines 
	 */

#ifdef GPFXDIST
	if (fd->transform)
	{
		fd->read  = read_subprocess;
		fd->write = write_subprocess;
		fd->close = close_subprocess;
	}
	else 
#endif
	if (fd->is_win_pipe)
	{
		fd->read = readwinpipe;
		fd->write = writewinpipe;
		fd->close = closewinpipe;
	}
	else
	{
		fd->read = read_and_retry;
		fd->write = write_and_retry;
		fd->close = nothing_close;
	}

	
	/*
	 * delegate remaining setup work to an appropriate open routine
	 * or return an error if we can't handle the type
	 */

#ifdef GPFXDIST
	if (fd->transform)
	{
		return subprocess_open(fd, fpath, (flags != GFILE_OPEN_FOR_READ), response_code, response_string);
	}
	else 
#endif
	if (s && strcasecmp(s,".gz")==0)
	{
#ifndef HAVE_LIBZ
		gfile_printf_then_putc_newline(".gz not supported");
#else
		/*
		 * flag used by function gfile close
		 */
		fd->compression = GZ_COMPRESSION;
		
		if (flags != GFILE_OPEN_FOR_READ)
		{
			fd->is_write = TRUE;
		}

		return gz_file_open(fd);
#endif
	}
	else if (s && strcasecmp(s,".bz2")==0)
	{
#ifndef HAVE_LIBBZ2
		gfile_printf_then_putc_newline(".bz2 not supported");
#else
		fd->compression = BZ_COMPRESSION;
		if (flags != GFILE_OPEN_FOR_READ)
			gfile_printf_then_putc_newline(".bz2 not yet supported for writable tables");

		return bz_file_open(fd);
#endif
	}
	else if (s && strcasecmp(s,".z") == 0)
		gfile_printf_then_putc_newline("gfile compression .z file is not supported");
	else if (s && strcasecmp(s,".zip") == 0)
		gfile_printf_then_putc_newline("gfile compression zip is not supported");
	else
		return 0;

	*response_code = 415;
	*response_string = "Unsupported File Type";

	return 1;
}

int
gfile_close(gfile_t*fd)
{
	int ret = 1;

	if (fd->close)
	{
#ifdef GPFXDIST
		if (fd->transform)
        {
			fd->close(fd);
		} 
        else
		{
#endif

		/*
		 * for the compressed data implementation we need to call the "close" callback. Other implementations
		 * didn't use to call this callback here and it will remain so.
		 */
		if (  fd->compression == GZ_COMPRESSION ) 
		{
			fd->close(fd);
		}

		if (fd->is_win_pipe)
		{
			fd->close(fd);
		}
		else
		{
			if(fd->held_pipe_lock)
			{
#ifndef WIN32
				flock (fd->fd.filefd, LOCK_UN);
#endif
			}
			do
			{
				//fsync(fd->fd.filefd);
#ifdef FRONTEND
				ret = close(fd->fd.filefd);
#else
				ret = CloseTransientFile(fd->fd.filefd);
#endif
			}
			while (ret < 0 && errno == EINTR);

			if (ret == -1)
				ret = 1;
		}

#ifdef GPFXDIST
		} 
#endif

		fd->read = 0;
		fd->close = 0;
	}
	return ret;
}

ssize_t 
gfile_read(gfile_t *fd, void *ptr, size_t len)
{
	size_t olen = len;
	
	while (len)
	{
		ssize_t i = fd->read(fd, ptr, len);
		if (i < 0)
			return i;
		if (i == 0)
			break;
		ptr = (char*) ptr + i;
		len -= i;
	}
	
	return olen - len;
}

ssize_t 
gfile_write(gfile_t *fd, void *ptr, size_t len)
{
	size_t olen = len;
	
	while (len)
	{
		ssize_t i = fd->write(fd, ptr, len);
				
		if (i < 0)
			return i;
		if (i == 0)
			break;
		
		ptr = (char*) ptr + i;
		len -= i;
	}
	
	return olen - len;
}

off_t gfile_get_compressed_size(gfile_t *fd)
{
	return fd->compressed_size;
}

off_t gfile_get_compressed_position(gfile_t *fd)
{
	return fd->compressed_position;
}
