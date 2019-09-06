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

#include "pxffilters.h"
#include "pxfheaders.h"
#include "access/fileam.h"
#include "catalog/pg_exttable.h"
#include "commands/defrem.h"
#include "utils/timestamp.h"

/* helper function declarations */
static void add_alignment_size_httpheader(CHURL_HEADERS headers);
static void add_tuple_desc_httpheader(CHURL_HEADERS headers, Relation rel);
static void add_location_options_httpheader(CHURL_HEADERS headers, GPHDUri *gphduri);
static char *get_format_name(char fmtcode);
static void add_projection_desc_httpheader(CHURL_HEADERS headers, ProjectionInfo *projInfo, List *qualsAttributes);
static bool add_attnums_from_targetList(Node *node, List *attnums);
static void add_projection_index_header(CHURL_HEADERS pVoid, StringInfoData data, int attno, char number[32]);

/*
 * Add key/value pairs to connection header.
 * These values are the context of the query and used
 * by the remote component.
 */
void
build_http_headers(PxfInputData *input)
{
	extvar_t       ev;
	CHURL_HEADERS  headers    = input->headers;
	GPHDUri        *gphduri   = input->gphduri;
	Relation       rel        = input->rel;
	char           *filterstr = input->filterstr;
	ProjectionInfo *proj_info = input->proj_info;

	if (rel != NULL)
	{
		/* format */
		ExtTableEntry *exttbl = GetExtTableEntry(rel->rd_id);
		ListCell   *option;
		List	   *copyFmtOpts = NIL;

		/* pxf treats CSV as TEXT */
		char *format = get_format_name(exttbl->fmtcode);

		churl_headers_append(headers, "X-GP-FORMAT", format);

		/* Parse fmtOptString here */
		if (fmttype_is_text(exttbl->fmtcode) || fmttype_is_csv(exttbl->fmtcode))
		{
			copyFmtOpts = parseCopyFormatString(rel, exttbl->fmtopts, exttbl->fmtcode);
		}

		/* pass external table's encoding to copy's options */
		copyFmtOpts = appendCopyEncodingOption(copyFmtOpts, exttbl->encoding);

		/* Extract options from the statement node tree */
		foreach(option, copyFmtOpts)
		{
			DefElem    *def = (DefElem *) lfirst(option);
			churl_headers_append(headers, normalize_key_name(def->defname), defGetString(def));
		}

		/* Record fields - name and type of each field */
		add_tuple_desc_httpheader(headers, rel);
	}

	if (proj_info != NULL)
	{
		bool qualsAreSupported = true;
		List *qualsAttributes =
				 extractPxfAttributes(input->quals, &qualsAreSupported);
		/* projection information is incomplete if columns from WHERE clause wasn't extracted */
		/* if any of expressions in WHERE clause is not supported - do not send any projection information at all*/
		if (qualsAreSupported &&
			(qualsAttributes != NIL || list_length(input->quals) == 0))
		{
			add_projection_desc_httpheader(headers, proj_info, qualsAttributes);
		}
		else
		{
			elog(DEBUG2,
				 "Query will not be optimized to use projection information");
		}
	}

	/* GP cluster configuration */
	external_set_env_vars(&ev, gphduri->uri, false, NULL, NULL, false, 0);

	/* make sure that user identity is known and set, otherwise impersonation by PXF will be impossible */
	if (!ev.GP_USER || !ev.GP_USER[0])
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("user identity is unknown")));
	churl_headers_append(headers, "X-GP-USER", ev.GP_USER);

	churl_headers_append(headers, "X-GP-SEGMENT-ID", ev.GP_SEGMENT_ID);
	churl_headers_append(headers, "X-GP-SEGMENT-COUNT", ev.GP_SEGMENT_COUNT);
	churl_headers_append(headers, "X-GP-XID", ev.GP_XID);

	add_alignment_size_httpheader(headers);

	/* headers for uri data */
	churl_headers_append(headers, "X-GP-URL-HOST", gphduri->host);
	churl_headers_append(headers, "X-GP-URL-PORT", gphduri->port);
	churl_headers_append(headers, "X-GP-DATA-DIR", gphduri->data);

	/* location options */
	add_location_options_httpheader(headers, gphduri);

	/* full uri */
	churl_headers_append(headers, "X-GP-URI", gphduri->uri);

	/* filters */
	if (filterstr != NULL)
	{
		churl_headers_append(headers, "X-GP-FILTER", filterstr);
		churl_headers_append(headers, "X-GP-HAS-FILTER", "1");
	}
	else
		churl_headers_append(headers, "X-GP-HAS-FILTER", "0");
}

