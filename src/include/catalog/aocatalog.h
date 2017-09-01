/*-------------------------------------------------------------------------
 *
 * aocatalog.h
 *
 * Helper function to support the creation of
 * append-only auxiliary relation as block directories and visimaps.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/aocatalog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOCATALOG_H
#define AOCATALOG_H

#include "postgres.h"
#include "catalog/heap.h"
#include "catalog/index.h"

extern bool CreateAOAuxiliaryTable(
		Relation rel,
		const char *auxiliaryNamePrefix,
		char relkind,
		TupleDesc tupledesc,
		IndexInfo  *indexInfo,
		Oid	*classObjectId,
		int16 *coloptions);

#endif   /* AOCATALOG_H */
