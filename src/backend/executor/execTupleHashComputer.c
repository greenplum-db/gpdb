/*-------------------------------------------------------------------------
 *
 *  execTupleHashComputer.c
 *  	Computes a hash value for a tuple, given a set of hashable columns
 *  	and the data type of each column
 *
 * Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "executor/execTupleHashComputer.h"
#include "utils/lsyscache.h"
#include "parser/parse_expr.h"
#include "utils/murmurhash3.h"

#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include "cdb/cdbhash.h"

static HashType CalculateHash(TupleHashComputer *tupleHashComputer, int hashCol, Datum datum);
static HashType hash_1_byte(void *buf, size_t len, int seed);
static HashType hash_2_bytes(void *buf, size_t len, int seed);
static HashType hash_4_bytes(void *buf, size_t len, int seed);
static HashType hash_8_bytes(void *buf, size_t len, int seed);
static HashType hash_n_bytes(void *buf, size_t len, int seed);

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/* Factory method to create a TupleHashComputer */
TupleHashComputer*
TupleHashComputer_Create(List *hashKeyExprs, List *hashOperators, bool shouldHashNull, ExprContext *exprContext)
{
	Assert(list_length(hashKeyExprs) == list_length(hashOperators));
	Assert(NULL != exprContext);

	int numHashKeys = list_length(hashKeyExprs);

	ExprState **hashKeyExprArray = palloc(sizeof(ExprState *) * numHashKeys);
	FmgrInfo *legacyHashFunctions = palloc(sizeof(FmgrInfo) * numHashKeys);
	bool *isStrictOperator = palloc0(sizeof(bool) * numHashKeys);

	Oid *typeOids = palloc0(sizeof(Oid) * numHashKeys);
	int16 *typeLength = palloc0(sizeof(int16) * numHashKeys);

	ListCell *hashKeyCell = NULL;
	ListCell *hashOpCell = NULL;

	int cellNumber = 0;
	forboth(hashKeyCell, hashKeyExprs, hashOpCell, hashOperators)
	{
		ExprState  *hashKeyExpr = (ExprState *) lfirst(hashKeyCell);
		hashKeyExprArray[cellNumber] = hashKeyExpr;

		Oid hashOperatorOid = (Oid) lfirst_oid(hashOpCell);
		Assert(OidIsValid(hashOperatorOid));

		Oid hashFunctionOid = get_op_hash_function(hashOperatorOid);
		Assert(OidIsValid(hashFunctionOid));
		fmgr_info(hashFunctionOid, &legacyHashFunctions[cellNumber]);

		isStrictOperator[cellNumber] = op_strict(hashOperatorOid);

		Oid firstArgTypeOid = exprType((Node *)hashKeyExpr->expr);
		int16 firstArgTypeLen = get_typlen(firstArgTypeOid);
		typeOids[cellNumber] = firstArgTypeOid;
		typeLength[cellNumber] = firstArgTypeLen;

		cellNumber++;
	}
	Assert(cellNumber == numHashKeys);

	TupleHashComputer *tupleHashComputer = palloc0(sizeof(TupleHashComputer));
	tupleHashComputer->numHashKeys = numHashKeys;
	tupleHashComputer->hashKeyExprs = hashKeyExprArray;
	tupleHashComputer->legacyHashFunctions = legacyHashFunctions;
	tupleHashComputer->isStrictOperator = isStrictOperator;
	tupleHashComputer->typeOids = typeOids;
	tupleHashComputer->typeLength = typeLength;
	tupleHashComputer->shouldHashNull = shouldHashNull;
	tupleHashComputer->exprContext = exprContext;

	return tupleHashComputer;
}

/*
 * API to begin the process of computing hash values by a sub-join. Succeeds only if
 * no other sub-join is currently using this tupleHashComputer to compute hash values
 */
void
TupleHashComputer_Begin(TupleHashComputer *tupleHashComputer, void *requester, int seed)
{
	if (tupleHashComputer->requester != NULL && tupleHashComputer->requester != requester)
	{
		elog(ERROR, "Cannot initialize hash computer");
	}

	tupleHashComputer->requester = requester;
	tupleHashComputer->seed = seed;
}

