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
#include "pxffragment.h"
#include "pxfutils.h"
#include "utils/formatting.h"

static const char *PTC_SEP = "://";
static const char *OPTION_SEP = "&";
static const int EMPTY_VALUE_LEN = 2;

/* helper function declarations */
static void GPHDUri_parse_protocol(GPHDUri *uri, char **cursor);
static void GPHDUri_parse_data(GPHDUri *uri, char **cursor);
static void GPHDUri_parse_options(GPHDUri *uri, char **cursor);
static List *GPHDUri_parse_option(char *pair, GPHDUri *uri);
static void GPHDUri_free_options(GPHDUri *uri);
static void GPHDUri_free_fragments(GPHDUri *uri);

/* parseGPHDUri
 *
 * Go over a URI string and parse it into its various components while
 * verifying valid structure given a specific target protocol.
 *
 * URI format:
 *     <protocol name>://<data resource>?<option>&<option>&<...>
 *
 *
 * protocol name    - must be 'pxf'
 * data resource    - data resource (directory name/table name/etc., depending on target)
 * options          - valid options are dependent on the protocol. Each
 *                    option is a key value pair.
 *
 * inputs:
 *         'uri_str'    - the raw uri str
 *
 * returns:
 *         a parsed uri as a GPHDUri structure, or reports a format error.
 */
GPHDUri *
parseGPHDUri(const char *uri_str)
{
	return parseGPHDUriHostPort(uri_str, PxfDefaultHost, PxfDefaultPort);
}

GPHDUri *
parseGPHDUriHostPort(const char *uri_str, const char *host, const int port)
{
	GPHDUri    *uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	uri->host = pstrdup(host);
	StringInfoData portstr;

	initStringInfo(&portstr);
	appendStringInfo(&portstr, "%d", port);
	uri->port = portstr.data;
	uri->uri = pstrdup(uri_str);
	char	   *cursor = uri->uri;

	GPHDUri_parse_protocol(uri, &cursor);
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
	if (uri->protocol)
		pfree(uri->protocol);
	if (uri->host)
		pfree(uri->host);
	if (uri->port)
		pfree(uri->port);
	if (uri->data)
		pfree(uri->data);
	if (uri->profile)
		pfree(uri->profile);

	GPHDUri_free_fragments(uri);
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
	char	   *start = *cursor;
	char	   *post_ptc = strstr(start, PTC_SEP);

	if (!post_ptc)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s", uri->uri)));

	uri->protocol = pnstrdup(start, post_ptc - start);
	if (!IS_PXF_URI(uri->uri))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: unsupported protocol '%s'", uri->uri, uri->protocol)));

	/* set cursor to new position and return */
	*cursor = post_ptc + strlen(PTC_SEP);
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
	char	   *start = *cursor;
	size_t		data_len = strlen(start);

	/*
	 * If there exists an 'options' section, the data section length is from
	 * start point to options section point. Otherwise, the data section
	 * length is the remaining string length from start.
	 */
	char	   *options_section = strrchr(start, '?');

	if (options_section)
		data_len = options_section - start;

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
	char	   *dup = pstrdup(*cursor);
	char	   *start = dup;

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
	char	   *ctx;

	for (char *pair = strtok_r(start, OPTION_SEP, &ctx); pair; pair = strtok_r(NULL, OPTION_SEP, &ctx))
		uri->options = GPHDUri_parse_option(pair, uri);

	pfree(dup);
}

/*
 * Parse an option in the form:
 * <key>=<value>
 * to OptionData object (key and value).
 */
