/*-------------------------------------------------------------------------
 *
 * cdbsrlz.h
 *	  definitions for plan serialization utilities
 *
 * Copyright (c) 2004-2008, Greenplum inc
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBSRLZ_H
#define CDBSRLZ_H

#include "nodes/nodes.h"

extern char *serializeNode(Node *node, int *size, int *uncompressed_size);
extern Node *deserializeNode(const char *strNode, int size);

#endif   /* CDBSRLZ_H */
