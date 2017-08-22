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

#include "libchurl.h"
#include "pxfuriparser.h"
#include "pxfutils.h"
#include "utils/formatting.h"

static const char* SEGWORK_SUBSTRING = "segwork=";
static const char* PTC_SEP = "://";
static const char* OPTION_SEP = "&";
static const char  SEGWORK_SEPARATOR = '@';
static const int   EMPTY_VALUE_LEN = 2;

/* helper function declarations */
static void  GPHDUri_parse_protocol(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_cluster(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_data(GPHDUri *uri, char **cursor);
static void  GPHDUri_parse_options(GPHDUri *uri, char **cursor);
static List* GPHDUri_parse_option(char* pair, GPHDUri *uri);
static void  GPHDUri_free_options(GPHDUri *uri);
static void  GPHDUri_parse_segwork(GPHDUri *uri, const char *uri_str);
static List* GPHDUri_parse_fragment(char* fragment, List* fragments);
static void  GPHDUri_free_fragments(GPHDUri *uri);
static char* GPHDUri_dup_without_segwork(const char* uri);

/* parseGPHDUri
 *
 * Go over a URI string and parse it into its various components while
 * verifying valid structure given a specific target protocol.
 *
 * URI format:
 *     <protocol name>://<cluster>/<data>?<option>&<option>&<...>&segwork=<segwork>
 *
 *
 * protocol name    - must be 'pxf'
 * cluster          - cluster name
 * data             - data path (directory name/table name/etc., depending on target)
 * options          - valid options are dependent on the protocol. Each
 *                    option is a key value pair.
 *
 * inputs:
 *         'uri_str'    - the raw uri str
 *
 * returns:
 *         a parsed uri as a GPHDUri structure, or reports a format error.
 */
GPHDUri*
parseGPHDUri(const char *uri_str)
{
    return parseGPHDUriHostPort(uri_str, PxfDefaultHost, PxfDefaultPort);
}

GPHDUri*
parseGPHDUriHostPort(const char *uri_str, const char *host, const int port)
{
    GPHDUri *uri = (GPHDUri *)palloc0(sizeof(GPHDUri));

    uri->host = pstrdup(host);
    StringInfoData portstr;
    initStringInfo(&portstr);
    appendStringInfo(&portstr, "%d", port);
    uri->port = portstr.data;
    uri->uri = GPHDUri_dup_without_segwork(uri_str);
    char *cursor = uri->uri;

    GPHDUri_parse_segwork(uri, uri_str);
    GPHDUri_parse_protocol(uri, &cursor);
    GPHDUri_parse_cluster(uri, &cursor);
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
    if (uri->cluster)
        pfree(uri->cluster);
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
    char *start = *cursor;
    char *post_ptc = strstr(start, PTC_SEP);
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
 * GPHDUri_parse_cluster
 *
 * Parse the cluster section of the URI which is passed down
 * in 'cursor', having 'cursor' point at the current string
 * location.
 *
 * See parseGPHDUri header for URI structure description.
 */
static void
GPHDUri_parse_cluster(GPHDUri *uri, char **cursor)
{
    char *start = *cursor;
    char *end = strchr(start, '/');

    if (*start == '/' || !end)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: missing cluster section", uri->uri)));

    uri->cluster = pnstrdup(start, end - start);
    *cursor = ++end;
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
    char *start = *cursor;
    size_t data_len = strlen(start);

    /*
     * If there exists an 'options' section, the data section length
     * is from start point to options section point. Otherwise, the
     * data section length is the remaining string length from start.
     */
    char *options_section = strrchr(start, '?');
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
    char *dup = pstrdup(*cursor);
    char *start = dup;

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
    char *ctx;
    for (char *pair = strtok_r(start, OPTION_SEP, &ctx); pair; pair = strtok_r(NULL, OPTION_SEP, &ctx))
        uri->options = GPHDUri_parse_option(pair, uri);

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
    OptionData* option_data = palloc0(sizeof(OptionData));
    char *sep = strchr(pair, '=');
    if (sep == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: option '%s' missing '='", uri->uri, pair)));

    if (strchr(sep + 1, '=') != NULL)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: option '%s' contains duplicate '='", uri->uri, pair)));

    int key_len = sep - pair;
    if (key_len == 0)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: option '%s' missing key before '='", uri->uri, pair)));

    int value_len = strlen(pair) - key_len + 1;
    if (value_len == EMPTY_VALUE_LEN)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: option '%s' missing value after '='", uri->uri, pair)));

    option_data->key = pnstrdup(pair, key_len);
    option_data->value = pnstrdup(sep + 1, value_len);

    char *x_gp_key = normalize_key_name(option_data->key);
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
    char *size_end, *sizestr, *fragment;
    /* skip segwork= */
    char *segwork = strstr(uri_str, SEGWORK_SUBSTRING);
    if (segwork == NULL)
        return;
    segwork += strlen(SEGWORK_SUBSTRING);

    /* read next segment. each segment is prefixed with its size. */
    int fragment_size, count = 0;
    while (segwork && strlen(segwork))
    {
        /* expect size */
        size_end = strchr(segwork, SEGWORK_SEPARATOR);
        Assert(size_end != NULL);
        sizestr = pnstrdup(segwork, size_end - segwork);
        fragment_size = atoi(sizestr);
        pfree(sizestr);
        segwork = size_end + 1; /* skip the size field */
        Assert(fragment_size <= strlen(segwork));

        fragment = pnstrdup(segwork, fragment_size);
        elog(DEBUG2, "GPHDUri_parse_segwork: fragment #%d, size %d", count, fragment_size);
        uri->fragments = GPHDUri_parse_fragment(fragment, uri->fragments);
        segwork += fragment_size;
        pfree(fragment);
        count++;
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
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is null.")));

    FragmentData* fragment_data = palloc0(sizeof(FragmentData));
    StringInfoData authority_formatter;
    initStringInfo(&authority_formatter);
    char *dup_frag = pstrdup(fragment);
    char *value_start = dup_frag;

    /* expect ip */
    char *value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    appendStringInfo(&authority_formatter, "%s:", value_start);
    value_start = value_end + 1;

    /* expect port */
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    appendStringInfoString(&authority_formatter, value_start);
    fragment_data->authority = authority_formatter.data;
    value_start = value_end + 1;

    /* expect source name */
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    fragment_data->source_name = pstrdup(value_start);
    value_start = value_end + 1;

    /* expect index */
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    fragment_data->index = pstrdup(value_start);
    value_start = value_end + 1;

    /* expect fragment metadata */
    Assert(value_start);
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    fragment_data->fragment_md = pstrdup(value_start);
    value_start = value_end + 1;

    /* expect user data */
    Assert(value_start);
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    fragment_data->user_data = pstrdup(value_start);
    value_start = value_end + 1;

    /* expect for profile */
    Assert(value_start);
    value_end = strchr(value_start, SEGWORK_SEPARATOR);
    if (value_end == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in GPHDUri_parse_fragment. Fragment string is invalid.")));
    *value_end = '\0';
    if (strlen(value_start) > 0)
        fragment_data->profile = pstrdup(value_start);

    pfree(dup_frag);
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
    ListCell *fragment;
    foreach(fragment, uri->fragments)
        GPHDUri_free_fragment((FragmentData*)lfirst(fragment));
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
    char *segwork = strstr(uri, SEGWORK_SUBSTRING);
    if (segwork != NULL) {
        segwork--; /* Discard the last character */
        return pnstrdup(uri, segwork - uri);
    }
    /* segwork_substring was not found */
    return pstrdup(uri);
}

/*
 * GPHDUri_opt_exists
 *
 * Returns 0 if the key was found with a non empty value, -1 otherwise.
 */
bool
GPHDUri_opt_exists(GPHDUri *uri, char *key)
{
    ListCell *item;
    foreach(item, uri->options)
    {
        OptionData *data = (OptionData*)lfirst(item);
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
    ListCell *option = NULL;
    List *duplicateKeys = NIL;
    List *previousKeys = NIL;

    foreach(option, uri->options)
    {
        OptionData *data = (OptionData*)lfirst(option);
        Value *key = makeString(str_toupper(data->key, strlen(data->key)));
        if (!list_member(previousKeys, key))
            previousKeys = lappend(previousKeys, key);
        else if (!list_member(duplicateKeys, key))
            duplicateKeys = lappend(duplicateKeys, key);
    }

    if (duplicateKeys && duplicateKeys->length > 0)
    {
        ListCell *key = NULL;
        StringInfoData duplicates;
        initStringInfo(&duplicates);
        foreach(key, duplicateKeys)
            appendStringInfo(&duplicates, "%s, ", strVal((Value*)lfirst(key)));
        //omit trailing ', '
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
    ListCell *coreOption = NULL;
    StringInfoData missing;
    initStringInfo(&missing);

    foreach(coreOption, coreOptions)
    {
        bool optExist = false;
        ListCell *option = NULL;
        foreach(option, uri->options)
        {
            char *key = ((OptionData*)lfirst(option))->key;
            if (pg_strcasecmp(key, lfirst(coreOption)) == 0)
            {
                optExist = true;
                break;
            }
        }
        if (!optExist)
        appendStringInfo(&missing, "%s and ", (char*)lfirst(coreOption));
    }

    if (missing.len > 0)
    {
        truncateStringInfo(&missing, missing.len - strlen(" and ")); //omit trailing ' and '
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: %s option(s) missing", uri->uri, missing.data)));
    }
    pfree(missing.data);
}

/*
 * GPHDUri_verify_cluster_exist
 * This function is given the name of the cluster to verify their existence.
 */
void
GPHDUri_verify_cluster_exists(GPHDUri *uri, char* cluster)
{
    if (pg_strcasecmp(uri->cluster, cluster) != 0)
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("Invalid URI %s: CLUSTER NAME %s not found", uri->uri, cluster)));
}
