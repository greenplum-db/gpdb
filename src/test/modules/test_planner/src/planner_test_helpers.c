#include "planner_test_helpers.h"

#include "optimizer/planmain.h"
#include "tcop/tcopprot.h"

static RawStmt *
get_parsetree_for(const char *query_string)
{
	List *parsetree_list = pg_parse_query(query_string);
	ListCell *parsetree_item = list_head(parsetree_list);
	return lfirst_node(RawStmt, parsetree_item);
}

static Query *
get_query_for_parsetree(Node *parsetree, const char *query_string)
{
	List *querytree_list = pg_analyze_and_rewrite(parsetree, query_string, NULL, 0, NULL);
	ListCell *querytree = list_head(querytree_list);
	return (Query *)lfirst(querytree);
}

Query *
make_query(const char *query_string)
{
	Node *parsetree = get_parsetree_for(query_string);

	return get_query_for_parsetree(parsetree, query_string);
}

Plan *
get_first_subplan(PlannedStmt *plannedStmt)
{
	return (Plan *)list_nth(plannedStmt->subplans, 0);
}

TargetEntry *
get_target_entry_from_root_plan_node(PlannedStmt *plannedstmt)
{
	Result *result_node = (Result*) plannedstmt->planTree;
	List *tlist = result_node->plan.targetlist;
	return list_nth(tlist, 0);
}
