/*-------------------------------------------------------------------------
 *
 * cdbsrlz.c
 *	  Serialize a PostgreSQL sequential plan tree.
 *
 * Copyright (c) 2004-2008, Greenplum inc
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbsrlz.h"
#include <math.h>
#include "miscadmin.h"
#include "nodes/print.h"
#include "optimizer/clauses.h"
#include "regex/regex.h"
#include "utils/guc.h"
#include "utils/memaccounting.h"
#include "utils/zlib_wrapper.h"

static char *compress_string(const char *src, int uncompressed_size, int *size);
static char *uncompress_string(const char *src, int size, int * uncompressed_len);

/*
 * serializeNode -
 * This is used on the query dispatcher to serialize Plan and Query Trees for
 * dispatching to qExecs.
 * The returned string is palloc'ed in the current memory context.
 */
char *
serializeNode(Node *node, int *size, int *uncompressed_size_out)
{
	char	   *pszNode;
	char	   *sNode;
	int		   uncompressed_size;

	Assert(node != NULL);
	Assert(size != NULL);
	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Serializer));
	{
		pszNode = nodeToBinaryStringFast(node, &uncompressed_size);
		Assert(pszNode != NULL);
	
		if (NULL != uncompressed_size_out)
		{
			*uncompressed_size_out = uncompressed_size;
		}
		sNode = compress_string(pszNode, uncompressed_size, size);
		pfree(pszNode);

		if (DEBUG5 >= log_min_messages)
		{
			Node * newnode = NULL;
			PG_TRY();
			{
				newnode = deserializeNode(sNode, *size);
			}
			PG_CATCH();
			{
				elog_node_display(DEBUG5, "Before serialization", node, true);
				PG_RE_THROW();
			}
			PG_END_TRY();

			/* Some plans guarantee these differences (see serialization
			 * of plan nodes -- they avoid sending QD-only info out) */
			if (strcmp(nodeToString(node), nodeToString(newnode)) != 0)
			{
				elog_node_display(DEBUG5, "Before serialization", node, true);

				elog_node_display(DEBUG5, "After deserialization", newnode, true);
			}
		}
	}
	END_MEMORY_ACCOUNT();

	return sNode;
}

/*
 * deserializeNode -
 * This is used on the qExecs to deserialize serialized Plan and Query Trees
 * received from the dispatcher.
 * The returned node is palloc'ed in the current memory context.
 */
Node *
deserializeNode(const char *strNode, int size)
{
	char		*sNode;
	Node		*node;
	int 		uncompressed_len;

	Assert(strNode != NULL);

	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Deserializer));
	{
		sNode = uncompress_string(strNode, size, &uncompressed_len);

		Assert(sNode != NULL);

		node = readNodeFromBinaryString(sNode, uncompressed_len);
	
		pfree(sNode);
	}
	END_MEMORY_ACCOUNT();

	return node;
}

/*
 * Compress a (binary) string using zlib.
 * 
 * returns the compressed data and the size of the compressed data.
 */
static char *
compress_string(const char *src, int uncompressed_size, int *size)
{
	int level = 3;
	unsigned long compressed_size;
	int status;

	Bytef * result;

	Assert(size!=NULL);
	
	if (src == NULL)
	{
		*size = 0;
		return NULL;
	}
	
	compressed_size = gp_compressBound(uncompressed_size);  /* worst case */
	
	result = palloc(compressed_size + sizeof(int));
	memcpy(result, &uncompressed_size, sizeof(int)); 		/* save the original length */
	
	status = gp_compress2(result+sizeof(int), &compressed_size, (Bytef *)src, uncompressed_size, level);
	if (status != Z_OK)
		elog(ERROR,"Compression failed: %s (errno=%d) uncompressed len %d, compressed %d",
			 zError(status), status, uncompressed_size, (int)compressed_size);
		
	*size = compressed_size + sizeof(int);
	elog(DEBUG2,"Compressed from %d to %d ", uncompressed_size, *size);

	return (char *)result;
}

/*
 * Uncompress the binary string 
 */
static char *
uncompress_string(const char *src, int size, int *uncompressed_len)
{
	Bytef * result;
	unsigned long resultlen;
	int status;
	*uncompressed_len = 0;
	
	if (src==NULL)
		return NULL;
		
	Assert(size >= sizeof(int));
		
	memcpy(uncompressed_len,src, sizeof(int));
	
	resultlen = *uncompressed_len;
	result = palloc(resultlen);
		
	status = gp_uncompress(result, &resultlen, (Bytef *)(src+sizeof(int)), size-sizeof(int));
	if (status != Z_OK)
		elog(ERROR,"Uncompress failed: %s (errno=%d compressed len %d, uncompressed %d)",
			 zError(status), status, size, *uncompressed_len);
		
	return (char *)result;
}
