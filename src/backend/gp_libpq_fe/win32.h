/*
 * $PostgreSQL: pgsql/src/interfaces/libpq/win32.h,v 1.30 2009/06/11 14:49:14 momjian Exp $
 */
#ifndef __win32_h_included
#define __win32_h_included

#include "gp-libpq-use.h"

/*
 * Some compatibility functions
 */
#ifdef __BORLANDC__
#define _timeb timeb
#define _ftime(a) ftime(a)
#define _errno errno
#define popen(a,b) _popen(a,b)
#else
/* open provided elsewhere */
#define close(a) _close(a)
#define read(a,b,c) _read(a,b,c)
#define write(a,b,c) _write(a,b,c)
#endif

#undef EAGAIN					/* doesn't apply on sockets */
#undef EINTR
#define EINTR WSAEINTR
#undef EWOULDBOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef ECONNRESET
#define ECONNRESET WSAECONNRESET
#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS

/*
 * support for handling Windows Socket errors
 */
extern const char *winsock_strerror(int err, char *strerrbuf, size_t buflen);

#include "gp-libpq-unuse.h"

#endif
