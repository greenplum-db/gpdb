---
title: gp_parallel_retrieve_cursor
---

The `gp_parallel_retrieve_cursor` module is an enhanced cursor implementation that you can use to create a special kind of cursor on the Greenplum Database master node, and retrieve query results, on demand and in parallel, directly from the Greenplum segments. Greenplum refers to such a cursor as a *parallel retrieve cursor*.

The `gp_parallel_retrieve_cursor` module is a Greenplum Database-specific cursor implementation loosely based on the PostgreSQL cursor.

This topic includes the following sections:

-   [Installing and Registering the Module](#topic_reg)
-   [About the gp\_parallel\_retrieve\_cursor Module](#topic_about)
-   [Using the gp\_parallel\_retrieve\_cursor Module](#topic_using)
-   [Known Issues and Limitations](#topic_limits)
-   [Additional Module Documentation](#topic_addtldocs)
-   [Example](#topic_examples)

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_parallel_retrieve_cursor` module is installed when you install Greenplum Database. Before you can use any of the functions or views defined in the module, you must register the `gp_parallel_retrieve_cursor` extension in each database where you want to use the functionality:

```
CREATE EXTENSION gp_parallel_retrieve_cursor;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_about"></a>About the gp\_parallel\_retrieve\_cursor Module 

You use a cursor to retrieve a smaller number of rows at a time from a larger query. When you declare a parallel retrieve cursor, the Greenplum Database Query Dispatcher \(QD\) dispatches the query plan to each Query Executor \(QE\), and creates an *endpoint* on each QE before it executes the query. An endpoint is a query result source for a parallel retrieve cursor on a specific QE. Instead of returning the query result to the QD, an endpoint retains the query result for retrieval via a different process: a direct connection to the endpoint. You open a special retrieve mode connection, called a *retrieve session*, and use the new `RETRIEVE` SQL command to retrieve query results from each parallel retrieve cursor endpoint. You can retrieve from parallel retrieve cursor endpoints on demand and in parallel.

The `gp_parallel_retrieve_cursor` module provides the following functions and views that you can use to examine and manage parallel retrieve cursors and endpoints:

|Function, View Name|Description|
|-------------------|-----------|
|gp\_get\_endpoints\(\)<br/><br/>[gp\_endpoints](../system_catalogs/gp_endpoints.html#topic1)|List the endpoints associated with all active parallel retrieve cursors declared by the current session user in the current database. When the Greenplum Database superuser invokes this function, it returns a list of all endpoints for all parallel retrieve cursors declared by all users in the current database.|
|gp\_get\_session\_endpoints\(\)<br/><br/>[gp\_session\_endpoints](../system_catalogs/gp_session_endpoints.html#topic1)|List the endpoints associated with all parallel retrieve cursors declared in the current session for the current session user.|
|gp\_get\_segment\_endpoints\(\)<br/><br/>[gp\_segment\_endpoints](../system_catalogs/gp_segment_endpoints.html#topic1)|List the endpoints created in the QE for all active parallel retrieve cursors declared by the current session user. When the Greenplum Database superuser accesses this view, it returns a list of all endpoints on the QE created for all parallel retrieve cursors declared by all users.|
|gp\_wait\_parallel\_retrieve\_cursor\(cursorname text, timeout\_sec int4 \)|Return cursor status or block and wait for results to be retrieved from all endpoints associated with the specified parallel retrieve cursor.|

**Note:** Each of these functions and views is located in the `pg_catalog` schema, and each `RETURNS TABLE`.

## <a id="topic_using"></a>Using the gp\_parallel\_retrieve\_cursor Module 

You will perform the following tasks when you use a Greenplum Database parallel retrieve cursor to read query results in parallel from Greenplum segments:

1.  [Declare the parallel retrieve cursor](#declare_cursor).
2.  [List the endpoints of the parallel retrieve cursor](#list_endpoints).
3.  [Open a retrieve connection to each endpoint](#open_retrieve_conn).
4.  [Retrieve data from each endpoint](#retrieve_data).
5.  [Wait for data retrieval to complete](#wait).
6.  [Handle data retrieval errors](#error_handling).
7.  [Close the parallel retrieve cursor](#close).

In addition to the above, you may optionally choose to open a utility-mode connection to an endpoint to [List segment-specific retrieve session information](#utility_endpoints).

### <a id="declare_cursor"></a>Declaring a Parallel Retrieve Cursor 

You [DECLARE](../sql_commands/DECLARE.html#topic1) a cursor to retrieve a smaller number of rows at a time from a larger query. When you declare a parallel retrieve cursor, you can retrieve the query results directly from the Greenplum Database segments.

The syntax for declaring a parallel retrieve cursor is similar to that of declaring a regular cursor; you must additionally include the `PARALLEL RETRIEVE` keywords in the command. You can declare a parallel retrieve cursor only within a transaction, and the cursor name that you specify when you declare the cursor must be unique within the transaction.

For example, the following commands begin a transaction and declare a parallel retrieve cursor named `prc1` to retrieve the results from a specific query:

```
BEGIN;
DECLARE prc1 PARALLEL RETRIEVE CURSOR FOR <query>;
```

Greenplum Database creates the endpoint\(s\) on the QD or QEs, depending on the *query* parameters:

-   Greenplum Database creates an endpoint on the QD when the query results must be gathered by the master. For example, this `DECLARE` statement requires that the master gather the query results:

    ```
    DECLARE c1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1 ORDER BY a;
    ```

    **Note:** You may choose to run the `EXPLAIN` command on the parallel retrieve cursor query to identify when motion is involved. Consider using a regular cursor for such queries.

-   When the query involves direct dispatch to a segment \(the query is filtered on the distribution key\), Greenplum Database creates the endpoint\(s\) on specific segment host\(s\). For example, this `DECLARE` statement may result in the creation of single endpoint:

    ```
    DECLARE c2 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1 WHERE a=1;
    ```

-   Greenplum Database creates the endpoints on all segment hosts when all hosts contribute to the query results. This example `DECLARE` statement results in all segments contributing query results:

    ```
    DECLARE c3 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1;
    ```


The `DECLARE` command returns when the endpoints are ready and query execution has begun.

### <a id="list_endpoints"></a>Listing a Parallel Retrieve Cursor's Endpoints 

You can obtain the information that you need to initiate a retrieve connection to an endpoint by invoking the `gp_get_endpoints()` function or examining the `gp_endpoints` view in a session on the Greenplum Database master host:

```
SELECT * FROM gp_get_endpoints();
SELECT * FROM gp_endpoints;
```

These commands return the list of endpoints in a table with the following columns:

|Column Name|Description|
|-----------|-----------|
|gp\_segment\_id|The QE's endpoint `gp_segment_id`.|
|auth\_token|The authentication token for a retrieve session.|
|cursorname|The name of the parallel retrieve cursor.|
|sessionid|The identifier of the session in which the parallel retrieve cursor was created.|
|hostname|The name of the host from which to retrieve the data for the endpoint.|
|port|The port number from which to retrieve the data for the endpoint.|
|username|The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|state|The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|endpointname|The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|

Refer to the [gp\_endpoints](../system_catalogs/gp_endpoints.html#topic1) view reference page for more information about the endpoint attributes returned by these commands.

You can similarly invoke the `gp_get_session_endpoints()` function or examine the `gp_session_endpoints` view to list the endpoints created for the parallel retrieve cursors declared in the current session and by the current user.

### <a id="open_retrieve_conn"></a>Opening a Retrieve Session 

After you declare a parallel retrieve cursor, you can open a retrieve session to each endpoint. Only a single retrieve session may be open to an endpoint at any given time.

**Note:** A retrieve session is independent of the parallel retrieve cursor itself and the endpoints.

Retrieve session authentication does not depend on the `pg_hba.conf` file, but rather on an authentication token \(`auth_token`\) generated by Greenplum Database.

**Note:** Because Greenplum Database skips `pg_hba.conf`-controlled authentication for a retrieve session, for security purposes you may invoke only the `RETRIEVE` command in the session.

When you initiate a retrieve session to an endpoint:

-   The user that you specify for the retrieve session must be the session user that declared the parallel retrieve cursor \(the `username` returned by `gp_endpoints`\). This user must have Greenplum Database login privileges.
-   You specify the `hostname` and `port` returned by `gp_endpoints` for the endpoint.
-   You authenticate the retrieve session by specifying the `auth_token` returned for the endpoint via the `PGPASSWORD` environment variable, or when prompted for the retrieve session `Password`.
-   You must specify the [gp\_retrieve\_conn](../config_params/guc-list.html#gp_retrieve_conn) server configuration parameter on the connection request, and set the value to `true` .

For example, if you are initiating a retrieve session via `psql`:

```
PGOPTIONS='-c gp_retrieve_conn=true' psql -h <hostname> -p <port> -U <username> -d <dbname>
```

To distinguish a retrieve session from other sessions running on a segment host, Greenplum Database includes the `[retrieve]` tag on the `ps` command output display for the process.

### <a id="retrieve_data"></a>Retrieving Data From the Endpoint 

Once you establish a retrieve session, you retrieve the tuples associated with a query result on that endpoint using the [RETRIEVE](../sql_commands/RETRIEVE.html#topic1) command.

You can specify a \(positive\) number of rows to retrieve, or `ALL` rows:

```
RETRIEVE 7 FROM ENDPOINT prc10000003300000003;
RETRIEVE ALL FROM ENDPOINT prc10000003300000003;
```

Greenplum Database returns an empty set if there are no more rows to retrieve from the endpoint.

**Note:** You can retrieve from multiple parallel retrieve cursors from the same retrieve session only when their `auth_token`s match.

### <a id="wait"></a>Waiting for Data Retrieval to Complete 

Use the `gp_wait_parallel_retrieve_cursor()` function to display the the status of data retrieval from a parallel retrieve cursor, or to wait for all endpoints to finishing retrieving the data. You invoke this function in the transaction block in which you declared the parallel retrieve cursor.

`gp_wait_parallel_retrieve_cursor()` returns `true` only when all tuples are fully retrieved from all endpoints. In all other cases, the function returns `false` and may additionally throw an error.

The function signatures of `gp_wait_parallel_retrieve_cursor()` follow:

```
gp_wait_parallel_retrieve_cursor( cursorname text )
gp_wait_parallel_retrieve_cursor( cursorname text, timeout_sec int4 )
```

You must identify the name of the cursor when you invoke this function. The timeout argument is optional:

-   The default timeout is `0` seconds: Greenplum Database checks the retrieval status of all endpoints and returns the result immediately.
-   A timeout value of `-1` seconds instructs Greenplum to block until all data from all endpoints has been retrieved, or block until an error occurs.
-   The function reports the retrieval status after a timeout occurs for any other positive timeout value that you specify.

`gp_wait_parallel_retrieve_cursor()` returns when it encounters one of the following conditions:

-   All data has been retrieved from all endpoints.
-   A timeout has occurred.
-   An error has occurred.

### <a id="error_handling"></a>Handling Data Retrieval Errors 

An error can occur in a retrieve sesson when:

-   You cancel or interrupt the retrieve operation.
-   The endpoint is only partially retrieved when the retrieve session quits.

When an error occurs in a specific retrieve session, Greenplum Database removes the endpoint from the QE. Other retrieve sessions continue to function as normal.

If you close the transaction before fully retrieving from all endpoints, or if `gp_wait_parallel_retrieve_cursor()` returns an error, Greenplum Database terminates all remaining open retrieve sessions.

### <a id="close"></a>Closing the Cursor 

When you have completed retrieving data from the parallel retrieve cursor, close the cursor and end the transaction:

```
CLOSE prc1;
END;
```

**Note:** When you close a parallel retrieve cursor, Greenplum Database terminates any open retrieve sessions associated with the cursor.

On closing, Greenplum Database frees all resources associated with the parallel retrieve cursor and its endpoints.

### <a id="utility_endpoints"></a>Listing Segment-Specific Retrieve Session Information 

You can obtain information about all retrieve sessions to a specific QE endpoint by invoking the `gp_get_segment_endpoints()` function or examining the `gp_segment_endpoints` view:

```
SELECT * FROM gp_get_segment_endpoints();
SELECT * FROM gp_segment_endpoints;
```

These commands provide information about the retrieve sessions associated with a QE endpoint for all active parallel retrieve cursors declared by the current session user. When the Greenplum Database superuser invokes the command, it returns the retrieve session information for all endpoints on the QE created for all parallel retrieve cursors declared by all users.

You can obtain segment-specific retrieve session information in two ways: from the QD, or via a utility-mode connection to the endpoint:

-   QD example:

    ```
    SELECT * from gp_dist_random('gp_segment_endpoints');
    ```

    Display the information filtered to a specific segment:

    ```
    SELECT * from gp_dist_random('gp_segment_endpoints') WHERE gp_segment_id = 0;
    ```

-   Example utilizing a utility-mode connection to the endpoint:

    ```
    $ PGOPTIONS='-c gp_session_role=utility' psql -h sdw3 -U localuser -p 6001 -d testdb
    
    testdb=> SELECT * from gp_segment_endpoints;
    ```


The commands return endpoint and retrieve session information in a table with the following columns:

|Column Name|Description|
|-----------|-----------|
|auth\_token|The authentication token for a the retrieve session.|
|databaseid|The identifier of the database in which the parallel retrieve cursor was created.|
|senderpid|The identifier of the process sending the query results.|
|receiverpid|The process identifier of the retrieve session that is receiving the query results.|
|state|The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|gp\_segment\_id|The QE's endpoint `gp_segment_id`.|
|sessionid|The identifier of the session in which the parallel retrieve cursor was created.|
|username|The name of the session user that initiated the retrieve session.|
|endpointname|The endpoint identifier.|
|cursorname|The name of the parallel retrieve cursor.|

Refer to the [gp\_segment\_endpoints](../system_catalogs/gp_segment_endpoints.html#topic1) view reference page for more information about the endpoint attributes returned by these commands.

## <a id="topic_limits"></a>Known Issues and Limitations 

The `gp_parallel_retrieve_cursor` module has the following limitations:

-   The Tanzu Greenplum Query Optimizer \(GPORCA\) does not support queries on a parallel retrieve cursor.
-   Greenplum Database ignores the `BINARY` clause when you declare a parallel retrieve cursor.
-   Parallel retrieve cursors cannot be declared `WITH HOLD`.
-   Parallel retrieve cursors do not support the `FETCH` and `MOVE` cursor operations.
-   Parallel retrieve cursors are not supported in SPI; you cannot declare a parallel retrieve cursor in a PL/pgSQL function.

## <a id="topic_addtldocs"></a>Additional Module Documentation 

Refer to the `gp_parallel_retrieve_cursor` [README](https://github.com/greenplum-db/gpdb/tree/master/src/backend/cdb/endpoint/README) in the Greenplum Database `github` repository for additional information about this module. You can also find parallel retrieve cursor [programming examples](https://github.com/greenplum-db/gpdb/tree/master/src/test/examples/) in the repository.

## <a id="topic_examples"></a>Example 

Create a parallel retrieve cursor and use it to pull query results from a Greenplum Database cluster:

1.  Open a `psql` session to the Greenplum Database master host:

    ```
    psql -d testdb
    ```

2.  Register the `gp_parallel_retrieve_cursor` extension if it does not already exist:

    ```
    CREATE EXTENSION IF NOT EXISTS gp_parallel_retrieve_cursor;
    ```

3.  Start the transaction:

    ```
    BEGIN;
    ```

4.  Declare a parallel retrieve cursor named `prc1` for a `SELECT *` query on a table:

    ```
    DECLARE prc1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1;
    ```

5.  Obtain the endpoints for this parallel retrieve cursor:

    ```
    SELECT * FROM gp_endpoints WHERE cursorname='prc1';
     gp_segment_id |            auth_token            | cursorname | sessionid | hostname | port | username | state |     endpointname     
    ---------------+----------------------------------+------------+-----------+----------+------+----------+-------+----------------------
                 2 | 39a2dc90a82fca668e04d04e0338f105 | prc1       |        51 | sdw1     | 6000 | bill     | READY | prc10000003300000003
                 3 | 1a6b29f0f4cad514a8c3936f9239c50d | prc1       |        51 | sdw1     | 6001 | bill     | READY | prc10000003300000003
                 4 | 1ae948c8650ebd76bfa1a1a9fa535d93 | prc1       |        51 | sdw2     | 6000 | bill     | READY | prc10000003300000003
                 5 | f10f180133acff608275d87966f8c7d9 | prc1       |        51 | sdw2     | 6001 | bill     | READY | prc10000003300000003
                 6 | dda0b194f74a89ed87b592b27ddc0e39 | prc1       |        51 | sdw3     | 6000 | bill     | READY | prc10000003300000003
                 7 | 037f8c747a5dc1b75fb10524b676b9e8 | prc1       |        51 | sdw3     | 6001 | bill     | READY | prc10000003300000003
                 8 | c43ac67030dbc819da9d2fd8b576410c | prc1       |        51 | sdw4     | 6000 | bill     | READY | prc10000003300000003
                 9 | e514ee276f6b2863142aa2652cbccd85 | prc1       |        51 | sdw4     | 6001 | bill     | READY | prc10000003300000003
    (8 rows)
    ```

6.  Wait until all endpoints are fully retrieved:

    ```
    SELECT gp_wait_parallel_retrieve_cursor( 'prc1', -1 );
    ```

7.  For each endpoint:
    1.  Open a retrieve session. For example, to open a retrieve session to the segment instance running on `sdw3`, port number `6001`, run the following command in a *different terminal window*; when prompted for the password, provide the `auth_token` identified in row 7 of the `gp_endpoints` output:

        ```
        $ PGOPTIONS='-c gp_retrieve_conn=true' psql -h sdw3 -U localuser -p 6001 -d testdb
        Password:
        ```

    2.  Retrieve data from the endpoint:

        ```
        -- Retrieve 7 rows of data from this session
        RETRIEVE 7 FROM ENDPOINT prc10000003300000003
        -- Retrieve the remaining rows of data from this session
        RETRIEVE ALL FROM ENDPOINT prc10000003300000003
        ```

    3.  Exit the retrieve session.

        ```
        \q
        ```

8.  In the original `psql` session \(the session in which you declared the parallel retrieve cursor\), verify that the `gp_wait_parallel_retrieve_cursor()` function returned `t`. Then close the cursor and complete the transaction:

    ```
    CLOSE prc1;
    END;
    ```


