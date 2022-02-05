/*-------------------------------------------------------------------------
 *
 * be-gssapi-common.h
 *       Definitions for GSSAPI authentication and encryption handling
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/be-gssapi-common.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BE_GSSAPI_COMMON_H
#define BE_GSSAPI_COMMON_H

#if defined(HAVE_GSSAPI_H)
#include <gssapi.h>
#else
#include <gssapi/gssapi.h>
#if HAVE_GSSAPI_PROXY
#include <gssapi/gssapi_ext.h>
#endif
#endif

void		pg_GSS_error_be(int severity, const char *errmsg,
						 OM_uint32 maj_stat, OM_uint32 min_stat);
void		pg_store_proxy_credential(gss_cred_id_t cred);
#endif							/* BE_GSSAPI_COMMON_H */
