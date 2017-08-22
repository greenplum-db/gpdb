/* mock implementation for pxfutils.h */
bool
are_ips_equal(char* ip1, char* ip2)
{
	check_expected(ip1);
	check_expected(ip2);
	optional_assignment(ip1);
	optional_assignment(ip2);
	return (bool) mock();
}

void
port_to_str(char** port, int new_port)
{
	check_expected(port);
	check_expected(new_port);
	optional_assignment(port);
	mock();
}

char*
normalize_key_name(const char* key)
{
    check_expected(key);
    return (char*) mock();
}

char*
TypeOidGetTypename(Oid typid)
{
	check_expected(typid);
	return (char*) mock();
}
