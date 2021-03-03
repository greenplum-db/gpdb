/*-------------------------------------------------------------------------
 *
 * read.c
 *	  routines to convert a string (legal ascii representation of node) back
 *	  to nodes
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/read.c
 *
 * HISTORY
 *	  AUTHOR			DATE			MAJOR EVENT
 *	  Andrew Yu			Nov 2, 1994		file creation
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "common/string.h"
#include "nodes/pg_list.h"
#include "nodes/readfuncs.h"
#include "nodes/value.h"


/* Static state for pg_strtok */
static const char *pg_strtok_ptr = NULL;
static const char *pg_strtok_begin = NULL;                    /*CDB*/

/* State flag that determines how readfuncs.c should treat location fields */
#ifdef WRITE_READ_PARSE_PLAN_TREES
bool		restore_location_fields = false;
#endif

static void nodeReadSkipThru(char closingDelimiter);    /*CDB*/

/*
 * Helper functions for saving current states and pg_strtok() another string
 *
 * External functions like ExtensibleNodeMethods->nodeRead() uses the
 * pg_strtok_ptr information via pg_strtok() but the context could be binary
 * (readfast.c), set_strtok_states() to pg_strtok() the string.
 */
void
save_strtok_states(const char ** save_ptr, const char ** save_begin)
{
	*save_ptr = pg_strtok_ptr;		/* point pg_strtok at the string to read */
	*save_begin = pg_strtok_begin;	/* CDB: save starting position for debug */
}

void
set_strtok_states(const char *ptr, const char *begin)
{
	pg_strtok_ptr = ptr;		/* point pg_strtok at the string to read */
	pg_strtok_begin = begin;	/* CDB: save starting position for debug */
}

/*
 * stringToNode -
 *	  builds a Node tree from its string representation (assumed valid)
 *
 * restore_loc_fields instructs readfuncs.c whether to restore location
 * fields rather than set them to -1.  This is currently only supported
 * in builds with the WRITE_READ_PARSE_PLAN_TREES debugging flag set.
 */
static void *
stringToNodeInternal(const char *str, bool restore_loc_fields)
{
	void	   *retval;
	const char *save_strtok;
    const char *save_begin = pg_strtok_begin;
#ifdef WRITE_READ_PARSE_PLAN_TREES
	bool		save_restore_location_fields;
#endif

	/*
	 * We save and restore the pre-existing state of pg_strtok. This makes the
	 * world safe for re-entrant invocation of stringToNode, without incurring
	 * a lot of notational overhead by having to pass the next-character
	 * pointer around through all the readfuncs.c code.
	 */
	save_strtok = pg_strtok_ptr;

    pg_strtok_ptr = str;		/* point pg_strtok at the string to read */
    pg_strtok_begin = str;      /* CDB: save starting position for debug */

	/*
	 * If enabled, likewise save/restore the location field handling flag.
	 */
#ifdef WRITE_READ_PARSE_PLAN_TREES
	save_restore_location_fields = restore_location_fields;
	restore_location_fields = restore_loc_fields;
#endif

	retval = nodeRead(NULL, 0); /* do the reading */

	pg_strtok_ptr = save_strtok;
    pg_strtok_begin = save_begin;

#ifdef WRITE_READ_PARSE_PLAN_TREES
	restore_location_fields = save_restore_location_fields;
#endif

	return retval;
}

/*
 * Externally visible entry points
 */
void *
stringToNode(const char *str)
{
	return stringToNodeInternal(str, false);
}

#ifdef WRITE_READ_PARSE_PLAN_TREES

void *
stringToNodeWithLocations(const char *str)
{
	return stringToNodeInternal(str, true);
}

#endif


/*****************************************************************************
 *
 * the lisp token parser
 *
 *****************************************************************************/

