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
static void BuildUriForRead(PxfFdwScanState *pxfsstate);
static void BuildUriForWrite(PxfFdwModifyState *pxfmstate);
static void SetCurrentFragmentHeaders(PxfFdwScanState *pxfsstate);
static size_t FillBuffer(PxfFdwScanState *pxfsstate, char *start, int minlen, int maxlen);

/*
 * Clean up churl related data structures from the PXF FDW modify state.
 */
void
PxfBridgeCleanup(PxfFdwModifyState *pxfmstate)
{
	if (pxfmstate == NULL)
		return;

	churl_cleanup(pxfmstate->churl_handle, false);
	pxfmstate->churl_handle = NULL;

	churl_headers_cleanup(pxfmstate->churl_headers);
	pxfmstate->churl_headers = NULL;

	/* TODO: do we need to cleanup filter_str for foreign scan? */
/*	if (pxfmstate->filter_str != NULL)
	{
		pfree(pxfmstate->filter_str);
		pxfmstate->filter_str = NULL;
	}*/
}

/*
 * Sets up data before starting import
 */
void
PxfBridgeImportStart(PxfFdwScanState *pxfsstate)
{
	if (!pxfsstate->fragments)
		return;

	pxfsstate->current_fragment = list_head(pxfsstate->fragments);
	pxfsstate->churl_headers = churl_headers_init();

	BuildUriForRead(pxfsstate);
	BuildHttpHeaders(pxfsstate->churl_headers,
					 pxfsstate->options,
					 pxfsstate->relation,
					 pxfsstate->filter_str,
					 pxfsstate->retrieved_attrs);
	SetCurrentFragmentHeaders(pxfsstate);

	pxfsstate->churl_handle = churl_init_download(pxfsstate->uri.data, pxfsstate->churl_headers);

	/* read some bytes to make sure the connection is established */
	churl_read_check_connectivity(pxfsstate->churl_handle);
}

/*
 * Sets up data before starting export
 */
void
PxfBridgeExportStart(PxfFdwModifyState *pxfmstate)
{
	BuildUriForWrite(pxfmstate);
	pxfmstate->churl_headers = churl_headers_init();
	BuildHttpHeaders(pxfmstate->churl_headers,
					 pxfmstate->options,
					 pxfmstate->relation,
					 NULL,
					 NULL);
	pxfmstate->churl_handle = churl_init_upload(pxfmstate->uri.data, pxfmstate->churl_headers);
}

/*
 * Reads data from the PXF server into the given buffer of a given size
 */
int
PxfBridgeRead(void *outbuf, int minlen, int maxlen, void *extra)
{
	size_t		n = 0;
	PxfFdwScanState *pxfsstate = (PxfFdwScanState *) extra;

	if (!pxfsstate->fragments)
		return (int) n;

	while ((n = FillBuffer(pxfsstate, outbuf, minlen, maxlen)) == 0)
	{
		/*
		 * done processing all data for current fragment - check if the
		 * connection terminated with an error
		 */
		churl_read_check_connectivity(pxfsstate->churl_handle);

		/* start processing next fragment */
		pxfsstate->current_fragment = lnext(pxfsstate->current_fragment);
		if (pxfsstate->current_fragment == NULL)
			return 0;

		SetCurrentFragmentHeaders(pxfsstate);
		churl_download_restart(pxfsstate->churl_handle, pxfsstate->uri.data, pxfsstate->churl_headers);

		/* read some bytes to make sure the connection is established */
		churl_read_check_connectivity(pxfsstate->churl_handle);
	}

	return (int) n;
}

/*
 * Writes data from the given buffer of a given size to the PXF server
 */
int
PxfBridgeWrite(PxfFdwModifyState *pxfmstate, char *databuf, int datalen)
{
	size_t		n = 0;

	if (datalen > 0)
	{
		n = churl_write(pxfmstate->churl_handle, databuf, datalen);
		elog(DEBUG5, "pxf PxfBridgeWrite: segment %d wrote %zu bytes to %s", PXF_SEGMENT_ID, n, pxfmstate->options->resource);
	}

	return (int) n;
}

/*
 * Format the URI for reading by adding PXF service endpoint details
 */
static void
BuildUriForRead(PxfFdwScanState *pxfsstate)
{
	FragmentData *data = (FragmentData *) lfirst(pxfsstate->current_fragment);

	resetStringInfo(&pxfsstate->uri);
	appendStringInfo(&pxfsstate->uri, "http://%s/%s/%s/Bridge", data->authority, PXF_SERVICE_PREFIX, PXF_VERSION);
	elog(DEBUG2, "pxf_fdw: uri %s for read", pxfsstate->uri.data);
}

/*
 * Format the URI for writing by adding PXF service endpoint details
 */
static void
BuildUriForWrite(PxfFdwModifyState *pxfmstate)
{
	PxfOptions *options = pxfmstate->options;

	resetStringInfo(&pxfmstate->uri);
	appendStringInfo(&pxfmstate->uri, "http://%s:%d/%s/%s/Writable/stream", options->pxf_host, options->pxf_port, PXF_SERVICE_PREFIX, PXF_VERSION);
	elog(DEBUG2, "pxf_fdw: uri %s with file name for write: %s", pxfmstate->uri.data, options->resource);
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
SetCurrentFragmentHeaders(PxfFdwScanState *pxfsstate)
{
	FragmentData *frag_data = (FragmentData *) lfirst(pxfsstate->current_fragment);
	int			fragment_count = list_length(pxfsstate->fragments);

	elog(DEBUG2, "pxf_fdw: set_current_fragment_source_name: source_name %s, index %s, has user data: %s ",
		 frag_data->source_name, frag_data->index, frag_data->user_data ? "TRUE" : "FALSE");

	churl_headers_override(pxfsstate->churl_headers, "X-GP-DATA-DIR", frag_data->source_name);
	churl_headers_override(pxfsstate->churl_headers, "X-GP-DATA-FRAGMENT", frag_data->index);
	churl_headers_override(pxfsstate->churl_headers, "X-GP-FRAGMENT-METADATA", frag_data->fragment_md);
	churl_headers_override(pxfsstate->churl_headers, "X-GP-FRAGMENT-INDEX", frag_data->index);

	if (frag_data->fragment_idx == fragment_count)
	{
		churl_headers_override(pxfsstate->churl_headers, "X-GP-LAST-FRAGMENT", "true");
	}

	if (frag_data->user_data)
	{
		churl_headers_override(pxfsstate->churl_headers, "X-GP-FRAGMENT-USER-DATA", frag_data->user_data);
	}
	else
	{
		churl_headers_remove(pxfsstate->churl_headers, "X-GP-FRAGMENT-USER-DATA", true);
	}

	if (frag_data->profile)
	{
		/* if current fragment has optimal profile set it */
		churl_headers_override(pxfsstate->churl_headers, "X-GP-PROFILE", frag_data->profile);
		elog(DEBUG2, "pxf_fdw: SetCurrentFragmentHeaders: using profile: %s", frag_data->profile);

	}
}

/*
 * Read data from churl until the buffer is full or there is no more data to be read
 */
static size_t
FillBuffer(PxfFdwScanState *pxfsstate, char *start, int minlen, int maxlen)
{
	size_t		n = 0;
	char	   *ptr = start;
	char	   *minend = ptr + minlen;
	char	   *maxend = ptr + maxlen;

	while (ptr < minend)
	{
		n = churl_read(pxfsstate->churl_handle, ptr, maxend - ptr);
		if (n == 0)
			break;

		ptr += n;
	}

	return ptr - start;
}
