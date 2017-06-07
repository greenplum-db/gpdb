/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "pxfutils.h"
#include "commands/copy.h"
#include "utils/formatting.h"
#include "utils/syscache.h"

/* public function declarations */
static void process_request(ClientContext* client_context, char *uri);

/*
 * Checks if two strings are equal
 */
bool
are_ips_equal(char *ip1, char *ip2)
{
	if ((ip1 == NULL) || (ip2 == NULL))
		return false;
	return (strcmp(ip1, ip2) == 0);
}

/*
 * Override port str with given new port int
 */
void
port_to_str(char **port, int new_port)
{
	char tmp[10];

	if (!port)
		elog(ERROR, "unexpected internal error in pxfutils.c");
	if (*port)
		pfree(*port);

	Assert((new_port <= 65535) && (new_port >= 1)); /* legal port range */
	pg_ltoa(new_port, tmp);
	*port = pstrdup(tmp);
}

/*
 * call_rest
 *
 * Creates the REST message and sends it to the PXF service located on
 * <hadoop_uri->host>:<hadoop_uri->port>
 */
void
call_rest(GPHDUri *hadoop_uri, ClientContext *client_context, char *rest_msg)
{
	StringInfoData request;
	initStringInfo(&request);

	appendStringInfo(&request, rest_msg,
								hadoop_uri->host,
								hadoop_uri->port,
								PXF_SERVICE_PREFIX,
								PXF_VERSION);

	/* send the request. The response will exist in rest_buf.data */
	process_request(client_context, request.data);
	pfree(request.data);
}

/*
 * Reads from churl in chunks of 64K and copies data to the context's buffer
 */
static void
process_request(ClientContext* client_context, char *uri)
{
	size_t n = 0;
	char buffer[RAW_BUF_SIZE];

	print_http_headers(client_context->http_headers);
	client_context->handle = churl_init_download(uri, client_context->http_headers);
	if (client_context->handle == NULL)
		elog(ERROR, "Unsuccessful connection to uri: %s", uri);
	memset(buffer, 0, RAW_BUF_SIZE);
	resetStringInfo(&(client_context->the_rest_buf));

	/*
	 * This try-catch ensures that in case of an exception during the "communication with PXF and the accumulation of
	 * PXF data in client_context->the_rest_buf", we still get to terminate the libcurl connection nicely and avoid
	 * leaving the PXF server connection hung.
	 */
	PG_TRY();
	{
		/* read some bytes to make sure the connection is established */
		churl_read_check_connectivity(client_context->handle);
		while ((n = churl_read(client_context->handle, buffer, sizeof(buffer))) != 0)
		{
			appendBinaryStringInfo(&(client_context->the_rest_buf), buffer, n);
			memset(buffer, 0, RAW_BUF_SIZE);
		}
		churl_cleanup(client_context->handle, false);
	}
	PG_CATCH();
	{
		if (client_context->handle)
			churl_cleanup(client_context->handle, true);
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/*
 * Full name of the HEADER KEY expected by the PXF service
 * Converts input string to upper case and prepends "X-GP-" string
 *
 */
char*
normalize_key_name(const char* key)
{
	if (!key || strlen(key) == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.")));
	}

	StringInfoData formatter;
	initStringInfo(&formatter);
	char* upperCasedKey = str_toupper(pstrdup(key), strlen(key));
	appendStringInfo(&formatter, "X-GP-%s", upperCasedKey);
	pfree(upperCasedKey);

	return formatter.data;
}

/*
 * TypeOidGetTypename
 * Get the name of the type, given the OID
 */
char*
TypeOidGetTypename(Oid typid)
{

	Assert(OidIsValid(typid));

	HeapTuple	typtup;
	Form_pg_type typform;

	typtup = SearchSysCache(TYPEOID,
							ObjectIdGetDatum(typid),
							0, 0, 0);
	if (!HeapTupleIsValid(typtup))
		elog(ERROR, "cache lookup failed for type %u", typid);

	typform = (Form_pg_type) GETSTRUCT(typtup);

	char *typname = NameStr(typform->typname);

	StringInfoData tname;
	initStringInfo(&tname);
	appendStringInfo(&tname, "%s", typname);

	ReleaseSysCache(typtup);

	return tname.data;
}