/*
 * pg_strtok --- retrieve next "token" from a string.
 *
 * Works kinda like strtok, except it never modifies the source string.
 * (Instead of storing nulls into the string, the length of the token
 * is returned to the caller.)
 * Also, the rules about what is a token are hard-wired rather than being
 * configured by passing a set of terminating characters.
 *
 * The string is assumed to have been initialized already by stringToNode.
 *
 * The rules for tokens are:
 *	* Whitespace (space, tab, newline) always separates tokens.
 *	* The characters '(', ')', '{', '}' form individual tokens even
 *	  without any whitespace around them.
 *	* Otherwise, a token is all the characters up to the next whitespace
 *	  or occurrence of one of the four special characters.
 *	* A backslash '\' can be used to quote whitespace or one of the four
 *	  special characters, so that it is treated as a plain token character.
 *	  Backslashes themselves must also be backslashed for consistency.
 *	  Any other character can be, but need not be, backslashed as well.
 *	* If the resulting token is '<>' (with no backslash), it is returned
 *	  as a non-NULL pointer to the token but with length == 0.  Note that
 *	  there is no other way to get a zero-length token.
 *
 * Returns a pointer to the start of the next token, and the length of the
 * token (including any embedded backslashes!) in *length.  If there are
 * no more tokens, NULL and 0 are returned.
 *
 * NOTE: this routine doesn't remove backslashes; the caller must do so
 * if necessary (see "debackslash").
 *
 * NOTE: prior to release 7.0, this routine also had a special case to treat
 * a token starting with '"' as extending to the next '"'.  This code was
 * broken, however, since it would fail to cope with a string containing an
 * embedded '"'.  I have therefore removed this special case, and instead
 * introduced rules for using backslashes to quote characters.  Higher-level
 * code should add backslashes to a string constant to ensure it is treated
 * as a single token.
 */
const char *
pg_strtok(int *length)
{
	const char *local_str;		/* working pointer to string */
	const char *ret_str;		/* start of token to return */

	local_str = pg_strtok_ptr;

	while (*local_str == ' ' || *local_str == '\n' || *local_str == '\t')
		local_str++;

	if (*local_str == '\0')
	{
		*length = 0;
		pg_strtok_ptr = local_str;
		return NULL;			/* no more tokens */
	}

	/*
	 * Now pointing at start of next token.
	 */
	ret_str = local_str;

	if (*local_str == '(' || *local_str == ')' ||
		*local_str == '{' || *local_str == '}')
	{
		/* special 1-character token */
		local_str++;
	}
	else
	{
		/* Normal token, possibly containing backslashes */
		while (*local_str != '\0' &&
			   *local_str != ' ' && *local_str != '\n' &&
			   *local_str != '\t' &&
			   *local_str != '(' && *local_str != ')' &&
			   *local_str != '{' && *local_str != '}')
		{
			if (*local_str == '\\' && local_str[1] != '\0')
				local_str += 2;
			else
				local_str++;
		}
	}

	*length = local_str - ret_str;

	/* Recognize special case for "empty" token */
	if (*length == 2 && ret_str[0] == '<' && ret_str[1] == '>')
		*length = 0;

	pg_strtok_ptr = local_str;

	return ret_str;
}

/*
 * debackslash -
 *	  create a palloc'd string holding the given token.
 *	  any protective backslashes in the token are removed.
 */
char *
debackslash(const char *token, int length)
{
	char	   *result = palloc(length + 1);
	char	   *ptr = result;

	while (length > 0)
	{
		if (*token == '\\' && length > 1)
			token++, length--;
		*ptr++ = *token++;
		length--;
	}
	*ptr = '\0';
	return result;
}

#define RIGHT_PAREN (1000000 + 1)
#define LEFT_PAREN	(1000000 + 2)
#define LEFT_BRACE	(1000000 + 3)
#define OTHER_TOKEN (1000000 + 4)

/*
 * nodeTokenType -
 *	  returns the type of the node token contained in token.
 *	  It returns one of the following valid NodeTags:
 *		T_Integer, T_Float, T_String, T_BitString
 *	  and some of its own:
 *		RIGHT_PAREN, LEFT_PAREN, LEFT_BRACE, OTHER_TOKEN
 *
 *	  Assumption: the ascii representation is legal
 */
static NodeTag
nodeTokenType(const char *token, int length)
{
	NodeTag		retval;
	const char *numptr;
	int			numlen;

	/*
	 * Check if the token is a number
	 */
	numptr = token;
	numlen = length;
	if (*numptr == '+' || *numptr == '-')
		numptr++, numlen--;
	if ((numlen > 0 && isdigit((unsigned char) *numptr)) ||
		(numlen > 1 && *numptr == '.' && isdigit((unsigned char) numptr[1])))
	{
		/*
		 * Yes.  Figure out whether it is integral or float; this requires
		 * both a syntax check and a range check. strtoint() can do both for
		 * us. We know the token will end at a character that strtoint will
		 * stop at, so we do not need to modify the string.
		 */
		char	   *endptr;

		errno = 0;
		(void) strtoint(token, &endptr, 10);
		if (endptr != token + length || errno == ERANGE)
			return T_Float;
		return T_Integer;
	}

	/*
	 * these three cases do not need length checks, since pg_strtok() will
	 * always treat them as single-byte tokens
	 */
	else if (*token == '(')
		retval = LEFT_PAREN;
	else if (*token == ')')
		retval = RIGHT_PAREN;
	else if (*token == '{')
		retval = LEFT_BRACE;
	else if (*token == '"' && length > 1 && token[length - 1] == '"')
		retval = T_String;
	else if (*token == 'b')
		retval = T_BitString;
	else
		retval = OTHER_TOKEN;
	return retval;
}

