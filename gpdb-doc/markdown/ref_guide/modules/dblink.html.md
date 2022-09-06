---
title: dblink 
---

The `dblink` module supports connections to other Greenplum Database databases from within a database session. These databases can reside in the same Greenplum Database system, or in a remote system.

Greenplum Database supports `dblink` connections between databases in Greenplum Database installations with the same major version number. You can also use `dblink` to connect to other Greenplum Database installations that use compatible `libpq` libraries.

**Note:** `dblink` is intended for database users to perform short ad hoc queries in other databases. `dblink` is not intended for use as a replacement for external tables or for administrative tools such as `gpcopy`.

The Greenplum Database `dblink` module is a modified version of the PostgreSQL `dblink` module. There are some restrictions and limitations when you use the module in Greenplum Database.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `dblink` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `dblink` extension in each database in which you want to use the functions. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_mpp"></a>Greenplum Database Considerations 

In this release of Greenplum Database, statements that modify table data cannot use named or implicit `dblink` connections. Instead, you must provide the connection string directly in the `dblink()` function. For example:

```
gpadmin=# CREATE TABLE testdbllocal (a int, b text) DISTRIBUTED BY (a);
CREATE TABLE
gpadmin=# INSERT INTO testdbllocal select * FROM dblink('dbname=postgres', 'SELECT * FROM testdblink') AS dbltab(id int, product text);
INSERT 0 2
```

The Greenplum Database version of `dblink` deactivates the following asynchronous functions:

-   `dblink_send_query()`
-   `dblink_is_busy()`
-   `dblink_get_result()`

## <a id="topic_using"></a>Using dblink 

The following procedure identifies the basic steps for configuring and using `dblink` in Greenplum Database. The examples use `dblink_connect()` to create a connection to a database and `dblink()` to run an SQL query.

1.  Begin by creating a sample table to query using the `dblink` functions. These commands create a small table in the `postgres` database, which you will later query from the `testdb` database using `dblink`:

    ```
    $ psql -d postgres
    psql (9.4.20)
    Type "help" for help.
    
    postgres=# CREATE TABLE testdblink (a int, b text) DISTRIBUTED BY (a);
    CREATE TABLE
    postgres=# INSERT INTO testdblink VALUES (1, 'Cheese'), (2, 'Fish');
    INSERT 0 2
    postgres=# \q
    $
    ```

2.  Log into a different database as a superuser. In this example, the superuser `gpadmin` logs into the database `testdb`. If the `dblink` functions are not already available, register the `dblink` extension in the database:

    ```
    $ psql -d testdb
    psql (9.4beta1)
    Type "help" for help.
    
    testdb=# CREATE EXTENSION dblink;
    CREATE EXTENSION
    ```

3.  Use the `dblink_connect()` function to create either an implicit or a named connection to another database. The connection string that you provide should be a `libpq`-style keyword/value string. This example creates a connection named `mylocalconn` to the `postgres` database on the local Greenplum Database system:

    ```
    testdb=# SELECT dblink_connect('mylocalconn', 'dbname=postgres user=gpadmin');
     dblink_connect
    ----------------
     OK
    (1 row)
    ```

    **Note:** If a `user` is not specified, `dblink_connect()` uses the value of the `PGUSER` environment variable when Greenplum Database was started. If `PGUSER` is not set, the default is the system user that started Greenplum Database.

4.  Use the `dblink()` function to query a database using a configured connection. Keep in mind that this function returns a record type, so you must assign the columns returned in the `dblink()` query. For example, the following command uses the named connection to query the table you created earlier:

    ```
    testdb=# SELECT * FROM dblink('mylocalconn', 'SELECT * FROM testdblink') AS dbltab(id int, product text);
     id | product
    ----+---------
      1 | Cheese
      2 | Fish
    (2 rows)
    ```


To connect to the local database as another user, specify the `user` in the connection string. This example connects to the database as the user `test_user`. Using `dblink_connect()`, a superuser can create a connection to another local database without specifying a password.

```
testdb=# SELECT dblink_connect('localconn2', 'dbname=postgres user=test_user');
```

To make a connection to a remote database system, include host and password information in the connection string. For example, to create an implicit `dblink` connection to a remote system:

```
testdb=# SELECT dblink_connect('host=remotehost port=5432 dbname=postgres user=gpadmin password=secret');
```

### <a id="dblink_u"></a>Using dblink as a Non-Superuser 

To make a connection to a database with `dblink_connect()`, non-superusers must include host, user, and password information in the connection string. The host, user, and password information must be included even when connecting to a local database. You must also include an entry in `pg_hba.conf` for this non-superuser and the target database. For example, the user `test_user` can create a `dblink` connection to the local system `mdw` with this command:

```
testdb=> SELECT dblink_connect('host=mdw port=5432 dbname=postgres user=test_user password=secret');
```

If non-superusers need to create `dblink` connections that do not require a password, they can use the `dblink_connect_u()` function. The `dblink_connect_u()` function is identical to `dblink_connect()`, except that it allows non-superusers to create connections that do not require a password.

