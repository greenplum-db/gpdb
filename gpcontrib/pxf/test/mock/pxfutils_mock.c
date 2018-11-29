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

char*
concat(int num_args, ...)
{
	check_expected(num_args);
	return (char*) mock();
}

char*
get_authority(void)
{
	return (char*) mock();
}