/* Report alignment size to remote component
 * GPDBWritable uses alignment that has to be the same as
 * in the C code.
 * Since the C code can be compiled for both 32 and 64 bits,
 * the alignment can be either 4 or 8.
 */
static void
add_alignment_size_httpheader(CHURL_HEADERS headers)
{
	char		tmp[sizeof(char *)];

	pg_ltoa(sizeof(char *), tmp);
	churl_headers_append(headers, "X-GP-ALIGNMENT", tmp);
}

/*
 * Report tuple description to remote component
 * Currently, number of attributes, attributes names, types and types modifiers
 * Each attribute has a pair of key/value
 * where X is the number of the attribute
 * X-GP-ATTR-NAMEX - attribute X's name
 * X-GP-ATTR-TYPECODEX - attribute X's type OID (e.g, 16)
 * X-GP-ATTR-TYPENAMEX - attribute X's type name (e.g, "boolean")
 * optional - X-GP-ATTR-TYPEMODX-COUNT - total number of modifier for attribute X
 * optional - X-GP-ATTR-TYPEMODX-Y - attribute X's modifiers Y (types which have precision info, like numeric(p,s))
 */
static void
add_tuple_desc_httpheader(CHURL_HEADERS headers, Relation rel)
{
	char           long_number[sizeof(int32) * 8];
	StringInfoData formatter;
	TupleDesc      tuple;

	initStringInfo(&formatter);

	/* Get tuple description itself */
	tuple = RelationGetDescr(rel);

	/* Convert the number of attributes to a string */
	pg_ltoa(tuple->natts, long_number);
	churl_headers_append(headers, "X-GP-ATTRS", long_number);

	/* Iterate attributes */
	for (int i = 0; i < tuple->natts; ++i)
	{
		/* Add a key/value pair for attribute name */
		resetStringInfo(&formatter);
		appendStringInfo(&formatter, "X-GP-ATTR-NAME%u", i);
		churl_headers_append(headers, formatter.data, tuple->attrs[i]->attname.data);

		/* Add a key/value pair for attribute type */
		resetStringInfo(&formatter);
		appendStringInfo(&formatter, "X-GP-ATTR-TYPECODE%u", i);
		pg_ltoa(tuple->attrs[i]->atttypid, long_number);
		churl_headers_append(headers, formatter.data, long_number);

		/* Add a key/value pair for attribute type name */
		resetStringInfo(&formatter);
		appendStringInfo(&formatter, "X-GP-ATTR-TYPENAME%u", i);
		churl_headers_append(headers, formatter.data, TypeOidGetTypename(tuple->attrs[i]->atttypid));

		/* Add attribute type modifiers if any */
		if (tuple->attrs[i]->atttypmod > -1)
		{
			switch (tuple->attrs[i]->atttypid)
			{
				case NUMERICOID:
					{
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-COUNT", i);
						pg_ltoa(2, long_number);
						churl_headers_append(headers, formatter.data, long_number);


						/* precision */
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-%u", i, 0);
						pg_ltoa((tuple->attrs[i]->atttypmod >> 16) & 0xffff, long_number);
						churl_headers_append(headers, formatter.data, long_number);

						/* scale */
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-%u", i, 1);
						pg_ltoa((tuple->attrs[i]->atttypmod - VARHDRSZ) & 0xffff, long_number);
						churl_headers_append(headers, formatter.data, long_number);
						break;
					}
				case CHAROID:
				case BPCHAROID:
				case VARCHAROID:
					{
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-COUNT", i);
						pg_ltoa(1, long_number);
						churl_headers_append(headers, formatter.data, long_number);

						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-%u", i, 0);
						pg_ltoa((tuple->attrs[i]->atttypmod - VARHDRSZ), long_number);
						churl_headers_append(headers, formatter.data, long_number);
						break;
					}
				case VARBITOID:
				case BITOID:
				case TIMESTAMPOID:
				case TIMESTAMPTZOID:
				case TIMEOID:
				case TIMETZOID:
					{
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-COUNT", i);
						pg_ltoa(1, long_number);
						churl_headers_append(headers, formatter.data, long_number);

						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-%u", i, 0);
						pg_ltoa((tuple->attrs[i]->atttypmod), long_number);
						churl_headers_append(headers, formatter.data, long_number);
						break;
					}
				case INTERVALOID:
					{
						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-COUNT", i);
						pg_ltoa(1, long_number);
						churl_headers_append(headers, formatter.data, long_number);

						resetStringInfo(&formatter);
						appendStringInfo(&formatter, "X-GP-ATTR-TYPEMOD%u-%u", i, 0);
						pg_ltoa(INTERVAL_PRECISION(tuple->attrs[i]->atttypmod), long_number);
						churl_headers_append(headers, formatter.data, long_number);
						break;
					}
				default:
					elog(DEBUG5, "add_tuple_desc_httpheader: unsupported type %d ", tuple->attrs[i]->atttypid);
					break;
			}
		}
	}

	pfree(formatter.data);
}

