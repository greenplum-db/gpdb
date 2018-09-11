#include "postgres.h"

#include "pxffragment.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "commands/copy.h"
#include "utils/guc.h"
#include "utils/jsonapi.h"

static List *get_data_fragment_list(GPHDUri *hadoop_uri, ClientContext *client_context);
static void rest_request(GPHDUri *hadoop_uri, ClientContext *client_context, char *rest_msg);
static List *parse_get_fragments_response(List *fragments, StringInfo rest_buf);
static List *filter_fragments_for_segment(List *list);
static void init(GPHDUri *uri, ClientContext *cl_context);
static void print_fragment_list(List *fragments);
static void init_client_context(ClientContext *client_context);
static void assign_pxf_location_to_fragments(List *fragments);
static void call_rest(GPHDUri *hadoop_uri, ClientContext *client_context, char *rest_msg);
static void process_request(ClientContext *client_context, char *uri);
static void pxf_fragment_scalar(void *state, char *token, JsonTokenType type);
static void pxf_fragment_object_start(void *state, char *name, bool isnull);
static void pxf_array_element_start(void *state, bool isnull);
static void pxf_array_element_end(void *state, bool isnull);

/* Get List of fragments using PXF
 * Returns selected fragments that have been allocated to the current segment
 */
void
get_fragments(GPHDUri *uri, Relation relation, char* filter_string)
{

	List	   *data_fragments = NIL;

	/* Context for the Fragmenter API */
	ClientContext client_context;
	PxfInputData inputData = {0};

	Assert(uri != NULL);

	/*
	 * 1. Initialize curl headers
	 */
	init(uri, &client_context);

	/*
	 * Enrich the curl HTTP header
	 */
	inputData.headers = client_context.http_headers;
	inputData.gphduri = uri;
	inputData.rel = relation;
	inputData.filterstr = filter_string;
	build_http_headers(&inputData);

	/*
	 * Get the fragments data from the PXF service
	 */
	data_fragments = get_data_fragment_list(uri, &client_context);
	if (data_fragments == NIL)
		return;

	if ((FRAGDEBUG >= log_min_messages) || (FRAGDEBUG >= client_min_messages))
	{
		elog(FRAGDEBUG, "---Available Data fragments---\n");
		print_fragment_list(data_fragments);
		elog(FRAGDEBUG, "------\n");
	}

	/*
	 * Call the work allocation algorithm
	 */
	data_fragments = filter_fragments_for_segment(data_fragments);
	if (data_fragments == NIL)
		return;

	/*
	 * Assign PXF location for the allocated fragments
	 */
	assign_pxf_location_to_fragments(data_fragments);

	if ((FRAGDEBUG >= log_min_messages) || (FRAGDEBUG >= client_min_messages))
	{
		elog(FRAGDEBUG, "---Allocated Data fragments---\n");
		print_fragment_list(data_fragments);
		elog(FRAGDEBUG, "------\n");
	}

	uri->fragments = data_fragments;

	return;
}

/*
 * Assign PXF Host for each Data Fragment
 * Will use the same host as the existing segment as the PXF host.
 */
static void
assign_pxf_location_to_fragments(List *fragments)
{
	ListCell   *frag_c = NULL;
	foreach(frag_c, fragments)
	{
		FragmentData *fragment = (FragmentData *) lfirst(frag_c);
		fragment->authority = get_authority();
	}
	return;
}

/*
 * get_data_fragment_list
 *
 * 1. Request a list of fragments from the PXF Fragmenter class
 * that was specified in the pxf URL.
 *
 * 2. Process the returned list and create a list of DataFragment with it.
 */
static List *
get_data_fragment_list(GPHDUri *hadoop_uri,
					   ClientContext *client_context)
{
	List	   *data_fragments = NIL;

	Assert(hadoop_uri->data != NULL);
	char	   *restMsg = concat(2, "http://%s:%s/%s/%s/Fragmenter/getFragments?path=", hadoop_uri->data);

	rest_request(hadoop_uri, client_context, restMsg);

	/* parse the JSON response and form a fragments list to return */
	data_fragments = parse_get_fragments_response(data_fragments, &(client_context->the_rest_buf));

	return data_fragments;
}

