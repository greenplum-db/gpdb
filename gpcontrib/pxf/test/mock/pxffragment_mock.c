
void
get_fragments(GPHDUri *uri,
              Relation relation,
              char *filter_string,
              ProjectionInfo *proj_info,
              List *quals)
{
	check_expected(uri);
	check_expected(relation);
	optional_assignment(uri);
	optional_assignment(relation);
	optional_assignment(filter_string);
	optional_assignment(proj_info);
	optional_assignment(quals);
	mock();
}

void
free_fragment(FragmentData *data)
{
	check_expected(data);
	optional_assignment(data);
	mock();
}
