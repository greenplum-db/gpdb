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

/* The struct gfile_t is private.  Please do not use any of its fields. */
struct gfile_t
{
	ssize_t(*read)(struct gfile_t*,void*,size_t);
	ssize_t(*write)(struct gfile_t*,void*,size_t);
	int(*close)(struct gfile_t*);
	off_t compressed_size,compressed_position;
	bool_t is_win_pipe;
	GFileErrorCode error_code;
	union
	{
		FileAccessInterface *file_access;
#ifdef WIN32
		HANDLE pipefd;
#endif
	} fd;

	union
	{
		int txt;
#ifndef WIN32
		struct zlib_stuff*z;
		struct bzlib_stuff*bz;
#endif
	}u;
	bool_t is_write;
	compression_type compression;

	struct gpfxdist_t* transform;
	GFileAlloca gfile_alloca;
	GFileFree gfile_free;
};

typedef struct FileDescriptorObject
{
	FileAccessInterface file_access;
	int file_descriptor;
} FileDescriptorObject;

static ssize_t
read_with_fd(FileAccessInterface *file_access, void *ptr, size_t size)
{
	assert(file_access);
	assert(CFileObj == file_access->ftype);
	ssize_t i = 0;
	FileDescriptorObject* fobj = (FileDescriptorObject*)file_access;
	do
		i = read(fobj->file_descriptor, ptr, size);
	while (i<0 && errno==EINTR);
	return i;
}

static ssize_t
write_with_fd(FileAccessInterface *file_access, void *ptr, size_t size)
{
	assert(file_access);
	assert(CFileObj == file_access->ftype);
	ssize_t i = 0;
	FileDescriptorObject* fobj = (FileDescriptorObject*)file_access;
	do
		i = write(fobj->file_descriptor, ptr, size);
	while (i<0 && errno==EINTR);

	return i;
}

static int
close_with_fd(FileAccessInterface *file_access)
{
	assert(file_access);
	assert(CFileObj == file_access->ftype);
	FileDescriptorObject* fobj = (FileDescriptorObject*)file_access;
	int ret = 0;
	do
	{
		//fsync(fobj->file_descriptor);
		ret = close(fobj->file_descriptor);
	}
	while (ret < 0 && errno == EINTR);
	return ret;
}

static FileAccessInterface*
gfile_create_CFileObj_descriptor_access(int file_descriptor, GFileAlloca gfile_alloca)
{
	FileDescriptorObject *fobj = gfile_alloca(sizeof(FileDescriptorObject));
	fobj->file_access.ftype = CFileObj;
	fobj->file_access.read_file = read_with_fd;
	fobj->file_access.write_file = write_with_fd;
	fobj->file_access.close_file = close_with_fd;
	fobj->file_descriptor = file_descriptor;
	return &fobj->file_access;
}


static ssize_t
read_and_retry(gfile_t *fd, void *ptr, size_t size)
{
	ssize_t i = fd->fd.file_access->read_file(fd->fd.file_access, ptr, size);
	if (i > 0)
		fd->compressed_position += i;
	if (i < 0)
		fd->error_code = GF_AccessError;
	return i;
}

static ssize_t
write_and_retry(gfile_t *fd, void *ptr, size_t size)
{
	ssize_t i = fd->fd.file_access->write_file(fd->fd.file_access, ptr, size);
	if (i > 0)
		fd->compressed_position += i;
	if (i < 0)
		fd->error_code = GF_AccessError;
	return i;
}

static int
nothing_close(gfile_t *fd)
{
	return 0;
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
	if (i < 0)
		fd->error_code = GF_AccessError;
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
	if (i < 0)
		fd->error_code = GF_AccessError;
	return i;
}

#ifndef WIN32
static void *
bz_alloc(void *a, int b, int c)
{
	gfile_t* fd = (gfile_t*)a;
	return fd->gfile_alloca(b * c);
}