/*
 * Wrap the REST call.
 * TODO: Add logic later to handle HDFS HA related exception
 *       to failover with a retry for the HA HDFS scenario.
 */
static void
rest_request(GPHDUri *hadoop_uri, ClientContext *client_context, char *rest_msg)
{
	Assert(hadoop_uri->host != NULL && hadoop_uri->port != NULL);

	/* construct the request */
	call_rest(hadoop_uri, client_context, rest_msg);
}

/*
 * parse the response of the PXF Fragments call. An example:
 *
 * Request:
 * 		curl --header "X-GP-FRAGMENTER: HdfsDataFragmenter" "http://goldsa1mac.local:50070/pxf/v2/Fragmenter/getFragments?path=demo" (demo is a directory)
 *
 * Response (left as a single line purposefully):
 * {"PXFFragments":[{"index":0,"userData":null,"sourceName":"demo/text2.csv","metadata":"rO0ABXcQAAAAAAAAAAAAAAAAAAAABXVyABNbTGphdmEubGFuZy5TdHJpbmc7rdJW5+kde0cCAAB4cAAAAAN0ABxhZXZjZWZlcm5hczdtYnAuY29ycC5lbWMuY29tdAAcYWV2Y2VmZXJuYXM3bWJwLmNvcnAuZW1jLmNvbXQAHGFldmNlZmVybmFzN21icC5jb3JwLmVtYy5jb20=","replicas":["10.207.4.23","10.207.4.23","10.207.4.23"]},{"index":0,"userData":null,"sourceName":"demo/text_csv.csv","metadata":"rO0ABXcQAAAAAAAAAAAAAAAAAAAABnVyABNbTGphdmEubGFuZy5TdHJpbmc7rdJW5+kde0cCAAB4cAAAAAN0ABxhZXZjZWZlcm5hczdtYnAuY29ycC5lbWMuY29tdAAcYWV2Y2VmZXJuYXM3bWJwLmNvcnAuZW1jLmNvbXQAHGFldmNlZmVybmFzN21icC5jb3JwLmVtYy5jb20=","replicas":["10.207.4.23","10.207.4.23","10.207.4.23"]}]}
 */

typedef enum pxf_fragment_object
{
	PXF_PARSE_START,
	PXF_PARSE_INDEX,
	PXF_PARSE_USERDATA,
	PXF_PARSE_PROFILE,
	PXF_PARSE_SOURCENAME,
	PXF_PARSE_METADATA,
	PXF_PARSE_REPLICAS
} pxf_fragment_object;

typedef struct FragmentState
{
	JsonLexContext		   *lex;
	pxf_fragment_object		object;
	List				   *fragments;
	bool					has_replicas;
	int						arraydepth;
} FragmentState;

static void
pxf_fragment_object_start(void *state, char *name, bool isnull)
{
	FragmentState *s = (FragmentState *) state;

	if (s->lex->token_type == JSON_TOKEN_NUMBER)
	{
		if (pg_strcasecmp(name, "index") == 0)
			s->object = PXF_PARSE_INDEX;
	}
	else if (s->lex->token_type == JSON_TOKEN_STRING || s->lex->token_type == JSON_TOKEN_NULL)
	{
		if (pg_strcasecmp(name, "userData") == 0)
			s->object = PXF_PARSE_USERDATA;
		else if (pg_strcasecmp(name, "sourceName") == 0)
			s->object = PXF_PARSE_SOURCENAME;
		else if (pg_strcasecmp(name, "metadata") == 0)
			s->object = PXF_PARSE_METADATA;
		else if (pg_strcasecmp(name, "profile") == 0)
			s->object = PXF_PARSE_PROFILE;
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unrecognized object in PXF fragment: \"%s\"",
					 		name)));
	}
	else if (s->lex->token_type == JSON_TOKEN_ARRAY_START)
	{
		if (pg_strcasecmp(name, "PXFFragments") == 0)
		{
			if (s->object != PXF_PARSE_START)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("malformed PXF fragment")));
		}
		else if (pg_strcasecmp(name, "replicas") == 0)
		{
			s->object = PXF_PARSE_REPLICAS;
			s->has_replicas = false;
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unrecognized array in PXF fragment: \"%s\"",
					 		name)));
	}
}

