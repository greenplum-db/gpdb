#include "postgres.h"

#include "pxf_fragment.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "commands/copy.h"
#include "foreign/foreign.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/jsonapi.h"

#define LOG_DEBUG (FRAGDEBUG >= log_min_messages) || (FRAGDEBUG >= client_min_messages)

static List *GetDataFragmentList(PxfOptions *options, ClientContext * client_context);
static void RestRequest(PxfOptions *options, ClientContext * client_context, char *rest_msg);
static List *ParseGetFragmentsResponse(StringInfo rest_buf);
static void Init(ClientContext * client_context);
static void LogFragmentList(const char *debugHeader, List *fragments);
static void InitClientContext(ClientContext * client_context);
static void CallRest(PxfOptions *options, ClientContext * client_context, char *rest_msg);
static void ProcessRequest(ClientContext * client_context, char *uri);
static void PxfFragmentScalar(void *state, char *token, JsonTokenType type);
static void PxfFragmentObjectStart(void *state, char *name, bool isnull);
static void PxfArrayElementStart(void *state, bool isnull);
static void PxfArrayElementEnd(void *state, bool isnull);

/* Get List of fragments using PXF
 * Returns selected fragments that have been allocated to the current segment
 */
List *
GetFragmentList(PxfOptions *options,
				Relation relation,
				char *filter_string,
				List *retrieved_attrs)
{
	List	   *data_fragments;

	/* Context for the Fragmenter API */
	ClientContext client_context;

	Assert(options != NULL);

	/* 1. Initialize curl headers */
	Init(&client_context);

	/* Enrich the curl HTTP header */
	BuildHttpHeaders(client_context.http_headers,
					 options,
					 relation,
					 filter_string,
					 retrieved_attrs);

	/*
	 * Get the fragments data from the PXF service
	 */
	data_fragments = GetDataFragmentList(options, &client_context);
	if (data_fragments == NIL)
		return NIL;

	if (LOG_DEBUG)
		LogFragmentList("Available Data fragments", data_fragments);

	return data_fragments;
}

/*
 * Assign PXF Host for each Data Fragment
 * Will use the same host as the existing segment as the PXF host.
 */
void
AssignPxfLocationToFragments(PxfOptions *options, List *fragments)
{
	ListCell   *frag_c = NULL;

	foreach(frag_c, fragments)
	{
		FragmentData *fragment = (FragmentData *) lfirst(frag_c);
		fragment->authority = psprintf("%s:%d", options->pxf_host, options->pxf_port);
	}
}

/*
 * GetDataFragmentList
 *
 * 1. Request a list of fragments from the PXF Fragmenter class
 * that was specified in the pxf URL.
 *
 * 2. Process the returned list and create a list of DataFragment with it.
 */
static List *
GetDataFragmentList(PxfOptions *options,
					ClientContext * client_context)
{
	List	   *data_fragments;
	char	   *restMsg = "http://%s:%d/%s/%s/Fragmenter/getFragments";

	RestRequest(options, client_context, restMsg);

	/* parse the JSON response and form a fragments list to return */
	data_fragments = ParseGetFragmentsResponse(&(client_context->the_rest_buf));

	return data_fragments;
}

/*
 * Wrap the REST call.
 * TODO: Add logic later to handle HDFS HA related exception
 *       to failover with a retry for the HA HDFS scenario.
 */
static void
RestRequest(PxfOptions *options, ClientContext * client_context, char *rest_msg)
{
	Assert(options->pxf_host != NULL && options->pxf_port > 0);

	/* construct the request */
	CallRest(options, client_context, rest_msg);
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
	JsonLexContext *lex;
	pxf_fragment_object object;
	List	   *fragments;
	bool		has_replicas;
	int			arraydepth;
} FragmentState;