static void
bz_free(void *a,void *b)
{
	gfile_t* fd = (gfile_t*)a;
	fd->gfile_free(b);
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
			{
				fd->error_code = GF_AccessError;
				return -1;
			}
			z->in_size += s;
		}
		
		z->s.avail_in = s = z->in + z->in_size - z->s.next_in;
		z->s.avail_out = sizeof z->out;
		e = BZ2_bzDecompress(&z->s);
		
		if (e == BZ_STREAM_END)
			z->eof = 1;
		else if (e)
		{
			fd->error_code = GF_BZDecompressError;
			return -1;
		}
		
		if (z->s.avail_out == sizeof z->out && z->s.avail_in == s)
		{
			fd->error_code = GF_BZDecompressError;
			return -1;
		}
		
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
	if (e != BZ_OK)
	{
		fd->error_code = GF_BZDecompressError;
	}
	
	fd->gfile_free(fd->u.bz);
	
	return e;
}

static int bz_file_open(gfile_t *fd, FileAccessInterface* file_access)
{
	if (!(fd->u.bz = fd->gfile_alloca(sizeof *fd->u.bz)))
	{
		fd->error_code = GF_OutOfMemory;
		gfile_printf_then_putc_newline("Out of memory");
		return 1;
	}
	
	memset(fd->u.bz, 0, sizeof *fd->u.bz);
	fd->u.bz->s.bzalloc = bz_alloc;
	fd->u.bz->s.bzfree = bz_free;
	fd->u.bz->s.opaque = (void*)fd;
	
	if (BZ2_bzDecompressInit(&fd->u.bz->s, 0, 0))
	{
		fd->error_code = GF_BZDecompressInitError;
		gfile_printf_then_putc_newline("BZ2_bzDecompressInit failed");
		return 1;
	}
	
	fd->u.bz->s.next_out = fd->u.bz->out;
	fd->u.bz->s.next_in = fd->u.bz->in;
	fd->read = bz_file_read;
	fd->close = bz_file_close;
	fd->fd.file_access = file_access;
	
	return 0;
}
#endif

#ifndef WIN32
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
				fd->error_code = GF_AccessError;
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
			{
				fd->error_code = GF_InflateError;
				return -1;
			}
		}
		else if (e)
		{
			fd->error_code = GF_InflateError;
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
		if (e != Z_OK)
		{
			fd->error_code = GF_DeflateError;
		}
	}
	else /* reading, that is inflating */
	{
		e = inflateEnd(&fd->u.z->s);
		if (e != Z_OK)
		{
			fd->error_code = GF_InflateError;
		}
	}
	
	fd->gfile_free(fd->u.z);
	return e;
}

static voidpf
z_alloc(voidpf a, uInt b, uInt c)
{
	gfile_t* fd = (gfile_t*)a;
	return fd->gfile_alloca(b * c);
}

static void z_free(voidpf a, voidpf b)
{
	gfile_t* fd = (gfile_t*)a;
	fd->gfile_free(b);
}

