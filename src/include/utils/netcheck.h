/*-------------------------------------------------------------------------
 *
 * netcheck.h
 *	  Interface for network checking utilities.
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/netcheck.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef _NETCHECK_H
#define _NETCHECK_H


/* check if the NIC used for routing to the given host is running */
extern bool NetCheckNIC(const char *hostname);


#endif /* _NETCHECK_H */

/* EOF */
