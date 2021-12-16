/*-------------------------------------------------------------------------
 *
 * be-gssapi-common.c
 *     Common code for GSSAPI authentication and encryption
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *       src/backend/libpq/be-gssapi-common.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/be-gssapi-common.h"

/*
 * Helper function for getting all strings of a GSSAPI error (of specified
 * stat).  Call once for GSS_CODE and once for MECH_CODE.
 */
static void
pg_GSS_error_int(char *s, size_t len, OM_uint32 stat, int type)
{
	gss_buffer_desc gmsg;
	size_t		i = 0;
	OM_uint32	lmin_s,
				msg_ctx = 0;

	gmsg.value = NULL;
	gmsg.length = 0;

	do
	{
		gss_display_status(&lmin_s, stat, type,
						   GSS_C_NO_OID, &msg_ctx, &gmsg);
		strlcpy(s + i, gmsg.value, len - i);
		i += gmsg.length;
		gss_release_buffer(&lmin_s, &gmsg);
	}
	while (msg_ctx && i < len);

	if (msg_ctx || i == len)
		ereport(WARNING,
				(errmsg_internal("incomplete GSS error report")));
}

/*
 * Fetch and report all error messages from GSSAPI.  To avoid allocation,
 * total error size is capped (at 128 bytes for each of major and minor).  No
 * known mechanisms will produce error messages beyond this cap.
 */
/*
 * In GPDB backend, we also link with fe-gssapi-common.o, which contains
 * this same function. Rename it with a "_be" suffix here to avoid linker error.
 */
void
pg_GSS_error_be(int severity, const char *errmsg,
			 OM_uint32 maj_stat, OM_uint32 min_stat)
{
	char		msg_major[128],
				msg_minor[128];

	/* Fetch major status message */
	pg_GSS_error_int(msg_major, sizeof(msg_major), maj_stat, GSS_C_GSS_CODE);

	/* Fetch mechanism minor status message */
	pg_GSS_error_int(msg_minor, sizeof(msg_minor), min_stat, GSS_C_MECH_CODE);

	/*
	 * errmsg_internal, since translation of the first part must be done
	 * before calling this function anyway.
	 */
	ereport(severity,
			(errmsg_internal("%s", errmsg),
			 errdetail_internal("%s: %s", msg_major, msg_minor)));
}

#if HAVE_GSSAPI_PROXY
void
pg_store_proxy_credential(gss_cred_id_t cred)
{
	OM_uint32 major, minor;
	gss_OID_set mech;
	gss_cred_usage_t usage;
	gss_key_value_element_desc cc;
	gss_key_value_set_desc ccset;

	cc.key = "ccache";
	cc.value = "MEMORY:";
	ccset.count = 1;
	ccset.elements = &cc;

	/* Make the proxy credential only available to current process */
	major = gss_store_cred_into(&minor,
		cred,
		GSS_C_INITIATE, /* credential only used for starting libpq connection */
		GSS_C_NULL_OID, /* store all */
		true, /* overwrite */
		true, /* make default */
		&ccset,
		&mech,
		&usage);


	if (major != GSS_S_COMPLETE)
	{
		pg_GSS_error_be(ERROR, "gss_store_cred", major, minor);
	}

	/* quite strange that gss_store_cred doesn't work with "KRB5CCNAME=MEMORY:",
	 * we have to use gss_store_cred_into instead and set the env for later
	 * gss_acquire_cred calls. */
	putenv("KRB5CCNAME=MEMORY:");
}
#else
void
pg_store_proxy_credential(gss_cred_id_t cred){

}
#endif
