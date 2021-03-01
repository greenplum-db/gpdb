/*
 * dblink_no_auth.c
 *
 * Functions returning results from a remote database
 *
 * Joe Conway <mail@joeconway.com>
 * And contributors:
 * Darko Prenosil <Darko.Prenosil@finteh.hr>
 * Shridhar Daithankar <shridhar_daithankar@persistent.co.in>
 *
 * contrib/dblink/dblink.c
 * Copyright (c) 2001-2014, PostgreSQL Global Development Group
 * ALL RIGHTS RESERVED;
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE AUTHOR OR DISTRIBUTORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND DISTRIBUTORS SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 */
extern PGconn *
backend_PQconnectdb(const char *conninfo);

/*
 * Create a persistent connection to another database
 *
 * dblink_connect_no_auth() is identical to dblink_connect_u(),
 * except that it skips authentication when gpdb uses it to connect
 * to itself, so impersonation and subsequent escalation of privileges
 * can occur when using it.
 *
 * We make dblink_connect_no_auth linked to backend_fe-connect.o,
 * and undef the FRONTEND in dblink_no_auth.c. So dblink can use
 * internal gpdb connection to connect to local master and then the
 * authentication will be skipped. More details are in the function
 * ClientAuthentication
 *
 * This function is really hack and has some serious security issues, why
 * do we add it? Because before the commit 1dc23be2, dblink is wrongly
 * linked to backend libpq, and when using it to connect to master itself,
 * authentication is skipped. However, after link issue fixed, customers
 * still want this feature, so we have to add it back.
 */
PG_FUNCTION_INFO_V1(dblink_connect_no_auth);
Datum
dblink_connect_no_auth(PG_FUNCTION_ARGS)
{
	char	   *conname_or_str = NULL;
	char	   *connstr = NULL;
	char	   *connname = NULL;
	char	   *msg;
	PGconn	   *conn = NULL;
	remoteConn *rconn = NULL;

	DBLINK_INIT;

	if (PG_NARGS() == 2)
	{
		conname_or_str = text_to_cstring(PG_GETARG_TEXT_PP(1));
		connname = text_to_cstring(PG_GETARG_TEXT_PP(0));
	}
	else if (PG_NARGS() == 1)
		conname_or_str = text_to_cstring(PG_GETARG_TEXT_PP(0));

	if (connname)
		rconn = (remoteConn *) MemoryContextAlloc(TopMemoryContext,
												  sizeof(remoteConn));

	/* first check for valid foreign data server */
	connstr = get_connect_string(conname_or_str);
	if (connstr == NULL)
		connstr = conname_or_str;

	/* check password in connection string if not superuser */
	connstr = dblink_connstr_check(connstr);
	conn = backend_PQconnectdb(connstr);

	if (PQstatus(conn) == CONNECTION_BAD)
	{
		msg = pstrdup(PQerrorMessage(conn));
		PQfinish(conn);
		if (rconn)
			pfree(rconn);

		ereport(ERROR,
				(errcode(ERRCODE_SQLCLIENT_UNABLE_TO_ESTABLISH_SQLCONNECTION),
				 errmsg("could not establish connection"),
				 errdetail_internal("%s", msg)));
	}

	/* check password actually used if not superuser */
	dblink_security_check(conn, rconn);

	/* attempt to set client encoding to match server encoding, if needed */
	if (PQclientEncoding(conn) != GetDatabaseEncoding())
		PQsetClientEncoding(conn, GetDatabaseEncodingName());

	if (connname)
	{
		rconn->conn = conn;
		createNewConnection(connname, rconn);
	}
	else
	{
		if (pconn->conn)
			PQfinish(pconn->conn);
		pconn->conn = conn;
	}

	PG_RETURN_TEXT_P(cstring_to_text("OK"));
}
