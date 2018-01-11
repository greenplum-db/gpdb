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
 *
 */

#include "pxfbridge.h"
#include "pxfheaders.h"
#include "pxffragment.h"
#include "pxfutils.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"

/* helper function declarations */
static void build_uri_for_read(gphadoop_context *context);
static void build_uri_for_write(gphadoop_context *context);
static void build_file_name_for_write(gphadoop_context *context);
static void add_querydata_to_http_headers(gphadoop_context *context);
static void set_current_fragment_headers(gphadoop_context *context);
static size_t fill_buffer(gphadoop_context *context, char *start, size_t size);

/*
 * Clean up churl related data structures from the context.
 */
void
gpbridge_cleanup(gphadoop_context *context)
{
	if (context == NULL)
		return;

	churl_cleanup(context->churl_handle, false);
	context->churl_handle = NULL;

	churl_headers_cleanup(context->churl_headers);
	context->churl_headers = NULL;

	if (context->gphd_uri != NULL)
	{
		freeGPHDUri(context->gphd_uri);
		context->gphd_uri = NULL;
	}
}

/*
 * Sets up data before starting import
 */
void
gpbridge_import_start(gphadoop_context *context)
{
	if (context->gphd_uri->fragments == NULL)
		return;

	context->current_fragment = list_head(context->gphd_uri->fragments);
	build_uri_for_read(context);
	context->churl_headers = churl_headers_init();
	add_querydata_to_http_headers(context);

	set_current_fragment_headers(context);

	context->churl_handle = churl_init_download(context->uri.data, context->churl_headers);

	/* read some bytes to make sure the connection is established */
	churl_read_check_connectivity(context->churl_handle);
}

/*
 * Sets up data before starting export
 */
void
gpbridge_export_start(gphadoop_context *context)
{
	build_file_name_for_write(context);
	build_uri_for_write(context);
	context->churl_headers = churl_headers_init();
	add_querydata_to_http_headers(context);

	context->churl_handle = churl_init_upload(context->uri.data, context->churl_headers);
}

/*
 * Reads data from the PXF server into the given buffer of a given size
 */
int
gpbridge_read(gphadoop_context *context, char *databuf, int datalen)
{
	size_t		n = 0;

	if (context->gphd_uri->fragments == NULL)
		return (int) n;

	while ((n = fill_buffer(context, databuf, datalen)) == 0)
	{
		/*
		 * done processing all data for current fragment - check if the
		 * connection terminated with an error
		 */
		churl_read_check_connectivity(context->churl_handle);

		/* start processing next fragment */
		context->current_fragment = lnext(context->current_fragment);
		if (context->current_fragment == NULL)
			return 0;

		set_current_fragment_headers(context);
		churl_download_restart(context->churl_handle, context->uri.data, context->churl_headers);

		/* read some bytes to make sure the connection is established */
		churl_read_check_connectivity(context->churl_handle);
	}

	return (int) n;
}

/*
 * Writes data from the given buffer of a given size to the PXF server
 */
int
gpbridge_write(gphadoop_context *context, char *databuf, int datalen)
{
	size_t		n = 0;

	if (datalen > 0)
	{
		n = churl_write(context->churl_handle, databuf, datalen);
		elog(DEBUG5, "pxf gpbridge_write: wrote %zu bytes to %s", n, context->write_file_name.data);
	}

	return (int) n;
}

/*
 * Format the URI for reading by adding PXF service endpoint details
 */
static void
build_uri_for_read(gphadoop_context *context)
{
	FragmentData *data = (FragmentData *) lfirst(context->current_fragment);

	resetStringInfo(&context->uri);
	appendStringInfo(&context->uri, "http://%s/%s/%s/Bridge/",
					 data->authority, PXF_SERVICE_PREFIX, PXF_VERSION);
	elog(DEBUG2, "pxf: uri %s for read", context->uri.data);
}

/*
 * Format the URI for writing by adding PXF service endpoint details
 */
static void
build_uri_for_write(gphadoop_context *context)
{
	appendStringInfo(&context->uri, "http://%s/%s/%s/Writable/stream?path=%s",
					 get_authority(), PXF_SERVICE_PREFIX, PXF_VERSION, context->write_file_name.data);
	elog(DEBUG2, "pxf: uri %s with file name for write: %s", context->uri.data, context->write_file_name.data);
}