`dblink_connect_u()` is initially installed with all privileges revoked from `PUBLIC`, making it un-callable except by superusers. In some situations, it may be appropriate to grant `EXECUTE` permission on `dblink_connect_u()` to specific users who are considered trustworthy, but this should be done with care.

**Warning:** If a Greenplum Database system has configured users with an authentication method that does not involve a password, then impersonation and subsequent escalation of privileges can occur when a non-superuser runs `dblink_connect_u()`. The `dblink` connection will appear to have originated from the user specified by the function. For example, a non-superuser can run `dblink_connect_u()` and specify a user that is configured with `trust` authentication.

Also, even if the `dblink` connection requires a password, it is possible for the password to be supplied from the server environment, such as a `~/.pgpass` file belonging to the server's user. It is recommended that any `~/.pgpass` file belonging to the server's user not contain any records specifying a wildcard host name.

1.  As a superuser, grant the `EXECUTE` privilege on the `dblink_connect_u()` functions in the user database. This example grants the privilege to the non-superuser `test_user` on the functions with the signatures for creating an implicit or a named `dblink` connection. The server and database will be identified through a standard `libpq` connection string and optionally, a name can be assigned to the connection.

    ```
    testdb=# GRANT EXECUTE ON FUNCTION dblink_connect_u(text) TO test_user;
    testdb=# GRANT EXECUTE ON FUNCTION dblink_connect_u(text, text) TO test_user;
    ```

2.  Now `test_user` can create a connection to another local database without a password. For example, `test_user` can log into the `testdb` database and run this command to create a connection named `testconn` to the local `postgres` database.

    ```
    testdb=> SELECT dblink_connect_u('testconn', 'dbname=postgres user=test_user');
    ```

    **Note:** If a `user` is not specified, `dblink_connect_u()` uses the value of the `PGUSER` environment variable when Greenplum Database was started. If `PGUSER` is not set, the default is the system user that started Greenplum Database.

3.  `test_user` can use the `dblink()` function to run a query using a `dblink` connection. For example, this command uses the `dblink` connection named `testconn` created in the previous step. `test_user` must have appropriate access to the table.

    ```
    testdb=> SELECT * FROM dblink('testconn', 'SELECT * FROM testdblink') AS dbltab(id int, product text);
    ```


### <a id="usindbnsu"></a>Using dblink as a Non-Superuser without Authentication Checks 

In rare cases you may need to allow non-superusers to acccess to `dblink` without making any authentication checks. The function `dblink_connect_no_auth()` provides this functionality as it bypasses the `pg_hba.conf` file.

**Warning:** Using this function introduces a security risk; ensure that you grant unauthorized access only to trusted user accounts. Also note that `dblink_connect_no_auth()` functions limit connections to the local cluster, and do not permit connections to a remote database.

These functions are not available by default; the `gpadmin` superuser must grant permission to the non-superuser beforehand:

1.  As a superuser, grant the `EXECUTE` privilege on the `dblink_connect_no_auth()` functions in the user database. This example grants the privilege to the non-superuser `test_user` on the functions with the signatures for creating an implicit or a named `dblink` connection.

    ```
    testdb=# GRANT EXECUTE ON FUNCTION dblink_connect_no_auth(text) TO test_user;
    testdb=# GRANT EXECUTE ON FUNCTION dblink_connect_no_auth(text, text) TO test_user;
    ```

2.  Now `test_user` can create a connection to another local database without providing a password, regardless of what is specified in `pg_hba.conf`. For example, `test_user` can log into the `testdb` database and execute this command to create a connection named `testconn` to the local `postgres` database.

    ```
    testdb=> SELECT dblink_connect_no_auth('testconn', 'dbname=postgres user=test_user');
    ```

3.  `test_user` can use the `dblink()` function to execute a query using a `dblink` connection. For example, this command uses the `dblink` connection named `testconn` created in the previous step. `test_user` must have appropriate access to the table.

    ```
    testdb=> SELECT * FROM dblink('testconn', 'SELECT * FROM testdblink') AS dbltab(id int, product text);
    ```


### <a id="dblink_ssl"></a>Using dblink with SSL-Encrypted Connections to Greenplum 

When you use `dblink` to connect to Greenplum Database over an encrypted connection, you must specify the `sslmode` property in the connection string. Set `sslmode` to at least `require` to disallow unencrypted transfers. For example:

```
testdb=# SELECT dblink_connect('greenplum_con_sales', 'dbname=sales host=gpmaster user=gpadmin sslmode=require');
```

Refer to [SSL Client Authentication](../../security-guide/topics/Authenticate.md#ssl_postgresql) for information about configuring Greenplum Database to use SSL.

## <a id="topic_info"></a>Additional Module Documentation 

Refer to the [dblink](https://www.postgresql.org/docs/9.4/dblink.html) PostgreSQL documentation for detailed information about the individual functions in this module.