static List *
GPHDUri_parse_option(char *pair, GPHDUri *uri)
{
	OptionData *option_data = palloc0(sizeof(OptionData));
	char	   *sep = strchr(pair, '=');

	if (sep == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing '='", uri->uri, pair)));

	if (strchr(sep + 1, '=') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' contains duplicate '='", uri->uri, pair)));

	int			key_len = sep - pair;

	if (key_len == 0)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing key before '='", uri->uri, pair)));

	int			value_len = strlen(pair) - key_len + 1;

	if (value_len == EMPTY_VALUE_LEN)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: option '%s' missing value after '='", uri->uri, pair)));

	option_data->key = pnstrdup(pair, key_len);
	option_data->value = pnstrdup(sep + 1, value_len);

	char	   *x_gp_key = normalize_key_name(option_data->key);

	if (strcmp(x_gp_key, "X-GP-PROFILE") == 0)
		uri->profile = pstrdup(option_data->value);
	pfree(x_gp_key);

	return lappend(uri->options, option_data);
}

/*
 * Free options list
 */
static void
GPHDUri_free_options(GPHDUri *uri)
{
	ListCell   *option = NULL;

	foreach(option, uri->options)
	{
		OptionData *data = (OptionData *) lfirst(option);

		pfree(data->key);
		pfree(data->value);
		pfree(data);
	}
	list_free(uri->options);
	uri->options = NIL;
}

/*
 * Free fragments list
 */
static void
GPHDUri_free_fragments(GPHDUri *uri)
{
	ListCell   *fragment = NULL;

	foreach(fragment, uri->fragments)
	{
		FragmentData *data = (FragmentData *) lfirst(fragment);

		free_fragment(data);
	}
	list_free(uri->fragments);
	uri->fragments = NIL;
}

/*
 * GPHDUri_opt_exists
 *
 * Returns 0 if the key was found with a non empty value, -1 otherwise.
 */
bool
GPHDUri_opt_exists(GPHDUri *uri, char *key)
{
	ListCell   *item;

	foreach(item, uri->options)
	{
		OptionData *data = (OptionData *) lfirst(item);

		if (data != NULL && pg_strcasecmp(data->key, key) == 0 && strlen(data->value) > 0)
			return true;
	}
	return false;
}

/*
 * GPHDUri_verify_no_duplicate_options
 * verify each option appears only once (case insensitive)
 */
void
GPHDUri_verify_no_duplicate_options(GPHDUri *uri)
{
	ListCell   *option = NULL;
	List	   *duplicateKeys = NIL;
	List	   *previousKeys = NIL;

	foreach(option, uri->options)
	{
		OptionData *data = (OptionData *) lfirst(option);
		Value	   *key = makeString(str_toupper(data->key, strlen(data->key)));

		if (!list_member(previousKeys, key))
			previousKeys = lappend(previousKeys, key);
		else if (!list_member(duplicateKeys, key))
			duplicateKeys = lappend(duplicateKeys, key);
	}

	if (duplicateKeys && duplicateKeys->length > 0)
	{
		ListCell   *key = NULL;
		StringInfoData duplicates;

		initStringInfo(&duplicates);
		foreach(key, duplicateKeys)
			appendStringInfo(&duplicates, "%s, ", strVal((Value *) lfirst(key)));
		/* omit trailing ', ' */
		truncateStringInfo(&duplicates, duplicates.len - strlen(", "));

		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: Duplicate option(s): %s", uri->uri, duplicates.data)));
		pfree(duplicates.data);
	}
	list_free_deep(duplicateKeys);
	list_free_deep(previousKeys);
}

/*
 * GPHDUri_verify_core_options_exist
 * This function is given a list of core options to verify their existence.
 */
void
GPHDUri_verify_core_options_exist(GPHDUri *uri, List *coreOptions)
{
	ListCell   *coreOption = NULL;
	StringInfoData missing;

	initStringInfo(&missing);

	foreach(coreOption, coreOptions)
	{
		bool		optExist = false;
		ListCell   *option = NULL;

		foreach(option, uri->options)
		{
			char	   *key = ((OptionData *) lfirst(option))->key;

			if (pg_strcasecmp(key, lfirst(coreOption)) == 0)
			{
				optExist = true;
				break;
			}
		}
		if (!optExist)
			appendStringInfo(&missing, "%s and ", (char *) lfirst(coreOption));
	}

	if (missing.len > 0)
	{
		/* omit trailing ' and ' */
		truncateStringInfo(&missing, missing.len - strlen(" and "));
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("Invalid URI %s: %s option(s) missing", uri->uri, missing.data)));
	}
	pfree(missing.data);
}
