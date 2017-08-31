/*-------------------------------------------------------------------------
 *
 * cdbselect.h
 *
 *	  Provides equivalents to FD_SET/FD_ZERO -- but handle more than
 *  1024 file descriptors (as far as I can tell all of our platforms
 *  support 65536).
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbselect.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBSELECT_H
#define CDBSELECT_H

#if !defined(_WIN32)

/* 8-bit bytes 32 or 64-bit ints */

typedef struct
{
 	int32 __fds_bits[65536/(sizeof(int32) * 8)];
} mpp_fd_set;

#define MPP_FD_ZERO(setp) (memset((char *)(setp), 0, sizeof(mpp_fd_set)))

#define MPP_FD_WORD(fd) ((fd) >> 5)
#define MPP_FD_BIT(fd) (1 << ((fd) & 0x1f))

#define MPP_FD_SET(fd, set) do{ \
	if (fd > 65535) \
	elog(FATAL,"Internal error:  Using fd > 65535 in MPP_FD_SET"); \
	((set)->__fds_bits[MPP_FD_WORD(fd)] = ((set)->__fds_bits[MPP_FD_WORD(fd)]) | MPP_FD_BIT(fd)); \
    } while(0)
#define MPP_FD_CLR(fd, set) ((set)->__fds_bits[MPP_FD_WORD(fd)] &= ~MPP_FD_BIT(fd))

#define MPP_FD_ISSET(fd, set) (((set)->__fds_bits[MPP_FD_WORD(fd)] & MPP_FD_BIT(fd)) ? 1 : 0)

#else

#define mpp_fd_set fd_set
#define MPP_FD_ZERO(setp) FD_ZERO(setp)

#define MPP_FD_SET(fd, set) do{ \
	if (fd > FD_SETSIZE) \
	elog(FATAL,"Internal error:  Using fd > FD_SETSIZE in FD_SET (MPP_FD_SET)"); \
	FD_SET(fd, set); \
	} while(0)
#define MPP_FD_CLR(fd, set) FD_CLR(fd, set)

#define MPP_FD_ISSET(fd, set) FD_ISSET(fd, set)

#endif

#endif
