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

#include "pxf_bridge.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"

/* helper function declarations */
static void BuildUriForRead(PxfContext * context);
static void BuildUriForWrite(PxfContext * context);
static void AddQuerydataToHttpHeaders(PxfContext * context);
static void SetCurrentFragmentHeaders(PxfContext * context);
static size_t FillBuffer(PxfContext * context, char *start, size_t size);

/*
 * Clean up churl related data structures from the context.
 */
void
PxfBridgeCleanup(PxfContext * context)
{
	if (context == NULL)
		return;

	churl_cleanup(context->churl_handle, false);
	context->churl_handle = NULL;

	churl_headers_cleanup(context->churl_headers);
	context->churl_headers = NULL;

	if (context->filterstr != NULL)
	{
		pfree(context->filterstr);
		context->filterstr = NULL;
	}
}

/*
 * Sets up data before starting import
 */
void
PxfBridgeImportStart(PxfContext * context)
{
	if (!context->fragments)
		return;

	context->current_fragment = list_head(context->fragments);
	context->churl_headers = churl_headers_init();

	BuildUriForRead(context);
	AddQuerydataToHttpHeaders(context);
	SetCurrentFragmentHeaders(context);

	context->churl_handle = churl_init_download(context->uri.data, context->churl_headers);

	/* read some bytes to make sure the connection is established */
	churl_read_check_connectivity(context->churl_handle);
}

/*
 * Sets up data before starting export
 */
void
pxfBridgeExportStart(PxfContext * context)
{
	elog(ERROR, "pxf_fdw: pxfBridgeExportStart not implemented");
}

/*
 * Reads data from the PXF server into the given buffer of a given size
 */
int
PxfBridgeRead(PxfContext * context, char *databuf, int datalen)
{
	size_t		n = 0;

	if (!context->fragments)
		return (int) n;

	while ((n = FillBuffer(context, databuf, datalen)) == 0)
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

		SetCurrentFragmentHeaders(context);
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
PxfBridgeWrite(PxfContext * context, char *databuf, int datalen)
{
	size_t		n = 0;

	if (datalen > 0)
	{
		n = churl_write(context->churl_handle, databuf, datalen);
/* 		elog(DEBUG5, "pxf gpbridge_write: segment %d wrote %zu bytes to %s", PXF_SEGMENT_ID, n, context->gphd_uri->data); */
	}

	return (int) n;
}

/*
 * Format the URI for reading by adding PXF service endpoint details
 */
static void
BuildUriForRead(PxfContext * context)
{
	FragmentData *data = (FragmentData *) lfirst(context->current_fragment);

	resetStringInfo(&context->uri);
	appendStringInfo(&context->uri, "http://%s/%s/%s/Bridge", data->authority, PXF_SERVICE_PREFIX, PXF_VERSION);
	elog(DEBUG2, "pxf_fdw: uri %s for read", context->uri.data);
}

/*
 * Format the URI for writing by adding PXF service endpoint details
 */
static void
BuildUriForWrite(PxfContext * context)
{
	elog(ERROR, "pxf_fdw: BuildUriForWrite not implemented");
}


/*
 * Add key/value pairs to connection header. These values are the context of the query and used
 * by the remote component.
 */
static void
AddQuerydataToHttpHeaders(PxfContext * context)
{
	BuildHttpHeaders(context->churl_headers,
					 context->options,
					 context->relation,
					 NULL,
					 context->proj_info,
					 context->quals);
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
SetCurrentFragmentHeaders(PxfContext * context)
{
	FragmentData *frag_data = (FragmentData *) lfirst(context->current_fragment);
	int			fragment_count = list_length(context->fragments);

	elog(DEBUG2, "pxf_fdw: set_current_fragment_source_name: source_name %s, index %s, has user data: %s ",
		 frag_data->source_name, frag_data->index, frag_data->user_data ? "TRUE" : "FALSE");

	churl_headers_override(context->churl_headers, "X-GP-DATA-DIR", frag_data->source_name);
	churl_headers_override(context->churl_headers, "X-GP-DATA-FRAGMENT", frag_data->index);
	churl_headers_override(context->churl_headers, "X-GP-FRAGMENT-METADATA", frag_data->fragment_md);
	churl_headers_override(context->churl_headers, "X-GP-FRAGMENT-INDEX", frag_data->index);

	if (frag_data->fragment_idx == fragment_count)
	{
		churl_headers_override(context->churl_headers, "X-GP-LAST-FRAGMENT", "true");
	}

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
		elog(DEBUG2, "pxf_fdw: SetCurrentFragmentHeaders: using profile: %s", frag_data->profile);

	}
}

/*
 * Read data from churl until the buffer is full or there is no more data to be read
 */
static size_t
FillBuffer(PxfContext * context, char *start, size_t size)
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