/*
 * nodeRead -
 *	  Slightly higher-level reader.
 *
 * This routine applies some semantic knowledge on top of the purely
 * lexical tokenizer pg_strtok().   It can read
 *	* Value token nodes (integers, floats, or strings);
 *	* General nodes (via parseNodeString() from readfuncs.c);
 *	* Lists of the above;
 *	* Lists of integers or OIDs.
 * The return value is declared void *, not Node *, to avoid having to
 * cast it explicitly in callers that assign to fields of different types.
 *
 * External callers should always pass NULL/0 for the arguments.  Internally
 * a non-NULL token may be passed when the upper recursion level has already
 * scanned the first token of a node's representation.
 *
 * We assume pg_strtok is already initialized with a string to read (hence
 * this should only be invoked from within a stringToNode operation).
 */
void *
nodeRead(const char *token, int tok_len)
{
	Node	   *result;
	NodeTag		type;

	if (token == NULL)			/* need to read a token? */
	{
		token = pg_strtok(&tok_len);

		if (token == NULL)		/* end of input */
			return NULL;
	}

	type = nodeTokenType(token, tok_len);

	switch ((int) type)
	{
		case LEFT_BRACE:
			result = parseNodeString();
			token = pg_strtok(&tok_len);

            /*
             * CDB: Check for extra fields left over following the ones that
             * were consumed by the node reader function.  If this tree was
             * read from the catalog, it might have been stored by a future
             * release which may have added fields that we don't know about.
             */
            while (token &&
                   token[0] == ':')
            {
                /*
                 * Check for special :prereq tag that a future release may have
                 * inserted to tell us that the node's semantics are not
                 * downward compatible.  The node reader function should have
                 * consumed any such tags for features that it supports.  If a
                 * :prereq is left to be seen here, that means its feature isn't
                 * implemented in this release and we must reject the statement.
                 * The tag should be followed by a concise feature name or
                 * release id that can be shown to the user in an error message.
                 */
                if (tok_len == 7 &&
                    memcmp(token, ":prereq", 7) == 0)
                {
        			token = pg_strtok(&tok_len);
                    token = debackslash(token, tok_len);
                    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                    errmsg("This operation requires a feature "
                                           "called \"%.*s\" which is not "
                                           "supported in this version of %s.", 
                                           tok_len, token, PACKAGE_NAME)
                            ));
                }

                /*
                 * Other extra fields can safely be ignored.  They are assumed
                 * downward compatible unless a :prereq tag tells us otherwise.
                 */
                nodeReadSkip();
                ereport(DEBUG2, (errmsg("nodeRead: unknown option '%.*s' ignored",
                                        tok_len, token),
                                 errdetail("Skipped '%.*s' at offset %d in %s",
                                           (int)(pg_strtok_ptr - token), token,
                                           (int)(token - pg_strtok_begin),
                                           pg_strtok_begin)
                        ));
                token = pg_strtok(&tok_len);
            }

			if (token == NULL )
				elog(ERROR, "did not find '}' at end of input node");
			if (token[0] != '}')
				elog(ERROR, "did not find '}' at end of input node, instead found %s",token);
			break;
		case LEFT_PAREN:
			{
				List	   *l = NIL;

				/*----------
				 * Could be an integer list:	(i int int ...)
				 * or an OID list:				(o int int ...)
				 * or a list of nodes/values:	(node node ...)
				 *----------
				 */
				token = pg_strtok(&tok_len);
				if (token == NULL)
					elog(ERROR, "unterminated List structure");
				if (tok_len == 1 && token[0] == 'i')
				{
					/* List of integers */
					for (;;)
					{
						int			val;
						char	   *endptr;

						token = pg_strtok(&tok_len);
						if (token == NULL)
							elog(ERROR, "unterminated List structure");
						if (token[0] == ')')
							break;
						val = (int) strtol(token, &endptr, 10);
						if (endptr != token + tok_len)
							elog(ERROR, "unrecognized integer: \"%.*s\"",
								 tok_len, token);
						l = lappend_int(l, val);
					}
				}
				else if (tok_len == 1 && token[0] == 'o')
				{
					/* List of OIDs */
					for (;;)
					{
						Oid			val;
						char	   *endptr;

						token = pg_strtok(&tok_len);
						if (token == NULL)
							elog(ERROR, "unterminated List structure");
						if (token[0] == ')')
							break;
						val = (Oid) strtoul(token, &endptr, 10);
						if (endptr != token + tok_len)
							elog(ERROR, "unrecognized OID: \"%.*s\"",
								 tok_len, token);
						l = lappend_oid(l, val);
					}
				}
				else
				{
					/* List of other node types */
					for (;;)
					{
						/* We have already scanned next token... */
						if (token[0] == ')')
							break;
						l = lappend(l, nodeRead(token, tok_len));
						token = pg_strtok(&tok_len);
						if (token == NULL)
							elog(ERROR, "unterminated List structure");
					}
				}
				result = (Node *) l;
				break;
			}
		case RIGHT_PAREN:
			elog(ERROR, "unexpected right parenthesis");
			result = NULL;		/* keep compiler happy */
			break;
		case OTHER_TOKEN:
			if (tok_len == 0)
			{
				/* must be "<>" --- represents a null pointer */
				result = NULL;
			}
			else
			{
				elog(ERROR, "unrecognized token: \"%.*s\"", tok_len, token);
				result = NULL;	/* keep compiler happy */
			}
			break;
		case T_Integer:

			/*
			 * we know that the token terminates on a char atoi will stop at
			 */
			result = (Node *) makeInteger(atoi(token));
			break;
		case T_Float:
			{
				char	   *fval = (char *) palloc(tok_len + 1);

				memcpy(fval, token, tok_len);
				fval[tok_len] = '\0';
				result = (Node *) makeFloat(fval);
			}
			break;
		case T_String:
			/* need to remove leading and trailing quotes, and backslashes */
			result = (Node *) makeString(debackslash(token + 1, tok_len - 2));
			break;
		case T_BitString:
			{
				char	   *val = palloc(tok_len);

				/* skip leading 'b' */
				memcpy(val, token + 1, tok_len - 1);
				val[tok_len - 1] = '\0';
				result = (Node *) makeBitString(val);
				break;
			}
		default:
			elog(ERROR, "unrecognized node type: %d", (int) type);
			result = NULL;		/* keep compiler happy */
			break;
	}

	return (void *) result;
}