/*
 * Report projection description to the remote component
 */
static void
add_projection_desc_httpheader(CHURL_HEADERS headers,
							   ProjectionInfo *projInfo,
							   List *qualsAttributes)
{
	int            i;
	int            number;
	int            numberTargetList;
	char           long_number[sizeof(int32) * 8];
	int            *varNumbers = projInfo->pi_varNumbers;
	StringInfoData formatter;

	initStringInfo(&formatter);
	numberTargetList = 0;

	/*
	 * Non-simpleVars are added to the targetlist
	 * we use expression_tree_walker to access attrno information
	 * we do it through a helper function add_attnums_from_targetList
	 */
	if (projInfo->pi_targetlist)
	{
		List     *l = lappend_int(NIL, 0);
		ListCell *lc1;

		foreach(lc1, projInfo->pi_targetlist)
		{
			GenericExprState *gstate = (GenericExprState *) lfirst(lc1);
			add_attnums_from_targetList((Node *) gstate->arg->expr, l);
		}

		foreach(lc1, l)
		{
			int attno = lfirst_int(lc1);
			if (attno > InvalidAttrNumber)
			{
				add_projection_index_header(headers,
											formatter, attno - 1, long_number);
				numberTargetList++;
			}
		}

		list_free(l);
	}

	number = numberTargetList + projInfo->pi_numSimpleVars +
		list_length(qualsAttributes);
	if (number == 0)
		return;

	/* Convert the number of projection columns to a string */
	pg_ltoa(number, long_number);
	churl_headers_append(headers, "X-GP-ATTRS-PROJ", long_number);

	for (i = 0; i < projInfo->pi_numSimpleVars; i++)
	{
		add_projection_index_header(headers,
									formatter, varNumbers[i] - 1, long_number);
	}

	ListCell *attribute = NULL;

	/*
	 * AttrNumbers coming from quals
	 */
	foreach(attribute, qualsAttributes)
	{
		AttrNumber attrNumber = (AttrNumber) lfirst_int(attribute);
		add_projection_index_header(headers,
									formatter, attrNumber, long_number);
	}

	list_free(qualsAttributes);
	pfree(formatter.data);
}

/*
 * Adds the projection index header for the given attno
 */
static void
add_projection_index_header(CHURL_HEADERS headers,
							StringInfoData str,
							int attno,
							char long_number[32])
{
	pg_ltoa(attno, long_number);
	resetStringInfo(&str);
	appendStringInfo(&str, "X-GP-ATTRS-PROJ-IDX");
	churl_headers_append(headers, str.data, long_number);
}

/*
 * The options in the LOCATION statement of "create external table"
 * FRAGMENTER=HdfsDataFragmenter&ACCESSOR=SequenceFileAccessor...
 */
static void
add_location_options_httpheader(CHURL_HEADERS headers, GPHDUri *gphduri)
{
	ListCell   *option = NULL;

	foreach(option, gphduri->options)
	{
		OptionData *data = (OptionData *) lfirst(option);
		char	   *x_gp_key = normalize_key_name(data->key);

		churl_headers_append(headers, x_gp_key, data->value);
		pfree(x_gp_key);
	}
}

/*
 * Converts a character code for the format name into a string of format definition
 */
static char *
get_format_name(char fmtcode)
{
	char	   *formatName = NULL;

	if (fmttype_is_text(fmtcode) || fmttype_is_csv(fmtcode))
	{
		formatName = TextFormatName;
	}
	else if (fmttype_is_custom(fmtcode))
	{
		formatName = GpdbWritableFormatName;
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("unable to get format name for format code: %c",
						fmtcode)));
	}

	return formatName;
}

/*
 * Gets a list of attnums from the given Node
 * it uses expression_tree_walker to recursively
 * get the list
 */
static bool
add_attnums_from_targetList(Node *node, List *attnums)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var        *variable = (Var *) node;
		AttrNumber attnum    = variable->varattno;

		lappend_int(attnums, attnum);
		return false;
	}

	/*
	 * Don't examine the arguments or filters of Aggrefs or WindowFuncs,
	 * because those do not represent expressions to be evaluated within the
	 * overall targetlist's econtext.
	 */
	if (IsA(node, Aggref))
		return false;
	if (IsA(node, WindowFunc))
		return false;
	return expression_tree_walker(node,
								  add_attnums_from_targetList,
								  (void *) attnums);
}
