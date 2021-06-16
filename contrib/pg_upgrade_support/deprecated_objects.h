/*
 * deprecated_objects.h
 *
 * Portions Copyright (c) 2021-Present Pivotal Software, Inc.
 */

#ifndef DEPRECATED_OBJECTS_H
#define DEPRECATED_OBJECTS_H

typedef struct
{
	Oid reloid;
	int attnum;
} DeprecatedColumnStatic;

typedef struct
{
	const char *relname;
	int attnum;
} DeprecatedColumnDynamic;

typedef struct {
	List *rtableStack;
} DeprecatedColumnsWalkerContext;

extern bool check_node_deprecated_tables_walker(Node *node, void *context);
extern bool check_node_deprecated_columns_walker(Node *node,
												 DeprecatedColumnsWalkerContext *context);

#endif   /* DEPRECATED_OBJECTS_H */