int gz_file_open(gfile_t *fd, FileAccessInterface* file_access)
{
	gfile_reset_error(fd);
	if (!(fd->u.z = fd->gfile_alloca(sizeof *fd->u.z)))
	{
		fd->error_code = GF_OutOfMemory;
		gfile_printf_then_putc_newline("Out of memory");
		return 1;
	}
	
	memset(fd->u.z, 0, sizeof *fd->u.z);
	fd->u.z->s.zalloc = z_alloc;
	fd->u.z->s.zfree = z_free;
	fd->u.z->s.opaque = (void*)fd;

	fd->u.z->s.next_out = fd->u.z->out;
	fd->u.z->s.next_in = fd->u.z->in;
	fd->read = gz_file_read;
	fd->write = gz_file_write;
	fd->close = gz_file_close;
	fd->fd.file_access = file_access;
	
	if ( fd->is_write == FALSE )/* for read */  
	{
		/*
		 * reading a compressed file
		 */		
		if (inflateInit2(&fd->u.z->s,31))
		{
			fd->error_code = GF_InflateInit2Error;
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
			fd->error_code = GF_DeflateInit2Error;
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
	gfile_printf_then_putc_newline(*response_string);
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

static void
gfile_init(gfile_t *gf, GFileAlloca gfile_alloca, GFileFree gfile_free)
{
	assert(gf);
	memset(gf, 0, sizeof*gf);
	gf->gfile_alloca = gfile_alloca;
	gf->gfile_free = gfile_free;
}

gfile_t* gfile_create(compression_type compression, bool_t is_write, GFileAlloca gfile_alloca, GFileFree gfile_free)
{
	assert(gfile_alloca);
	assert(gfile_free);
	gfile_t* gf = gfile_alloca(sizeof(gfile_t));
	gfile_init(gf, gfile_alloca, gfile_free);
	gf->compression = compression;
	gf->is_write = is_write;
	return gf;
}

void gfile_destroy(gfile_t* fd)
{
	GFileFree gfile_free = fd->gfile_free;
	gfile_free(fd);
}

int gfile_open(gfile_t* fd, const char* fpath, int flags, int* response_code, const char** response_string, struct gpfxdist_t* transform)
{
	gfile_reset_error(fd);
	const char* s = strrchr(fpath, '.');
	bool_t is_win_pipe = FALSE;
	int filefd;
#ifndef WIN32
	struct 		stat sta;
#endif
	off_t ssize = 0;

	gfile_init(fd, fd->gfile_alloca, fd->gfile_free);

#ifdef WIN32
#if !defined(S_ISDIR)
#define S_IFDIR  _S_IFDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
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
		do
		{
			int syncFlag = 0;
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
				filefd = open(fpath, O_WRONLY | O_CREAT | O_BINARY | O_APPEND | syncFlag, S_IRUSR | S_IWUSR);
			else
				filefd = open(fpath, O_RDONLY | O_BINARY);
		}
		while (filefd < 0 && errno == EINTR);
	}

	if (!fd->is_win_pipe && -1 == filefd)
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
	 * create fileaccess based on file descriptor.
	 */
	fd->fd.file_access = gfile_create_CFileObj_descriptor_access(filefd, fd->gfile_alloca);
	
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
#ifdef WIN32
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

		return gz_file_open(fd, fd->fd.file_access);
#endif
	}
	else if (s && strcasecmp(s,".bz2")==0)
	{
#ifdef WIN32
		gfile_printf_then_putc_newline(".bz2 not supported");
#else
		fd->compression = BZ_COMPRESSION;
		if (flags != GFILE_OPEN_FOR_READ)
			gfile_printf_then_putc_newline(".bz2 not yet supported for writable tables");

		return bz_file_open(fd, fd->fd.file_access);
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

void
gfile_close(gfile_t*fd)
{
	gfile_reset_error(fd);
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
			fd->fd.file_access->close_file(fd->fd.file_access);
		}

#ifdef GPFXDIST
		} 
#endif

		fd->read = 0;
		fd->close = 0;
		fd->gfile_free((void*)fd->fd.file_access);
		fd->fd.file_access = 0;
	}
	return;
}

ssize_t 
gfile_read(gfile_t *fd, void *ptr, size_t len)
{
	size_t olen = len;
	gfile_reset_error(fd);
	while (len)
	{
		ssize_t i = fd->read(fd, ptr, len);
		assert((i >= 0 && fd->error_code == GF_NoError) ||
				(i < 0 && fd->error_code != GF_NoError));
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
	gfile_reset_error(fd);
	while (len)
	{
		ssize_t i = fd->write(fd, ptr, len);
		assert(i >= 0 && fd->error_code == GF_NoError);
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

bool_t gfile_is_win_pipe(gfile_t* fd)
{
	return fd->is_win_pipe;
}

GFileErrorCode
gfile_error_code(gfile_t *fd)
{
	assert(fd);
	return fd->error_code;
}

void
gfile_reset_error(gfile_t *fd)
{
	assert(fd);
	fd->error_code = GF_NoError;
}

bool_t
gfile_has_error(gfile_t *fd)
{
	return fd->error_code != GF_NoError;
}

char*
gfile_error_detail(gfile_t *fd)
{
	assert(fd);
	if (fd->error_code == GF_NoError ||
		fd->compression != GZ_COMPRESSION)
	{
		return NULL;
	}
	return fd->u.z->s.msg;
}