/*
 * nodeReadSkip
 *    Skips next item (a token, list or subtree).
 */
void
nodeReadSkip(void)
{
    int     tok_len;
    const char *token = pg_strtok(&tok_len);

    if (!token)
    {
        elog(ERROR, "did not find expected token");
        return;                 /* not reached */
    }

    switch (*token)
    {
        case '{':
            nodeReadSkipThru('}');
            break;

        case '(':
            nodeReadSkipThru(')');
            break;

        case '}':
        case ')':
            elog(ERROR, "did not find expected token, instead found %s", token);
            return;             /* not reached */

        default:
            break;
    }
}                               /* nodeReadSkip */


/*
 * nodeReadSkipThru
 *    Skips one or more tokens, lists or subtrees up to and including
 *    the specified matching delimiter.
 */
void
nodeReadSkipThru(char closingDelimiter)
{
    for (;;)
    {
        int     tok_len;
        const char   *token = pg_strtok(&tok_len);

        if (!token)
        {
            elog(ERROR, "did not find '%c' as expected", closingDelimiter);
            return;             /* not reached */
        }

        switch (*token)
        {
            case '{':
                nodeReadSkipThru('}');
                break;

            case '(':
                nodeReadSkipThru(')');
                break;

            case '}':
            case ')':
                if (*token != closingDelimiter)
                    elog(ERROR, "did not find '%c' as expected, instead found %s",
                         closingDelimiter, token);
                return;         /* not reached */

            default:
                break;
        }
    }
}                               /* nodeReadSkipThru */


/*
 * pg_strtok_peek_fldname
 *    Peeks at the token that will be returned by the next call to
 *    pg_strtok and returns true if it is, case-sensitively,
 *          :fldname
 */
bool
pg_strtok_peek_fldname(const char *fldname)
{
    const char   *bp = pg_strtok_ptr;
    const char   *cp;

    if (!bp)
        return false;

    /* trim leading whitespace */
    if (*bp <= ' ')
    {
        while (*bp == ' ' || *bp == '\n' || *bp == '\t')
		    bp++;
        pg_strtok_ptr = bp;
    }

    if (*bp != ':')
        return false;

    cp = bp+1;
    while (*fldname != '\0' &&
           *fldname == *cp)
    {
        cp++;
        fldname++;
    }

    if (*fldname != '\0')
        return false;

    if (*cp == ' ' ||
        *cp == '\n' ||
        *cp == '\t' ||
        *cp == '(' ||
        *cp == ')' ||
        *cp == '{' ||
        *cp == '}' ||
        *cp == '\0')
        return true;

    return false;
}                                   /* pg_strtok_peek_fldname */
