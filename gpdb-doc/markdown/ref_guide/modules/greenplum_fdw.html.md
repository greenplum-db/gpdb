---
title: greenplum_fdw
---

The `greenplum_fdw` module is a foreign-data wrapper \(FDW\) that you can use to run queries across one or more Greenplum Database version 6.20+ clusters.

The Greenplum Database `greenplum_fdw` module is an MPP extension of the PostgreSQL `postgres_fdw` module.

This topic includes the following sections:

-   [Installing and Registering the Module](#topic_reg)
-   [About Module Dependencies](#topic_depend)
-   [About the greenplum\_fdw Module](#topic_about)
-   [Using the greenplum\_fdw Module](#topic_using)
-   [Additional Information](#topic_other)
-   [Known Issues and Limitations](#topic_limits)
-   [Compatibility](#topic_compat)
-   [Example](#topic_examples)

## <a id="topic_reg"></a>Installing and Registering the Module 

The `greenplum_fdw` module is installed when you install Greenplum Database. Before you can use this FDW, you must register the `greenplum_fdw` extension in each database in the local Greenplum Database cluster in which you plan to use it:

```
CREATE EXTENSION greenplum_fdw;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information about installing and registering modules in Greenplum Database.

## <a id="topic_depend"></a>About Module Dependencies 

`greenplum_fdw` depends on the [gp\_parallel\_retrieve\_cursor](gp_parallel_retrieve_cursor.html) module.

**Note:** You must register the `gp_parallel_retrieve_cursor` module in **each remote Greenplum database** with tables that you plan to access using the `greenplum_fdw` foreign-data wrapper.

## <a id="topic_about"></a>About the greenplum\_fdw Module 

`greenplum_fdw` is an MPP version of the [postgres\_fdw](https://www.postgresql.org/docs/9.4/postgres-fdw.html) foreign-data wrapper. While it behaves similarly to `postgres_fdw` in many respects, `greenplum_fdw` uses a Greenplum Database parallel retrieve cursor to pull data directly from the segments of a remote Greenplum cluster to the segments in the local Greenplum cluster, in parallel.

By supporting predicate pushdown, `greenplum_fdw` minimizes the amount of data transferred between the Greenplum clusters by sending a query filter condition to the remote Greenplum server where it is applied there.

## <a id="topic_using"></a>Using the greenplum\_fdw Module 

You will perform the following tasks when you use `greenplum_fdw` to access data that resides in a remote Greenplum Database cluster\(s\):

1.  [Create a server](#create_server) to represent each remote Greenplum database to which you want to connect.
2.  [Create a user mapping](#user_mapping) for each \(local\) Greenplum Database user that you want to allow to access each server.
3.  [Create a foreign table](#create_ftable) for each remote Greenplum table that you want to access.
4.  [Construct and run queries](#query_ftable).

### <a id="create_server"></a>Creating a Server 

To access a remote Greenplum Database cluster, you must first create a foreign server object which specifies the host, port, and database connection details. You provide these connection parameters in the `OPTIONS` clause of the [CREATE SERVER](../sql_commands/CREATE_SERVER.html) command.

A foreign server using the `greenplum_fdw` foreign-data wrapper accepts and disallows the same options as that of a foreign server using the `postgres_fdw` FDW; refer to the [Connection Options](https://www.postgresql.org/docs/9.4/postgres-fdw.html) topic in the PostgreSQL `postgres_fdw` documentation for more information about these options.

To obtain the full benefits of the parallel transfer feature provided by `greenplum_fdw`, you must also specify:

```
mpp_execute 'all segments'
```

and

```
num_segments '<num>'
```

in the `OPTIONS` clause when you create the server. Set num to the number of segments in the the remote Greenplum Database cluster. If you do not provide the

```
num_segments
```

option, the default value is the number of segments on the local Greenplum Database cluster.

The following example command creates a server named `gpc1_testdb` that will be used to access tables residing in the database named `testdb` on the remote `8`-segment Greenplum Database cluster whose master is running on the host `gpc1_master`, port `5432`:

```
CREATE SERVER gpc1_testdb FOREIGN DATA WRAPPER greenplum_fdw
    OPTIONS (host 'gpc1_master', port '5432', dbname 'testdb', mpp_execute 'all segments', num_segments '8');
```

### <a id="user_mapping"></a>Creating a User Mapping 

After you identify which users you will allow to access the remote Greenplum Database cluster, you must create one or more mappings between a local Greenplum user and a user on the remote Greenplum cluster. You create these mappings with the [CREATE USER MAPPING](../sql_commands/CREATE_USER_MAPPING.html) command.

User mappings that you create may include the following `OPTIONS`:

|Option Name|Description|Default Value|
|-----------|-----------|-------------|
|user|The name of the remote Greenplum Database user to connect as.|The name of the current \(local\) Greenplum Database user.|
|password|The password for user on the remote Greenplum Database system.|No default value.|

Only a Greenplum Database superuser may connect to a Greenplum foreign server without password authentication. Always specify the `password` option for user mappings that you create for non-superusers.

The following command creates a default user mapping on the local Greenplum cluster to the user named `bill` on the remote Greenplum cluster that allows access to the database identified by the `gpc1_testdb` server. Specifying the `PUBLIC` user name creates a mapping for all current and future users when no user-specific mapping is applicable.

```
CREATE USER MAPPING FOR PUBLIC SERVER gpc1_testdb
    OPTIONS (user 'bill', password 'changeme');
```

The remote user must have the appropriate privileges to access any table\(s\) of interest in the database identified by the specified `SERVER`. 

If the mapping is used to access a foreign-data wrapper across multiple Greenplum clusters, then the remote user also requires `SELECT` access to the `pg_catalog.gp_endpoints` view. For example:

```
GRANT SELECT ON TABLE pg_catalog.gp_endpoints TO bill;
```

### <a id="create_ftable"></a>Creating a Foreign Table 

You invoke the [CREATE FOREIGN TABLE](../sql_commands/CREATE_FOREIGN_TABLE.html) command to create a foreign table. The column data types that you specify when you create the foreign table should exactly match those in the referenced remote table. It is also recommended that the columns be declared with exactly the same collations, if applicable, as the referenced columns of the remote table.

Because `greenplum_fdw` matches foreign table columns to the remote table by name, not position, you can create a foreign table with fewer columns, or with a different column order, than the underlying remote table.

Foreign tables that you create may include the following `OPTIONS`:

|Option Name|Description|Default Value|
|-----------|-----------|-------------|
|schema\_name|The name of the schema in which the remote Greenplum Database table resides.|The name of the schema in which the foreign table resides.|
|table\_name|The name of the remote Greenplum Database table.|The name of the foreign table.|

The following command creates a foreign table named `f_gpc1_orders` that references a table named `orders` located in the `public` schema of the database identified by the `gpc1_testdb` server \(`testdb`\):

```
CREATE FOREIGN TABLE f_gpc1_orders ( id int, qty int, item text )
    SERVER gpc1_testdb OPTIONS (schema_name 'public', table_name 'orders');
```

You can additionally specify column name mappings via `OPTIONS` that you provide in the column declaration of the foreign table. The `column_name` option identifies the name of the associated column in the remote Greenplum Database table, and defaults to the foreign table column name when not specified.

### <a id="query_ftable"></a>Constructing and Running Queries 

You `SELECT` from a foreign table to access the data stored in the underlying remote Greenplum Database table. By default, you can also modify the remote table using the `INSERT` command, provided that the remote user specified the user mapping has the privileges to perform these operations. \(Refer to [About the Updatability Option](#topic_update) for information about changing the updatability of foreign tables.\)

`greenplum_fdw` attempts to optimize remote queries to reduce the amount of data transferred from foreign servers. This is achieved by sending query `WHERE` clauses to the remote Greenplum Database server for execution, and by not retrieving table columns that are not needed for the current query. To reduce the risk of misexecution of queries, `greenplum_fdw` does not send `WHERE` clauses to the remote server unless they use only built-in data types, operators, and functions. Operators and functions in the clauses must be `IMMUTABLE` as well.

You can run the `EXPLAIN VERBOSE` command to examine the query that is actually sent to the remote Greenplum Database server for execution.

## <a id="topic_other"></a>Additional Information 

For more information about `greenplum_fdw` updatability and cost estimation options, connection management, and transaction management, refer to the individual topics below.

### <a id="topic_update"></a>About the Updatability Option 

By default, all foreign tables created with `greenplum_fdw` are assumed to be updatable. You can override this for a foreign server or a foreign table using the following option:

`updatable`
:   Controls whether `greenplum_fdw` allows foreign tables to be modified using the `INSERT` command. The default is true.

Setting this option at the foreign table-level overrides a foreign server-level option setting.

### <a id="topic_costest"></a>About the Cost Estimation Options 

`greenplum_fdw` supports the same cost estimation options as described in the [Cost Estimation Options](https://www.postgresql.org/docs/9.4/postgres-fdw.html) topic in the PostgreSQL `postgres_fdw` documentation.

### <a id="topic_connmgmt"></a>About Connection Management 

`greenplum_fdw` establishes a connection to a foreign server during the first query on any foreign table associated with the server. `greenplum_fdw` retains and reuses this connection for subsequent queries submitted in the same session. However, if multiple user identities \(user mappings\) are used to access the foreign server, `greenplum_fdw` establishes a connection for each user mapping.

### <a id="topic_transmgmt"></a>About Transaction Management 

`greenplum_fdw` manages transactions as described in the [Transaction Management](https://www.postgresql.org/docs/9.4/postgres-fdw.html) topic in the PostgreSQL `postgres_fdw` documentation.

## <a id="topic_limits"></a>Known Issues and Limitations 

The `greenplum_fdw` module has the following known issues and limitations:

-   The Greenplum Query Optimizer \(GPORCA\) does not support queries on foreign tables that you create with the `greenplum_fdw` foreign-data wrapper.
-   `greenplum_fdw` does not support `UPDATE` and `DELETE` operations on foreign tables.

## <a id="topic_compat"></a>Compatibility 

You can use `greenplum_fdw` to access other remote Greenplum Database clusters running version 6.20+.

## <a id="topic_examples"></a>Example 

In this example, you query data residing in a database named `rdb` on the remote 16-segment Greenplum Database cluster whose master is running on host `gpc2_master`, port `5432`:

1.  Open a `psql` session to the master host of the remote Greenplum Database cluster:

    ```
    psql -h gpc2_master -d rdb
    ```

2.  Register the `gp_parallel_retrieve_cursor` extension in the database if it does not already exist:

    ```
    CREATE EXTENSION IF NOT EXISTS gp_parallel_retrieve_cursor;
    ```

3.  Exit the session.
4.  Initiate a `psql` session to the database named `testdb` on the local Greenplum Database master host:

    ```
    $ psql -d testdb
    ```

5.  Register the `greenplum_fdw` extension in the database if it does not already exist:

    ```
    CREATE EXTENSION IF NOT EXISTS greenplum_fdw;
    ```

6.  Create a server to access the remote Greenplum Database cluster:

    ```
    CREATE SERVER gpc2_rdb FOREIGN DATA WRAPPER greenplum_fdw
        OPTIONS (host 'gpc2_master', port '5432', dbname 'rdb', mpp_execute 'all segments', num_segments '16');
    ```

7.  Create a user mapping for a user named `jane` on the local Greenplum Database cluster and the user named `john` on the remote Greenplum cluster and database represented by the server named `gpc2_rdb`:

    ```
    CREATE USER MAPPING FOR jane SERVER gpc2_rdb OPTIONS (user 'john', password 'changeme');
    ```

8.  Create a foreign table named `f_gpc2_emea` to reference the table named `emea` that is resides in the `public` schema of the database identified by the `gpc2_rdb` server \(`rdb`\):

    ```
    CREATE FOREIGN TABLE f_gpc2_emea( bu text, income int )
        SERVER gpcs2_rdb OPTIONS (schema_name 'public', table_name 'emea');
    ```

9.  Query the foreign table:

    ```
    SELECT * FROM f_gpc2_emea;
    ```

10. Join the results of a foreign table query with a local table named `amer` that has similarly-named columns:

    ```
    SELECT amer.bu, amer.income as amer_in, f_gpc2_emea.income as emea_in
        FROM amer, f_gpc2_emea
        WHERE amer.bu = f_gpc2_emea.bu;
    ```


