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

#include "pxfuriparser.h"
#include "pxfutils.h"

static const char* segwork_substring = "segwork=";
static const char segwork_separator = '@';
static const int EMPTY_VALUE_LEN = 2;

/* helper function declarations */
static void  GPHDUri_parse_protocol(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_authority(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_data(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_options(GPHDUri *uri, char **cursor);
static List* GPHDUri_parse_option(char* pair, GPHDUri *uri);
static void  GPHDUri_free_options(GPHDUri *uri);
static void  GPHDUri_parse_segwork(GPHDUri *uri, const char *uri_str);
static List* GPHDUri_parse_fragment(char* fragment, List* fragments);
static void  GPHDUri_free_fragments(GPHDUri *uri);
static char	*GPHDUri_dup_without_segwork(const char* uri);

/* parseGPHDUri
 *
 * Go over a URI string and parse it into its various components while
 * verifying valid structure given a specific target protocol.
 *
 * URI format:
 * 		<protocol name>://<authority>/<data>?<option>&<option>&<...>&segwork=<segwork>
 *
 *
 * protocol name	- must be 'pxf'
 * authority		- host:port
 * data				- data path (directory name/table name/etc., depending on target)
 * options			- valid options are dependent on the protocol. Each
 * 					  option is a key value pair.
 * 					  segwork option is not a user option but a gp master
 * 					  option. It is removed as fast as possible from the uri
 * 					  so errors, won't be printed with it to the user.
 *
 * inputs:
 * 		'uri_str'	- the raw uri str
 *
 * returns:
 * 		a parsed uri as a GPHDUri structure, or reports a format error.
 */
GPHDUri*
parseGPHDUri(const char *uri_str)
{
	GPHDUri	*uri = (GPHDUri *)palloc0(sizeof(GPHDUri));
	char	*cursor;

	uri->uri = GPHDUri_dup_without_segwork(uri_str);
	cursor = uri->uri;

	GPHDUri_parse_segwork(uri, uri_str);
	GPHDUri_parse_protocol(uri, &cursor);
	GPHDUri_parse_authority(uri, &cursor);
	GPHDUri_parse_data(uri, &cursor);
	GPHDUri_parse_options(uri, &cursor);

	return uri;
}

/*
 * Frees the elements of the data structure
 */
void
freeGPHDUri(GPHDUri *uri)
{
	pfree(uri->protocol);
	GPHDUri_free_fragments(uri);

	pfree(uri->host);
	pfree(uri->port);
	pfree(uri->data);
	if (uri->profile)
		pfree(uri->profile);

	GPHDUri_free_options(uri);

	pfree(uri);
}

/*
 * GPHDUri_parse_protocol
 *
 * Parse the protocol section of the URI which is passed down
 * in 'cursor', having 'cursor' point at the current string
 * location. Set the protocol string and the URI type.
 *
 * See parseGPHDUri header for URI structure description.
 */
static void
GPHDUri_parse_protocol(GPHDUri *uri, char **cursor)
{
	const char *ptc_sep = "://";
	int			ptc_sep_len = strlen(ptc_sep);
	char 	   *post_ptc;
	char	   *start = *cursor;
	int			ptc_len;

	post_ptc = strstr(start, ptc_sep);

	if(!post_ptc)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s", uri->uri)));

	ptc_len = post_ptc - start;
	uri->protocol = pnstrdup(start, ptc_len);

	if (!IS_PXF_URI(uri->uri))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s : unsupported protocol '%s'",
						uri->uri, uri->protocol)));

	/* set cursor to new position and return */
	*cursor = start + ptc_len + ptc_sep_len;
}

/*
 * GPHDUri_parse_authority
 *
 * Parse the authority section of the URI which is passed down
 * in 'cursor', having 'cursor' point at the current string
 * location.
 * authority string can have one of two (2) forms:
 *    host:port
 *    ha_nameservice_string
 *
 * See parseGPHDUri header for URI structure description.
 */