/*
 * Builds a unique file name for write per segment, based on
 * directory name from the table's URI, the transaction id (XID) and segment id.
 * e.g. with path in URI '/data/writable/table1', XID 1234 and segment id 3,
 * the file name will be '/data/writable/table1/1234_3'.
 */
static void
build_file_name_for_write(gphadoop_context *context)
{
	char		xid[TMGIDSIZE];

	if (!getDistributedTransactionIdentifier(xid))
		elog(ERROR, "Unable to obtain distributed transaction id");

	appendStringInfo(&context->write_file_name, "/%s/%s_%d",
					 context->gphd_uri->data, xid, GpIdentity.segindex);
	elog(DEBUG2, "pxf: file name for write: %s", context->write_file_name.data);
}


/*
 * Add key/value pairs to connection header. These values are the context of the query and used
 * by the remote component.
 */
static void
add_querydata_to_http_headers(gphadoop_context *context)
{
	PxfInputData inputData = {0};

	inputData.headers = context->churl_headers;
	inputData.gphduri = context->gphd_uri;
	inputData.rel = context->relation;
	build_http_headers(&inputData);
}

/*
 * Change the headers with current fragment information:
 * 1. X-GP-DATA-DIR header is changed to the source name of the current fragment.
 * We reuse the same http header to send all requests for specific fragments.
 * The original header's value contains the name of the general path of the query
 * (can be with wildcard or just a directory name), and this value is changed here
 * to the specific source name of each fragment name.
 * 2. X-GP-FRAGMENT-USER-DATA header is changed to the current fragment's user data.
 * If the fragment doesn't have user data, the header will be removed.
 */
static void
set_current_fragment_headers(gphadoop_context *context)
{
	FragmentData *frag_data = (FragmentData *) lfirst(context->current_fragment);

	elog(DEBUG2, "pxf: set_current_fragment_source_name: source_name %s, index %s, has user data: %s ",
		 frag_data->source_name, frag_data->index, frag_data->user_data ? "TRUE" : "FALSE");

	churl_headers_override(context->churl_headers, "X-GP-DATA-DIR", frag_data->source_name);
	churl_headers_override(context->churl_headers, "X-GP-DATA-FRAGMENT", frag_data->index);
	churl_headers_override(context->churl_headers, "X-GP-FRAGMENT-METADATA", frag_data->fragment_md);
	churl_headers_override(context->churl_headers, "X-GP-FRAGMENT-INDEX", frag_data->index);

	if (frag_data->user_data)
	{
		churl_headers_override(context->churl_headers, "X-GP-FRAGMENT-USER-DATA", frag_data->user_data);
	}
	else
	{
		churl_headers_remove(context->churl_headers, "X-GP-FRAGMENT-USER-DATA", true);
	}

	if (frag_data->profile)
	{
		/* if current fragment has optimal profile set it */
		churl_headers_override(context->churl_headers, "X-GP-PROFILE", frag_data->profile);
		elog(DEBUG2, "pxf: set_current_fragment_headers: using profile: %s", frag_data->profile);

	}
	else if (context->gphd_uri->profile)
	{
		/*
		 * if current fragment doesn't have any optimal profile, set to use
		 * profile from url
		 */
		churl_headers_override(context->churl_headers, "X-GP-PROFILE", context->gphd_uri->profile);
		elog(DEBUG2, "pxf: set_current_fragment_headers: using profile: %s", context->gphd_uri->profile);
	}

	/*
	 * if there is no profile passed in url, we expect to have
	 * accessor+fragmenter+resolver so no action needed by this point
	 */

}

/*
 * Read data from churl until the buffer is full or there is no more data to be read
 */
static size_t
fill_buffer(gphadoop_context *context, char *start, size_t size)
{

	size_t		n = 0;
	char	   *ptr = start;
	char	   *end = ptr + size;

	while (ptr < end)
	{
		n = churl_read(context->churl_handle, ptr, end - ptr);
		if (n == 0)
			break;

		ptr += n;
	}

	return ptr - start;
}