/*
 * Ends the hash computation of the current sub-join
 */
void
TupleHashComputer_End(TupleHashComputer *tupleHashComputer, void *requester)
{
	if (tupleHashComputer->requester == NULL || tupleHashComputer->requester != requester)
	{
		elog(ERROR, "Cannot cleanup hash computer");
	}

	tupleHashComputer->requester = NULL;
	tupleHashComputer->seed = 0;
}

/*
 * hash is computed by evaluating the hashKeyExprs on the exprContext inner/outer tuple.
 * The caller must properly set the exprContext's inner/outer tuple, as required
 */
HashType
TupleHashComputer_ComputeHash(TupleHashComputer *tupleHashComputer, bool *allHashValsAreNull)
{
	bool foundAllNull = true;

	HashType combinedHashValue = 0;
	for (int hkNo = 0; hkNo < tupleHashComputer->numHashKeys; hkNo++)
	{
		ExprState *hashKeyExpr = tupleHashComputer->hashKeyExprs[hkNo];
		bool isNull = false;

		/*
		 * Get the join attribute value of the tuple
		 */
		Datum hashKeyValue = ExecEvalExpr(hashKeyExpr, tupleHashComputer->exprContext, &isNull, NULL);

		if (!isNull)
		{
			combinedHashValue ^= CalculateHash(tupleHashComputer, hkNo, hashKeyValue);
		}

		foundAllNull &= isNull;
	}

	*allHashValsAreNull = foundAllNull;
	return combinedHashValue;
}


/********************************************************************************
 *
 * ******************** PRIVATE METHODS ******************************************
 */

/* Calculate hash for a single column */
static HashType
CalculateHash(TupleHashComputer *tupleHashComputer, int hashCol, Datum datum)
{
	void *bytes = NULL;
	void *toFree = NULL;
	size_t length = 0;

	Oid type = tupleHashComputer->typeOids[hashCol];
	ConvertDatumToHashableBytes(datum, type, &bytes, &length, &toFree);

	HashType hashVal = 0;

	int seed = tupleHashComputer->seed;

	switch (length)
	{
		case 1:
			hashVal = hash_1_byte(bytes, length, seed);
			break;
		case 2:
			hashVal = hash_2_bytes(bytes, length, seed);
			break;
		case 4:
			hashVal = hash_4_bytes(bytes, length, seed);
			break;
		case 8:
			hashVal = hash_8_bytes(bytes, length, seed);
			break;
		default:
			hashVal = hash_n_bytes(bytes, length, seed);
	}

	if(toFree)
	{
		pfree(toFree);
	}

	return hashVal;
}

/* Hash function for data types of length 1 byte */
static HashType
hash_1_byte(void *buf, size_t len, int seed)
{
	if (0 == seed)
	{
		return ~((uint32) *(char *)buf);
	}

	HashType out[2];
	MurmurHash3(buf, len, seed, out);

	return out[0];

}

/* Hash function for data types of length 2 bytes */
static HashType
hash_2_bytes(void *buf, size_t len, int seed)
{
	if (0 == seed)
	{
		return ~((uint32) *(uint16 *)buf);
	}

	HashType out[2];
	MurmurHash3(buf, len, seed, out);

	return out[0];
}

/* Hash function for data types of length 4 bytes */
static HashType
hash_4_bytes(void *buf, size_t len, int seed)
{
	if (0 == seed)
	{
		return ~((uint32) *(uint32 *)buf);
	}

	HashType out[2];
	MurmurHash3(buf, len, seed, out);

	return out[0];
}

/* Hash function for data types of length 8 bytes */
static HashType
hash_8_bytes(void *buf, size_t len, int seed)
{
	if (0 == seed)
	{
		int64		val = *(int64 *)buf;
		uint32		loHalf = (uint32) val;
		uint32		hiHalf = (uint32) (val >> 32);

		loHalf ^= (val >= 0) ? hiHalf : ~hiHalf;

		return (~loHalf);
	}

	HashType out[2];
	MurmurHash3(buf, len, seed, out);

	return out[0];
}

static HashType
hash_n_bytes(void *buf, size_t len, int seed)
{
	HashType out[2];
	MurmurHash3(buf, len, seed, out);

	return out[0];
}