static void
GPHDUri_parse_authority(GPHDUri *uri, char **cursor)
{
	char		*portstart, *end, *ipv6, *hostport;
	int         totlen, hostlen; 
	const long  max_port_number = 65535;
	long        port;
	
	char		*hoststart = *cursor;

	/* implicit authority 'localhost:defport' (<ptc>:///) */
	if (*hoststart == '/')		
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s : missing authority section", uri->uri)));
	
	end = strchr(hoststart, '/');
	if (!end)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s : missing authority section", uri->uri)));
	/* host:port string*/	
	totlen = end - hoststart;
	hostport = pnstrdup(hoststart, totlen);
		
	/* find the portstart ':' */
	ipv6 = strchr(hostport, ']');
	if (ipv6) /* IPV6 */
		portstart = strchr(ipv6, ':');
	else /* IPV4 */
		portstart = strchr(hostport, ':');

	if (portstart) /* the authority is of the form host:port */
	{
		uri->port = pstrdup(portstart + 1);
		hostlen = portstart - hostport;
		uri->host = pnstrdup(hostport, hostlen);
	}

	pfree(hostport);
	*cursor = ++end;
	
	port = atol(uri->port);
	if (port <=0 || port > max_port_number)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid port: %s for authority host %s", uri->port, uri->host)));
}

/*
 * GPHDUri_parse_data
 *
 * Parse the data section of the URI which is passed down
 * in 'cursor', having 'cursor' point at the current string
 * location.
 *
 * See parseGPHDUri header for URI structure description.
 */
static void
GPHDUri_parse_data(GPHDUri *uri, char **cursor)
{
	char	*start = *cursor;
	char	*options_section = strrchr(start, '?');
	size_t	 data_len;

	/*
	 * If there exists an 'options' section, the data section length
	 * is from start point to options section point. Otherwise, the
	 * data section length is the remaining string length from start.
	 */
	if (options_section)
		data_len = options_section - start;
	else
		data_len = strlen(start);

	uri->data = pnstrdup(start, data_len);
	*cursor += data_len;
}

/*
 * GPHDUri_parse_options
 *
 * Parse the data section of the URI which is passed down
 * in 'cursor', having 'cursor' point at the current string
 * location.
 *
 * See parseGPHDUri header for URI structure description.
 */
static void
GPHDUri_parse_options(GPHDUri *uri, char **cursor)
{
	char    *dup = pstrdup(*cursor);
	char	*start = dup;

	/* option section must start with '?'. if absent, there are no options */
	if (!start || start[0] != '?')
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: missing options section", uri->uri)));

	/* skip '?' */
	start++;

	/* sanity check */
	if (strlen(start) < 2)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: invalid option after '?'", uri->uri)));

	/* ok, parse the options now */

	const char	*sep = "&";
	char		*strtok_context;
	char		*pair;

	for (pair = strtok_r(start, sep, &strtok_context);
			pair;
			pair = strtok_r(NULL, sep, &strtok_context))
	{
		uri->options = GPHDUri_parse_option(pair, uri);
	}

	pfree(dup);
}

/*
 * Parse an option in the form:
 * <key>=<value>
 * to OptionData object (key and value).
 */
