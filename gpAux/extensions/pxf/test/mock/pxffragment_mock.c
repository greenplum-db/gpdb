void
get_fragments(GPHDUri *uri, Relation relation)
{
	check_expected(uri);
	check_expected(relation);
	optional_assignment(uri);
	optional_assignment(relation);
	mock();
}

void
call_rest(GPHDUri* hadoop_uri, ClientContext* client_context, char* rest_msg)
{
	check_expected(hadoop_uri);
	check_expected(client_context);
	check_expected(rest_msg);
	optional_assignment(hadoop_uri);
	optional_assignment(client_context);
	optional_assignment(rest_msg);
	mock();
}

static void
process_request(ClientContext* client_context, char* uri)
{
	mock();
}

void
free_fragment(FragmentData *data)
{
	check_expected(data);
	optional_assignment(data);
	mock();
}