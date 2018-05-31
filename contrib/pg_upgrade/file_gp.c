/*
 *	file.c
 *
 *	Greenplum specific file system operations
 *
 *	Portions Copyright (c) 2010, PostgreSQL Global Development Group
 *	Portions Copyright (c) 2016-Present, Pivotal Software Inc
 */

#include "pg_upgrade.h"

#include "storage/bufpage.h"
#include "storage/checksum.h"
#include "storage/checksum_impl.h"

/*
 * In upgrading from GPDB4, copy the pg_distributedlog over in
 * vanilla. The assumption that this works needs to be verified
 */
void
copy_distributedlog(void)
{
	char		old_dlog_path[MAXPGPATH];
	char		new_dlog_path[MAXPGPATH];

	prep_status("Deleting new distributedlog");

	snprintf(old_dlog_path, sizeof(old_dlog_path), "%s/pg_distributedlog", old_cluster.pgdata);
	snprintf(new_dlog_path, sizeof(new_dlog_path), "%s/pg_distributedlog", new_cluster.pgdata);
	if (rmtree(new_dlog_path, true) != true)
		pg_log(PG_FATAL, "Unable to delete directory %s\n", new_dlog_path);
	check_ok();

	prep_status("Copying old distributedlog to new server");
	/* libpgport's copydir() doesn't work in FRONTEND code */
#ifndef WIN32
	exec_prog(true, SYSTEMQUOTE "%s \"%s\" \"%s\"" SYSTEMQUOTE,
			  "cp -Rf",
#else
	/* flags: everything, no confirm, quiet, overwrite read-only */
	exec_prog(true, SYSTEMQUOTE "%s \"%s\" \"%s\\\"" SYSTEMQUOTE,
			  "xcopy /e /y /q /r",
#endif
			  old_dlog_path, new_dlog_path);
	check_ok();
}

/*
 * rewriteHeapPageWithChecksum
 *
 * Copies a relation file from src to dst and sets the data checksum in the
 * page headers in the process. We are not using a pageConverter, even though
 * that would make sense, since pageConverter are deprecated and removed in
 * upstream and would give us merge headaches.
 */
void
rewriteHeapPageChecksum(const char *fromfile, const char *tofile,
						const char *schemaName, const char *relName)
{
	int			src_fd;
	int			dst_fd;
	int			blkno;
	int			bytesRead;
	int			totalBytesRead;
	char	   *buf;
	ssize_t		writesize;
	struct stat statbuf;

	/*
	 * transfer_relfile() should never call us unless requested by the data
	 * checksum option but better doublecheck before we start rewriting data.
	 */
	if (user_opts.checksum_mode == CHECKSUM_NONE)
		pg_log(PG_FATAL, "error, incorrect checksum configuration detected.\n");

	if ((src_fd = open(fromfile, O_RDONLY | PG_BINARY, 0)) < 0)
		pg_log(PG_FATAL,
			   "error while rewriting relation \"%s.%s\": could not open file \"%s\": %s\n",
			   schemaName, relName, fromfile, strerror(errno));

	if (fstat(src_fd, &statbuf) != 0)
		pg_log(PG_FATAL,
			   "error while rewriting relation \"%s.%s\": could not stat file \"%s\": %s\n",
			   schemaName, relName, fromfile, strerror(errno));

	if ((dst_fd = open(tofile, O_RDWR | O_CREAT | O_EXCL | PG_BINARY, S_IRUSR | S_IWUSR)) < 0)
		pg_log(PG_FATAL,
			   "error while rewriting relation \"%s.%s\": could not create file \"%s\": %s\n",
			   schemaName, relName, tofile, strerror(errno));

	blkno = 0;
	totalBytesRead = 0;
	buf = (char *) pg_malloc(BLCKSZ);

	while ((bytesRead = read(src_fd, buf, BLCKSZ)) == BLCKSZ)
	{
		Size		page_size;

		page_size = PageGetPageSize((PageHeader) buf);

		if (!PageSizeIsValid(page_size) && page_size != 0)
			pg_log(PG_FATAL,
				   "error while rewriting relation \"%s.%s\": invalid page size detected (%zd)\n",
				   schemaName, relName, page_size);

		if (!PageIsNew(buf))
		{
			if (user_opts.checksum_mode == CHECKSUM_ADD)
				((PageHeader) buf)->pd_checksum = pg_checksum_page(buf, blkno);
			else
				memset(&(((PageHeader) buf)->pd_checksum), 0, sizeof(uint16));
		}

		writesize = write(dst_fd, buf, BLCKSZ);

		if (writesize != BLCKSZ)
			pg_log(PG_FATAL, "error when rewriting relation \"%s.%s\": %s",
				   schemaName, relName, strerror(errno));

		blkno++;
		totalBytesRead += BLCKSZ;
	}

	if (totalBytesRead != statbuf.st_size)
		pg_log(PG_FATAL,
			   "error when rewriting relation \"%s.%s\": torn read on file \"%s\"%c %s\n",
			   schemaName, relName, fromfile,
			   (errno != 0 ? ':' : ' '), strerror(errno));

	pg_free(buf);
	close(dst_fd);
	close(src_fd);
}
