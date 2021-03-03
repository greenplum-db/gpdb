/*-------------------------------------------------------------------------
 *
 * gdddetectorpriv.h
 *	  Global DeadLock Detector - Detector Algorithm Private Structs
 *
 *
 * Copyright (c) 2019-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef GDDDETECTORPRIV_H
#define GDDDETECTORPRIV_H

#include "gdddetector.h"	/* for the typedefs of the structs */

/***************************************************************************/

/*
 * Safe list foreach which supports in-place deletion.
 */
#define gdd_list_foreach_safe(iter, _list) \
	for ((iter).list = (_list), \
		 (iter).prev = NULL, \
		 (iter).cell = list_head((iter).list); \
		 (iter).cell != NULL; \
		 (iter).prev = (iter).cell, \
		 (iter).cell = (iter).cell ? lnext((iter).cell) : list_head((iter).list))

/*
 * Helper functions to get the information stored in iter.
 */
#define gdd_list_iter_get_list(iter) ((iter).list)
#define gdd_list_iter_get_cell(iter) ((iter).cell)
#define gdd_list_iter_get_prev(iter) ((iter).prev)

/*
 * Helper function to get the current pointer at iter.
 */
#define gdd_list_iter_get_ptr(iter) (lfirst((iter).cell))

/*
 * In-place delete the list cell at iter.
 * By using this you must re-get the list via gdd_list_iter_get_list().
 */
#define gdd_list_iter_delete(iter) do \
{ \
	(iter).list = list_delete_cell((iter).list, (iter).cell, (iter).prev); \
	(iter).prev = (iter).list != NIL ? (iter).prev : NULL; \
	(iter).cell = (iter).prev; \
} while (0)


/*
 * Safe map foreach which supports in-place deletion.
 */
#define gdd_map_foreach_safe(iter, _map) \
	for ((iter).map = (_map), (iter).pos = 0; \
		 (iter).pos < (iter).map->length; \
		 (iter).pos++)

/*
 * Get current pointer at iter.
 */
#define gdd_map_iter_get_ptr(iter) \
	((iter).map->pairs[(iter).pos].ptr)

/*
 * In-place delete the <k,v> pair at iter.
 */
#define gdd_map_iter_delete(iter) \
	(iter).map->pairs[(iter).pos--] = (iter).map->pairs[--((iter).map->length)]

/*
 * Return true if iter is not the last item.
 */
#define gdd_map_iter_has_next(iter) \
	((iter).pos + 1 < (iter).map->length)


/*
 * Foreach helpers to walk through kinds of object.
 *
 * With these helpers we can flatten some nested walk through logic,
 * but be careful on some limitations:
 *
 * - Some of these helpers are implemented with nested for loops,
 *   `break` might not work as expected, do not use it;
 * - These helpers are all safe, but only the innermost iter can be in-place
 *   deleted, do not delete iters at other levels;
 */

#define gdd_vert_foreach_in_edge(edgeiter, vert) \
	gdd_list_foreach_safe((edgeiter), (vert)->edgesIn)

#define gdd_vert_foreach_out_edge(edgeiter, vert) \
	gdd_list_foreach_safe((edgeiter), (vert)->edgesOut)


#define gdd_graph_foreach_vert(vertiter, graph) \
	gdd_map_foreach_safe((vertiter), &((graph)->verts))

#define gdd_graph_foreach_in_edge(vertiter, edgeiter, graph) \
	gdd_graph_foreach_vert((vertiter), (graph)) \
	gdd_vert_foreach_in_edge((edgeiter), \
							 (GddVert *) gdd_map_iter_get_ptr(vertiter))

#define gdd_graph_foreach_out_edge(vertiter, edgeiter, graph) \
	gdd_graph_foreach_vert((vertiter), (graph)) \
	gdd_vert_foreach_out_edge((edgeiter), \
							  (GddVert *) gdd_map_iter_get_ptr(vertiter))


#define gdd_ctx_foreach_global(globaliter, ctx) \
	gdd_map_foreach_safe((globaliter), &((ctx)->globals))

#define gdd_ctx_foreach_graph(graphiter, ctx) \
	gdd_map_foreach_safe((graphiter), &((ctx)->graphs))

#define gdd_ctx_foreach_vert(graphiter, vertiter, ctx) \
	gdd_ctx_foreach_graph((graphiter), (ctx)) \
	gdd_graph_foreach_vert((vertiter), \
						   (GddGraph *) gdd_map_iter_get_ptr(graphiter))

#define gdd_ctx_foreach_in_edge(graphiter, vertiter, edgeiter, ctx) \
	gdd_ctx_foreach_graph((graphiter), (ctx)) \
	gdd_graph_foreach_in_edge((vertiter), (edgeiter), \
							  (GddGraph *) gdd_map_iter_get_ptr(graphiter))

#define gdd_ctx_foreach_out_edge(graphiter, vertiter, edgeiter, ctx) \
	gdd_ctx_foreach_graph((graphiter), (ctx)) \
	gdd_graph_foreach_out_edge((vertiter), (edgeiter), \
							   (GddGraph *) gdd_map_iter_get_ptr(graphiter))

/***************************************************************************/

/*
 * A <int, pointer> pair.
 */
struct GddPair
{
	int			key;
	void		*ptr;
};

/*
 * A simple int->ptr map, implemented with array.
 */
struct GddMap
{
	int			length;			/* length of the <k,v> array */
	int			capacity;		/* capacity of the <k,v> array */

	GddPair		*pairs;			/* array of <k,v> pairs */
};

/*
 * Map iterator which supports safe map foreach iteration.
 */
struct GddMapIter
{
	GddMap		*map;
	int			pos;
};

/*
 * List iterator which supports safe list foreach iteration.
 */
struct GddListIter
{
	List		*list;
	ListCell	*cell;
	ListCell	*prev;
};

/*
 * A directed edge.
 */
struct GddEdge
{
	bool		solid;			/* a solid edge? */

	GddVert		*from;			/* the from vert */
	GddVert		*to;			/* the to vert */

	void        *data;
};

/*
 * A vertex (vert) on a segment.
 */
struct GddVert
{
	int			id;				/* vert id */

	/*
	 * Pointer to ctx->topstat, all the verts on all the globals maintain
	 * the overall in/out degrees count together.
	 */
	GddStat		*topstat;

	/*
	 * Pointer to the `global` struct with the same vert id, the global
	 * in/out degrees of vert.
	 */
	GddStat		*global;

	List		*edgesIn;		/* List<Edge>, directed edges to vert */
	List		*edgesOut;		/* List<Edge>, directed edges from vert */

	/*
	 * The data set and used only by the caller, GDD does not touch or access
	 * it.
	 */
	void		*data;
};

/*
 * Directed graph on a segment.
 */
struct GddGraph
{
	int			id;				/* segment id */

	GddMap		verts;			/* Map<vid, Vert>, the verts */
};

/*
 * Accounting information of a vert.
 */
struct GddStat
{
	int			id;				/* vert id */

	int			indeg;			/* in degree */
	int			outdeg;			/* out degree */
};

/*
 * GDD context, the toplevel struct which contains all the information.
 */
struct GddCtx
{
	GddStat		topstat;		/* overall in/out degrees of all verts on all graphs */
	GddMap		globals;		/* Map<vid, Stat>, global in/out degrees */
	GddMap		graphs;			/* Map<segid, Graph>, the local graphs */
};

/***************************************************************************/

#endif   /* GDDDETECTORPRIV_H */
