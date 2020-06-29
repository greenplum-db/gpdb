/*-------------------------------------------------------------------------
 *
 * transform.h
 * 	Query transformation routines
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/optimizer/transform.h
 * Author: Siva
 *-------------------------------------------------------------------------
 */

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern Query *normalize_query(Query *query);

#endif /* TRANSFORM_H */