static void
PxfFragmentObjectStart(void *state, char *name, bool isnull)
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
CheckAndAssign(char **field, JsonTokenType type, char *token,
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
PxfFragmentScalar(void *state, char *token, JsonTokenType type)
{
	FragmentState *s = (FragmentState *) state;
	FragmentData *d = (FragmentData *) llast(s->fragments);

	/*
	 * Populate the fragment depending on the type of the scalar and the
	 * current object
	 */
	switch (s->object)
	{
		case PXF_PARSE_USERDATA:
			CheckAndAssign(&(d->user_data), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_METADATA:
			CheckAndAssign(&(d->fragment_md), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_PROFILE:
			CheckAndAssign(&(d->profile), type, token, JSON_TOKEN_STRING, true);
			break;
		case PXF_PARSE_SOURCENAME:
			CheckAndAssign(&(d->source_name), type, token, JSON_TOKEN_STRING, false);
			break;
		case PXF_PARSE_INDEX:
			CheckAndAssign(&(d->index), type, token, JSON_TOKEN_NUMBER, true);
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
PxfArrayElementStart(void *state, bool isnull)
{
	FragmentState *s = (FragmentState *) state;
	FragmentData *data;

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
PxfArrayElementEnd(void *state, bool isnull)
{
	FragmentState *s = (FragmentState *) state;

	if (--s->arraydepth == 0)
	{
		if (!s->has_replicas)
			s->fragments = list_truncate(s->fragments,
										 list_length(s->fragments) - 1);
	}
}

static List *
ParseGetFragmentsResponse(StringInfo rest_buf)
{
	JsonSemAction *sem;
	FragmentState *state;

	sem = palloc0(sizeof(JsonSemAction));
	state = palloc0(sizeof(FragmentState));

	state->fragments = NIL;
	state->lex = makeJsonLexContext(cstring_to_text(rest_buf->data), true);
	state->object = PXF_PARSE_START;
	state->arraydepth = 0;
	state->has_replicas = false;

	sem->semstate = state;
	sem->scalar = PxfFragmentScalar;
	sem->object_field_start = PxfFragmentObjectStart;
	sem->array_element_start = PxfArrayElementStart;
	sem->array_element_end = PxfArrayElementEnd;

	pg_parse_json(state->lex, sem);

	pfree(state->lex);

	return state->fragments;
}

/*
 * Takes a list of fragments and determines which ones need to be processes by the given segment based on MOD function.
 * Removes the elements which will not be processed from the list and frees up their memory.
 * Returns the resulting list, or NIL if no elements satisfy the condition.
 */
List *
FilterFragmentsForSegment(List *list)
{
	if (!list)
		elog(ERROR, "Parameter list is null in FilterFragmentsForSegment");

	/*
	 * to determine which segment S should process an element at a given index
	 * I, use a randomized MOD function
	 *
	 * S = MOD(I + MOD(gp_session_id, N) + gp_command_count, N)
	 *
	 * which ensures more fair work distribution for small lists of just a few
	 * elements across N segments global session ID and command count is used
	 * as a randomizer, as it is different for every query while being the same
	 * across all segments for a given query
	 */

	List	   *result = list;
	ListCell   *previous = NULL,
			   *current = NULL;

	int			index = 0;
	int			frag_index = 1;
	int			numsegments = PXF_SEGMENT_COUNT;
	int32		shift = gp_session_id % numsegments;

	for (current = list_head(list); current != NULL; index++)
	{
		if (PXF_SEGMENT_ID == (index + shift + gp_command_count) % numsegments)
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
				FreeFragment((FragmentData *) to_delete->data.ptr_value);
			current = lnext(to_delete);
			result = list_delete_cell(list, to_delete, previous);
		}
	}
	return result;
}

/*
 * Serializes a List of FragmentData such that it can be transmitted
 * via the interconnect, and the list can be passed from master to
 * segments
 */
List *
SerializeFragmentList(List *fragments)
{
	ListCell   *frag_c = NULL;
	List	   *serializedFragmentList = NIL;
	List	   *serializedFragment;

	foreach(frag_c, fragments)
	{
		FragmentData *fragment = (FragmentData *) lfirst(frag_c);

		serializedFragment = NIL;
		serializedFragment = lappend(serializedFragment, makeString(fragment->index));
		serializedFragment = lappend(serializedFragment, makeString(fragment->source_name));
		serializedFragment = lappend(serializedFragment, makeString(fragment->fragment_md));
		serializedFragment = lappend(serializedFragment, makeString(fragment->user_data));
		serializedFragment = lappend(serializedFragment, makeString(fragment->profile));
		serializedFragment = lappend(serializedFragment, makeString(psprintf("%d", fragment->fragment_idx)));

		serializedFragmentList = lappend(serializedFragmentList, serializedFragment);
	}

	return serializedFragmentList;
}

/*
 * Deserializes a List of FragmentData received from the interconnect
 */
List *
DeserializeFragmentList(List *serializedFragmentList)
{
	ListCell	*frag_c = NULL;
	List		*fragments = NIL;

	foreach(frag_c, serializedFragmentList)
	{
		List *serializedFragment = (List *) lfirst(frag_c);

		FragmentData *fragment = (FragmentData *) palloc0(sizeof(FragmentData));
		fragment->index = strVal(list_nth(serializedFragment, 0));
		fragment->source_name = strVal(list_nth(serializedFragment, 1));
		fragment->fragment_md = strVal(list_nth(serializedFragment, 2));
		fragment->user_data = strVal(list_nth(serializedFragment, 3));
		fragment->profile = strVal(list_nth(serializedFragment, 4));
		fragment->fragment_idx = atoi(strVal(list_nth(serializedFragment, 5)));

		fragments = lappend(fragments, fragment);
	}

	return fragments;
}

/*
 * Preliminary curl initializations for the REST communication
 */
static void
Init(ClientContext * client_context)
{
	InitClientContext(client_context);
	/* Communication with back-end, initialize churl client context and header */
	client_context->http_headers = churl_headers_init();

	/* set HTTP header that guarantees response in JSON format */
	churl_headers_append(client_context->http_headers, REST_HEADER_JSON_RESPONSE, NULL);
}

/*
 * Free fragment data
 */
void
FreeFragment(FragmentData *data)
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
LogFragmentList(const char *debugHeader, List *fragments)
{
	ListCell   *fragment_cell = NULL;
	StringInfoData log_str;

	initStringInfo(&log_str);

	appendStringInfo(&log_str, "---%s---\nFragment list: (%d elements)\n",
					 debugHeader, fragments ? fragments->length : 0);

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
	appendStringInfo(&log_str, "------\n");

	elog(FRAGDEBUG, "%s", log_str.data);
	pfree(log_str.data);
}

/*
 * Initializes the client context
 */
static void
InitClientContext(ClientContext * client_context)
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
CallRest(PxfOptions *options, ClientContext * client_context, char *rest_msg)
{
	StringInfoData request;

	initStringInfo(&request);

	appendStringInfo(&request, rest_msg,
					 options->pxf_host,
					 options->pxf_port,
					 PXF_SERVICE_PREFIX,
					 PXF_VERSION);

	/* send the request. The response will exist in rest_buf.data */
	ProcessRequest(client_context, request.data);
	pfree(request.data);
}

/*
 * Reads from churl in chunks of 64K and copies data to the context's buffer
 */
static void
ProcessRequest(ClientContext * client_context, char *uri)
{
	size_t		n = 0;
	char		buffer[RAW_BUF_SIZE];

	print_http_headers(client_context->http_headers);
	client_context->handle = churl_init_download(uri, client_context->http_headers);
	if (client_context->handle == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_FAILURE),
				 errmsg("unsuccessful connection to uri: \"%s\"", uri)));
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
