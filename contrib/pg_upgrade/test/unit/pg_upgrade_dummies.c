/*
 * Dummy functions:
 * 
 * used to provide completely invalid results, not necessary for tests 
 * to run successfully
 */

void
pg_fatal(const char *fmt, ...)
{
	exit(0);
}

void
report_status(eLogType type, const char *fmt, ...)
{

}

PGconn *
connectToServer(ClusterInfo *cluster, const char *db_name)
{
	return NULL;
}

PGresult *
executeQueryOrDie(PGconn *conn, const char *fmt, ...)
{
	return NULL;
}

const char *
getErrorText(void)
{
	return NULL;
}

void check_ok() {}
void prep_status(const char *fmt,...) {}