static List*
GPHDUri_parse_option(char* pair, GPHDUri *uri)
{

	char	*sep;
	int		pair_len, key_len, value_len;
	OptionData* option_data;

	option_data = palloc0(sizeof(OptionData));
	pair_len = strlen(pair);

	sep = strchr(pair, '=');
	if (sep == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing '='", uri->uri, pair)));

	if (strchr(sep + 1, '=') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' contains duplicate '='", uri->uri, pair)));

	key_len = sep - pair;
	if (key_len == 0)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing key before '='", uri->uri, pair)));

	value_len = pair_len - key_len + 1;
	if (value_len == EMPTY_VALUE_LEN)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing value after '='", uri->uri, pair)));
    
	option_data->key = pnstrdup(pair,key_len);
	option_data->value = pnstrdup(sep + 1, value_len);

	char *x_gp_key = normalize_key_name(option_data->key);
	if (strcmp(x_gp_key, "X-GP-PROFILE") == 0)
	{
		uri->profile = pstrdup(option_data->value);
	}
	pfree(x_gp_key);

	return lappend(uri->options, option_data);
}

/*
 * Free options list
 */
static void
GPHDUri_free_options(GPHDUri *uri)
{
	ListCell *option = NULL;

	foreach(option, uri->options)
	{
		OptionData *data = (OptionData*)lfirst(option);
		pfree(data->key);
		pfree(data->value);
		pfree(data);
	}
	list_free(uri->options);
	uri->options = NIL;
}

/*
 * GPHDUri_parse_segwork parses the segwork section of the uri.
 * ...&segwork=<size>@<ip>@<port>@<index><size>@<ip>@<port>@<index><size>...
 */
static void
GPHDUri_parse_segwork(GPHDUri *uri, const char *uri_str)
{
	char		*segwork;
	char		*fragment;
	char		*sizestr;
	char		*size_end;
	int 		 fragment_size, count = 0;

	/* skip segwork= */
	segwork = strstr(uri_str, segwork_substring);
	if (segwork == NULL)
		return;

	segwork += strlen(segwork_substring);

	/*
	 * read next segment.
	 * each segment is prefixed its size.
	 */
	while (segwork && strlen(segwork))
	{
		/* expect size */
		size_end = strchr(segwork, segwork_separator);
		Assert(size_end != NULL);
        sizestr = pnstrdup(segwork, size_end - segwork);
		fragment_size = atoi(sizestr);
		segwork = size_end + 1; /* skip the size field */
		Assert(fragment_size <= strlen(segwork));

		fragment = pnstrdup(segwork, fragment_size);
		elog(DEBUG2, "GPHDUri_parse_segwork: fragment #%d, size %d, str %s", count, fragment_size, fragment);
		uri->fragments = GPHDUri_parse_fragment(fragment, uri->fragments);
		segwork += fragment_size;
		++count;
		pfree(sizestr);
		pfree(fragment);
	}
}

/*
 * Parsed a fragment string in the form:
 * <ip>@<port>@<index>[@user_data] - 192.168.1.1@1422@1[@user_data]
 * to authority ip:port - 192.168.1.1:1422
 * to index - 1
 * user data is optional
 */
static List*
GPHDUri_parse_fragment(char* fragment, List* fragments)
{
	if (!fragment)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is null.")));
	}

	char	*dup_frag = pstrdup(fragment);
	char	*value_start;
	char	*value_end;

	StringInfoData authority_formatter;
	FragmentData* fragment_data;

	fragment_data = palloc0(sizeof(FragmentData));
	initStringInfo(&authority_formatter);

	value_start = dup_frag;

	/* expect ip */
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	appendStringInfo(&authority_formatter, "%s:", value_start);
	value_start = value_end + 1;

	/* expect port */
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	appendStringInfo(&authority_formatter, "%s", value_start);
	fragment_data->authority = pstrdup(authority_formatter.data);
	pfree(authority_formatter.data);
	value_start = value_end + 1;

	/* expect source name */
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	fragment_data->source_name = pstrdup(value_start);
	value_start = value_end + 1;

	/* expect index */
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	fragment_data->index = pstrdup(value_start);
	value_start = value_end + 1;

	/* expect fragment metadata */
	Assert(value_start);
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	fragment_data->fragment_md = pstrdup(value_start);
	value_start = value_end + 1;

	/* expect user data */
	Assert(value_start);
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	fragment_data->user_data = pstrdup(value_start);
	value_start = value_end + 1;

	/* expect for profile */
	Assert(value_start);
	value_end = strchr(value_start, segwork_separator);
	if (value_end == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfuriparser.c:GPHDUri_parse_fragment. Fragment string is invalid.")));
	}
	*value_end = '\0';
	if (strlen(value_start) > 0)
		fragment_data->profile = pstrdup(value_start);

	return lappend(fragments, fragment_data);
}

/*
 * Free fragment data
 */
static void
GPHDUri_free_fragment(FragmentData *data)
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
 * Free fragments list
 */
static void 
GPHDUri_free_fragments(GPHDUri *uri)
{
	ListCell *fragment = NULL;
	
	foreach(fragment, uri->fragments)
	{
		FragmentData *data = (FragmentData*)lfirst(fragment);
		GPHDUri_free_fragment(data);
	}
	list_free(uri->fragments);
	uri->fragments = NIL;
}

/*
 * Returns a uri without the segwork section.
 * segwork section removed so users won't get it 
 * when an error occurs and the uri is printed
 */
static char*
GPHDUri_dup_without_segwork(const char* uri)
{
	char	*segwork;
	char	*no_segwork;

	no_segwork = pstrdup(uri);
	segwork = strstr(no_segwork, segwork_substring);

	/* If segwork_substring was not found,
	 * just return a dup of the string
	 */
	if (segwork != NULL)
	{
		/* back 1 char to include either & or ? */
		--segwork;
		*segwork = 0;
	}

	return no_segwork;
}