static void
check_and_assign(char **field, JsonTokenType type, char *token,
				 JsonTokenType expected_type, bool nullable)
{
	if (type == JSON_TOKEN_NULL && nullable)
		return;

	if (type == expected_type)
		*field = pstrdup(token);
	else
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("unexpected value \"%s\" for attribute", token)));
}

static void
pxf_fragment_scalar(void *state, char *token, JsonTokenType type)
{
	FragmentState  *s = (FragmentState *) state;
	FragmentData   *d = (FragmentData *) llast(s->fragments);

	/* Populate the fragment depending on the type of the scalar and the current object */
	switch(s->object)
	{
		case PXF_PARSE_USERDATA:
			check_and_assign(&(d->user_data), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_METADATA:
			check_and_assign(&(d->fragment_md), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_PROFILE:
			check_and_assign(&(d->profile), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_SOURCENAME:
			check_and_assign(&(d->source_name), type, token, JSON_TOKEN_STRING, false);
			break;
		case PXF_PARSE_INDEX:
			check_and_assign(&(d->index), type, token, JSON_TOKEN_NUMBER, true);
			break;
		case PXF_PARSE_REPLICAS:
			if (type == JSON_TOKEN_STRING)
				s->has_replicas = true;
			break;
		case PXF_PARSE_START:
			break;
		default:
			elog(ERROR, "Unexpected PXF object state: %d", s->object);
			break;
	}
}

static void
pxf_array_element_start(void *state, bool isnull)
{
	FragmentState  *s = (FragmentState *) state;
	FragmentData   *data;

	/*
	 * If we are entering a nested array, an array inside the object in the
	 * main PXFFragments array, then we are still inside the current fragment
	 * and there is nothing but book keeping to do
	 */
	if (++s->arraydepth > 1)
		return;

	/*
	 * Reaching here means we are entering a new fragment in the PXFFragments
	 * array, allocate a new fragment on the list to populate during parsing.
	 */
	data = palloc0(sizeof(FragmentData));
	s->fragments = lappend(s->fragments, data);
	s->has_replicas = false;
	s->object = PXF_PARSE_START;
}

static void
pxf_array_element_end(void *state, bool isnull)
{
	FragmentState  *s = (FragmentState *) state;

	if (--s->arraydepth == 0)
	{
		if (!s->has_replicas)
			s->fragments = list_truncate(s->fragments,
										 list_length(s->fragments) - 1);
	}
}

static List *
parse_get_fragments_response(List *fragments, StringInfo rest_buf)
{
	JsonSemAction	*sem;
	FragmentState	*state;

	sem = palloc0(sizeof(JsonSemAction));
	state = palloc0(sizeof(FragmentState));

	state->fragments = NIL;
	state->lex = makeJsonLexContext(cstring_to_text(rest_buf->data), true);
	state->object = PXF_PARSE_START;
	state->arraydepth = 0;
	state->has_replicas = false;

	sem->semstate = state;
	sem->scalar = pxf_fragment_scalar;
	sem->object_field_start = pxf_fragment_object_start;
	sem->array_element_start = pxf_array_element_start;
	sem->array_element_end = pxf_array_element_end;

	pg_parse_json(state->lex, sem);

	pfree(state->lex);

    return state->fragments;
}

/*
 * Takes a list of fragments and determines which ones need to be processes by the given segment based on MOD function.
 * Removes the elements which will not be processed from the list and frees up their memory.
 * Returns the resulting list, or NIL if no elements satisfy the condition.
 */
static List *
filter_fragments_for_segment(List *list)
{
	if (!list)
		elog(ERROR, "Parameter list is null in filter_fragments_for_segment");

	DistributedTransactionId xid = getDistributedTransactionId();

	if (xid == InvalidDistributedTransactionId)
		elog(ERROR, "Cannot get distributed transaction identifier in filter_fragments_for_segment");

	/*
	 * to determine which segment S should process an element at a given index
	 * I, use a randomized MOD function
	 *
	 * S = MOD(I + MOD(XID, N), N)
	 *
	 * which ensures more fair work distribution for small lists of just a few
	 * elements across N segments global transaction ID is used as a
	 * randomizer, as it is different for every query while being the same
	 * across all segments for a given query
	 */

	List	   *result = list;
	ListCell   *previous = NULL,
			   *current = NULL;

	int			index = 0;
	int			frag_index = 1;
	int32		shift = xid % GpIdentity.numsegments;

	for (current = list_head(list); current != NULL; index++)
	{
		if (GpIdentity.segindex == (index + shift) % GpIdentity.numsegments)
		{
			/*
			 * current segment is the one that should process, keep the
			 * element, adjust cursor pointers
			 */
			FragmentData *frag = (FragmentData *) current->data.ptr_value;
			frag->fragment_idx = frag_index++;
			previous = current;
			current = lnext(current);
		}
		else
		{
			/*
			 * current segment should not process this element, another will,
			 * drop the element from the list
			 */
			ListCell   *to_delete = current;

			if (to_delete->data.ptr_value)
				free_fragment((FragmentData *) to_delete->data.ptr_value);
			current = lnext(to_delete);
			result = list_delete_cell(list, to_delete, previous);
		}
	}
	return result;
}

/*
 * Preliminary curl initializations for the REST communication
 */
static void
init(GPHDUri *uri, ClientContext *cl_context)
{
	/*
	 * Communication with the Hadoop back-end Initialize churl client context
	 * and header
	 */
	init_client_context(cl_context);
	cl_context->http_headers = churl_headers_init();

	/* set HTTP header that guarantees response in JSON format */
	churl_headers_append(cl_context->http_headers, REST_HEADER_JSON_RESPONSE, NULL);

	return;
}

/*
 * Free fragment data
 */
void
free_fragment(FragmentData *data)
{
	if (data->authority)
		pfree(data->authority);
	if (data->fragment_md)
		pfree(data->fragment_md);
	if (data->index)
		pfree(data->index);
	if (data->profile)
		pfree(data->profile);
	if (data->source_name)
		pfree(data->source_name);
	if (data->user_data)
		pfree(data->user_data);
	pfree(data);
}

/*
 * Debug function - print the splits data structure obtained from the namenode
 * response to <GET_BLOCK_LOCATIONS> request
 */
static void
print_fragment_list(List *fragments)
{
	ListCell   *fragment_cell = NULL;
	StringInfoData log_str;

	initStringInfo(&log_str);

	appendStringInfo(&log_str, "Fragment list: (%d elements)\n",
					 fragments ? fragments->length : 0);

	foreach(fragment_cell, fragments)
	{
		FragmentData *frag = (FragmentData *) lfirst(fragment_cell);

		appendStringInfo(&log_str, "Fragment index: %s\n", frag->index);
		appendStringInfo(&log_str, "authority: %s\n", frag->authority);
		appendStringInfo(&log_str, "source: %s\n", frag->source_name);
		appendStringInfo(&log_str, "metadata: %s\n", frag->fragment_md ? frag->fragment_md : "NULL");
		appendStringInfo(&log_str, "user data: %s\n", frag->user_data ? frag->user_data : "NULL");
		appendStringInfo(&log_str, "profile: %s\n", frag->profile ? frag->profile : "NULL");
	}

	elog(FRAGDEBUG, "%s", log_str.data);
	pfree(log_str.data);
}

/*
 * Initializes the client context
 */
static void
init_client_context(ClientContext *client_context)
{
	client_context->http_headers = NULL;
	client_context->handle = NULL;
	initStringInfo(&(client_context->the_rest_buf));
}


/*
 * call_rest
 *
 * Creates the REST message and sends it to the PXF service located on
 * <hadoop_uri->host>:<hadoop_uri->port>
 */
static void
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
process_request(ClientContext *client_context, char *uri)
{
	size_t		n = 0;
	char		buffer[RAW_BUF_SIZE];

	print_http_headers(client_context->http_headers);
	client_context->handle = churl_init_download(uri, client_context->http_headers);
	if (client_context->handle == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_FAILURE),
				 errmsg("Unsuccessful connection to uri: \"%s\"", uri)));
	memset(buffer, 0, RAW_BUF_SIZE);
	resetStringInfo(&(client_context->the_rest_buf));

	/*
	 * This try-catch ensures that in case of an exception during the
	 * "communication with PXF and the accumulation of PXF data in
	 * client_context->the_rest_buf", we still get to terminate the libcurl
	 * connection nicely and avoid leaving the PXF server connection hung.
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
