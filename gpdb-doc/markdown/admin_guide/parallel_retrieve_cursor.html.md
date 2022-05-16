---
title: Retrieving Query Results with a Parallel Retrieve Cursor (Beta)</title>
---

A *parallel retrieve cursor* is an enhanced cursor implementation that you can use to create a special kind of cursor on the Greenplum Database coordinator node, and retrieve query results, on demand and in parallel, directly from the Greenplum segments.

## <a id="topic_about"></a>About Parallel Retrieve Cursors

You use a cursor to retrieve a smaller number of rows at a time from a larger
          query. When you declare a parallel retrieve cursor, the Greenplum
          Database Query Dispatcher (QD) dispatches the query plan to each Query Executor
          (QE), and creates an *endpoint* on each QE before it executes the query.
          An endpoint is a query result source for a parallel retrieve cursor on a specific
          QE. Instead of returning the query result to the QD, an endpoint retains the
          query result for retrieval via a different process: a direct connection to the
          endpoint. You open a special retrieve mode connection, called a *retrieve
          session*, and use the new `RETRIEVE` SQL command to retrieve
          query results from each parallel retrieve cursor endpoint. You can retrieve
          from parallel retrieve cursor endpoints on demand and in parallel.

You can use the following functions and views to examine and manage parallel retrieve cursors and endpoints:


<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic_about__funcs" class="table" frame="border" border="1" rules="all">

          <thead class="thead" align="left">
            <tr class="row">
              <th class="entry" valign="top" width="48.760330578512395%" id="d181858e131">Function, View Name</th>

              <th class="entry" valign="top" width="51.2396694214876%" id="d181858e134">Description</th>

            </tr>

          </thead>

          <tbody class="tbody">
            <tr class="row">
              <td class="entry" valign="top" width="48.760330578512395%" headers="d181858e131 ">gp_get_endpoints()<p class="p">
               <a class="xref" href="../ref_guide/system_catalogs/gp_endpoints.html#topic1">gp_endpoints</a></p>
</td>

              <td class="entry" valign="top" width="51.2396694214876%" headers="d181858e134 ">List the endpoints associated with all active parallel
                retrieve cursors declared by the current session user in the current
                database. When the Greenplum
                Database superuser invokes this function, it returns a list of all endpoints
                for all parallel retrieve cursors declared by all users in the current
                database.</td>

            </tr>

            <tr class="row">
              <td class="entry" valign="top" width="48.760330578512395%" headers="d181858e131 ">gp_get_session_endpoints()<p class="p">
                <a class="xref" href="../ref_guide/system_catalogs/gp_session_endpoints.html#topic1">gp_session_endpoints</a></p>
</td>

              <td class="entry" valign="top" width="51.2396694214876%" headers="d181858e134 ">List the endpoints associated with all parallel retrieve
                cursors declared in the current session for the current session user.</td>

            </tr>
    
            <tr class="row">
              <td class="entry" valign="top" width="48.760330578512395%" headers="d181858e131 ">gp_get_segment_endpoints()<p class="p">
                <a class="xref" href="../ref_guide/system_catalogs/gp_segment_endpoints.html#topic1">gp_segment_endpoints</a></p>
</td>

              <td class="entry" valign="top" width="51.2396694214876%" headers="d181858e134 ">List the endpoints created in the QE for all active
                parallel retrieve cursors declared by the current session user. When the
                Greenplum Database superuser accesses this view, it returns a list of all
                endpoints on the QE created for all parallel retrieve cursors declared by
                all users.</td>

            </tr>

            <tr class="row">
              <td class="entry" valign="top" width="48.760330578512395%" headers="d181858e131 ">gp_wait_parallel_retrieve_cursor(cursorname text, timeout_sec int4 )</td>

              <td class="entry" valign="top" width="51.2396694214876%" headers="d181858e134 ">Return cursor status or block and wait for results
                to be retrieved from all endpoints associated with the specified
                parallel retrieve cursor.</td>

            </tr>

          </tbody>
        </table>
    </div>

<div class="note">Each of these functions and views is located in the <code>pg_catalog</code> schema, and each <code>RETURNS TABLE</code>.</div>


## <a id="topic_using"></a>Using a Parallel Retrieve Cursor

You will perform the following tasks when you use a Greenplum Database parallel retrieve cursor to read query results in parallel from Greenplum segments:

1. [Declare the parallel retrieve cursor](#declare_cursor).
1. [List the endpoints of the parallel retrieve cursor](#list_endpoints).
1. [Open a retrieve connection to each endpoint](#open_retrieve_conn).
1. [Retrieve data from each endpoint](#retrieve_data).
1. [Wait for data retrieval to complete](#wait).
1. [Handle data retrieval errors](#error_handling).
1. [Close the parallel retrieve cursor](#close).

In addition to the above, you may optionally choose to [List all parallel retrieve cursors](#list_all_prc) in the system or [List segment-specific retrieve session information](#utility_endpoints).

### <a id="declare_cursor"></a>Declaring a Parallel Retrieve Cursor

You [DECLARE](../ref_guide/sql_commands/DECLARE.html#topic1) a cursor to retrieve a smaller number of rows at a time from a larger query. When you declare a parallel retrieve cursor, you can retrieve the query results directly from the Greenplum Database segments.

The syntax for declaring a parallel retrieve cursor is similar to that of declaring a regular cursor; you must additionally include the `PARALLEL RETRIEVE` keywords in the command.  You can declare a parallel retrieve cursor only within a transaction, and the cursor name that you specify when you declare the cursor must be unique within the transaction.

For example, the following commands begin a transaction and declare a parallel retrieve cursor named `prc1` to retrieve the results from a specific query:

``` sql
BEGIN;
DECLARE prc1 PARALLEL RETRIEVE CURSOR FOR <i>query</i>;
```

Greenplum Database creates the endpoint(s) on the QD or QEs, depending on the *query* parameters:

- Greenplum Database creates an endpoint on the QD when the query results must be gathered by the coordinator. For example, this `DECLARE` statement requires that the coordinator gather the query results:

    ``` sql
    DECLARE c1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1 ORDER BY a;
    ```
    <div class="note">You may choose to run the <code>EXPLAIN</code> command on the parallel retrieve cursor query to identify when motion is involved. Consider using a regular cursor for such queries.</div>

- When the query involves direct dispatch to a segment (the query is filtered on the distribution key), Greenplum Database creates the endpoint(s) on specific segment host(s). For example, this `DECLARE` statement may result in the creation of single endpoint:

    ``` sql
    DECLARE c2 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1 WHERE a=1;
    ```

- Greenplum Database creates the endpoints on all segment hosts when all hosts contribute to the query results. This example `DECLARE` statement results in all segments contributing query results:

    ``` sql
    DECLARE c3 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1;
    ```

The `DECLARE` command returns when the endpoints are ready and query execution has begun.

### <a id="list_endpoints"></a>Listing a Parallel Retrieve Cursor's Endpoints

You can obtain the information that you need to initiate a retrieve
            connection to an endpoint by invoking the `gp_get_endpoints()`
            function or examining the `gp_endpoints` view in a session on
            the Greenplum Database coordinator host:

``` sql
SELECT * FROM gp_get_endpoints();
SELECT * FROM gp_endpoints;
```

These commands return the list of endpoints in a table with the following columns:



<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic_using__gp_get_endpoints_table" class="table" frame="border" border="1" rules="all">


              <thead class="thead" align="left">
                <tr class="row">
                  <th class="entry" valign="top" width="100" id="d181858e379">Column Name</th>

                  <th class="entry" valign="top" width="NaN%" id="d181858e382">Description</th>

                </tr>

              </thead>

            <tbody class="tbody">
              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">gp_segment_id</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The QE's endpoint <samp class="ph codeph">gp_segment_id</samp>.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">auth_token</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The authentication token for a retrieve session.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">cursorname</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The name of the parallel retrieve cursor.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">sessionid</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The identifier of the session in which the parallel
                  retrieve cursor was created.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">hostname</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The name of the host from which to retrieve the data
                  for the endpoint.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">port</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The port number from which to retrieve the data for
                  the endpoint.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">username</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The name of the session user (not the current user);
                  <em class="ph i">you must initiate the retrieve session as this user</em>.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">state</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The state of the endpoint; the valid states are:
                  <p class="p">READY: The endpoint is ready to be retrieved.</p>

                  <p class="p">ATTACHED: The endpoint is attached to a retrieve connection.</p>

                  <p class="p">RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.</p>

                  <p class="p">FINISHED: The endpoint has been fully retrieved.</p>

                  <p class="p">RELEASED: Due to an error, the endpoint has been released and the connection
                    closed.</p>
</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e379 ">endpointname</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e382 ">The endpoint identifier; you provide this identifier
                  to the <samp class="ph codeph">RETRIEVE</samp> command.</td>

              </tr>

              </tbody>

            </table>
          </div>

Refer to the [gp_endpoints](../ref_guide/system_catalogs/gp_endpoints.html#topic1) view reference page for more information about the endpoint attributes returned by these commands.

You can similarly invoke the `gp_get_session_endpoints()` function or examine the `gp_session_endpoints` view to list the endpoints created for the parallel retrieve cursors declared in the current session and by the current user.

### <a id="open_retrieve_conn"></a>Opening a Retrieve Session

After you declare a parallel retrieve cursor, you can open a retrieve session to each endpoint. Only a single retrieve session may be open to an endpoint at any given time.

<div class="note">A retrieve session is independent of the parallel retrieve cursor itself and the endpoints.</div>

Retrieve session authentication does not depend on the `pg_hba.conf` file, but rather on an authentication token (`auth_token`) generated by Greenplum Database.

<div class="note">Because Greenplum Database skips <code>pg_hba.conf</code>-controlled authentication for a retrieve session, for security purposes you may invoke only the <code>RETRIEVE</code> command in the session.</div>

When you initiate a retrieve session to an endpoint:

- The user that you specify for the retrieve session must be the session user that declared the parallel retrieve cursor (the `username` returned by `gp_endpoints`). This user must have Greenplum Database login privileges.

- You specify the `hostname` and `port` returned by `gp_endpoints` for the endpoint.

- You authenticate the retrieve session by specifying the `auth_token` returned for the endpoint via the `PGPASSWORD` environment variable, or when prompted for the retrieve session `Password`.

- You must specify the [gp_retrieve_conn](../ref_guide/config_params/guc-list.html#gp_retrieve_conn) server configuration parameter on the connection request, and set the value to `true` .

For example, if you are initiating a retrieve session via `psql`:

``` shell
PGOPTIONS='-c gp_retrieve_conn=true' psql -h <hostname> -p <port> -U <username> -d <dbname>
```

To distinguish a retrieve session from other sessions running on a segment host, Greenplum Database includes the `[retrieve]` tag on the `ps` command output display for the process.

### <a id="retrieve_data"></a>Retrieving Data From the Endpoint

Once you establish a retrieve session, you retrieve the tuples associated with a query result on that endpoint using the [RETRIEVE](../ref_guide/sql_commands/RETRIEVE.html#topic1) command.

You can specify a (positive) number of rows to retrieve, or `ALL` rows:

``` sql
RETRIEVE 7 FROM ENDPOINT prc10000003300000003;
RETRIEVE ALL FROM ENDPOINT prc10000003300000003;
```

Greenplum Database returns an empty set if there are no more rows to retrieve from the endpoint.

<div class="note">You can retrieve from multiple parallel retrieve cursors from the same retrieve session only when their <code>auth_token</code>s match.</div>

### <a id="wait"></a>Waiting for Data Retrieval to Complete

Use the `gp_wait_parallel_retrieve_cursor()` function to display the the status of data retrieval from a parallel retrieve cursor, or to wait for all endpoints to finishing retrieving the data. You invoke this function in the transaction block in which you declared the parallel retrieve cursor.

`gp_wait_parallel_retrieve_cursor()` returns `true` only when all tuples are fully retrieved from all endpoints. In all other cases, the function returns `false` and may additionally throw an error.

The function signatures of `gp_wait_parallel_retrieve_cursor()` follow:

``` sql
gp_wait_parallel_retrieve_cursor( cursorname text )
gp_wait_parallel_retrieve_cursor( cursorname text, timeout_sec int4 )
```

You must identify the name of the cursor when you invoke this function.  The timeout argument is optional:

- The default timeout is `0` seconds: Greenplum Database checks the retrieval status of all endpoints and returns the result immediately.

- A timeout value of `-1` seconds instructs Greenplum to block until all data from all endpoints has been retrieved, or block until an error occurs.

- The function reports the retrieval status after a timeout occurs for any other positive timeout value that you specify.

`gp_wait_parallel_retrieve_cursor()` returns when it encounters one of the following conditions:

- All data has been retrieved from all endpoints.
- A timeout has occurred.
- An error has occurred.

### <a id="error_handling"></a>Handling Data Retrieval Errors

An error can occur in a retrieve sesson when:

- You cancel or interrupt the retrieve operation.
- The endpoint is only partially retrieved when the retrieve session quits.

When an error occurs in a specific retrieve session, Greenplum Database removes the endpoint from the QE. Other retrieve sessions continue to function as normal.

If you close the transaction before fully retrieving from all endpoints, or if `gp_wait_parallel_retrieve_cursor()` returns an error, Greenplum Database terminates all remaining open retrieve sessions.

### <a id="close"></a>Closing the Cursor

When you have completed retrieving data from the parallel retrieve cursor, close the cursor and end the transaction:

``` sql
CLOSE prc1;
END;
```

<div class="note">When you close a parallel retrieve cursor, Greenplum Database terminates any open retrieve sessions associated with the cursor.</div>

On closing, Greenplum Database frees all resources associated with the parallel retrieve cursor and its endpoints.

### <a id="list_all_prc"></a>Listing All Parallel Retrieve Cursors

The [pg_cursors](../ref_guide/system_catalogs/pg_cursors.html#topic1) view lists all declared cursors that are currently available in the system. You can obtain information about all parallel retrieve cursors by running the following command:

``` sql
SELECT * FROM pg_cursors WHERE is_parallel = true;
```

### <a id="utility_endpoints"></a>Listing Segment-Specific Retrieve Session Information

You can obtain information about all retrieve sessions to a specific QE endpoint by invoking the `gp_get_segment_endpoints()` function or examining the `gp_segment_endpoints` view:

``` sql
SELECT * FROM gp_get_segment_endpoints();
SELECT * FROM gp_segment_endpoints;
```

These commands provide information about the retrieve sessions associated with a QE endpoint for all active parallel retrieve cursors declared by the current session user. When the Greenplum Database superuser invokes the command, it returns the retrieve session information for all endpoints on the QE created for all parallel retrieve cursors declared by all users.

You can obtain segment-specific retrieve session information in two ways: from the QD, or via a utility-mode connection to the endpoint:

- QD example:

    ``` sql
    SELECT * from gp_dist_random('gp_segment_endpoints');
    ```

    Display the information filtered to a specific segment:

    ``` sql
    SELECT * from gp_dist_random('gp_segment_endpoints') WHERE gp_segment_id = 0;
    ```

- Example utilizing a utility-mode connection to the endpoint:

    ``` sql
    $ PGOPTIONS='-c gp_session_role=utility' psql -h sdw3 -U localuser -p 6001 -d testdb

    testdb=> SELECT * FROM gp_segment_endpoints;
    ```

The commands return endpoint and retrieve session information in a table with the following columns:

<div class="tablenoborder"> <table cellpadding="4" cellspacing="0" summary="" id="topic_using__gp_get_segment_endpoints_table" class="table" frame="border" border="1" rules="all">


              <thead class="thead" align="left">
                <tr class="row">
                  <th class="entry" valign="top" width="100" id="d181858e835">Column Name</th>

                  <th class="entry" valign="top" width="NaN%" id="d181858e838">Description</th>

                </tr>

              </thead>

            <tbody class="tbody">
              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">auth_token</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The authentication token for a the retrieve
                  session.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">databaseid</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The identifier of the database in which the parallel
                  retrieve cursor was created.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">senderpid</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The identifier of the process sending the query
                  results.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">receiverpid</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The process identifier of the retrieve session that is
                  receiving the query results.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">state</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The state of the endpoint; the valid states are:
                  <p class="p">READY: The endpoint is ready to be retrieved.</p>

                  <p class="p">ATTACHED: The endpoint is attached to a retrieve connection.</p>

                  <p class="p">RETRIEVING: A retrieve session is retrieving data from the endpoint at this
                    moment.</p>

                  <p class="p">FINISHED: The endpoint has been fully retrieved.</p>

                  <p class="p">RELEASED: Due to an error, the endpoint has been released and the
                    connection closed.</p>
</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">gp_segment_id</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The QE's endpoint <samp class="ph codeph">gp_segment_id</samp>.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">sessionid</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The identifier of the session in which the parallel
                  retrieve cursor was created.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">username</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The name of the session user that initiated the
                  retrieve session.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">endpointname</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The endpoint identifier.</td>

              </tr>

              <tr class="row">
                <td class="entry" valign="top" width="100" headers="d181858e835 ">cursorname</td>

                <td class="entry" valign="top" width="NaN%" headers="d181858e838 ">The name of the parallel retrieve cursor.</td>

              </tr>

              </tbody>

            </table>
            </div>

Refer to the [gp_segment_endpoints](../ref_guide/system_catalogs/gp_segment_endpoints.html#topic1) view reference page for more information about the endpoint attributes returned by these commands.


## <a id="topic_limits"></a>Known Issues and Limitations

The parallel retrieve cursor implementation has the following limitations:

- The Pivotal Query Optimizer (GPORCA) does not support queries on a parallel retrieve cursor.
- Greenplum Database ignores the `BINARY` clause when you declare a parallel retrieve cursor.
- Parallel retrieve cursors cannot be declared `WITH HOLD`.
- Parallel retrieve cursors do not support the `FETCH` and `MOVE` cursor operations.
- Parallel retrieve cursors are not supported in SPI; you cannot declare a parallel retrieve cursor in a PL/pgSQL function.


## <a id="topic_addtldocs"></a>Additional Documentation

Refer to the [README](https://github.com/greenplum-db/gpdb/tree/master/src/backend/cdb/endpoint/README) in the Greenplum Database `github` repository for additional information about the parallel retrieve cursor implementation.  You can also find parallel retrieve cursor [programming examples](https://github.com/greenplum-db/gpdb/tree/master/src/test/examples/) in the repository.


## <a id="topic_examples"></a>Example

Create a parallel retrieve cursor and use it to pull query results from a Greenplum Database cluster:

1. Open a `psql` session to the Greenplum Database coordinator host:

    ``` shell
    psql -d testdb
    ```

1. Start the transaction:

    ``` sql
    BEGIN;
    ```

1. Declare a parallel retrieve cursor named `prc1` for a `SELECT *` query on a table:

    ``` sql
    DECLARE prc1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM t1;
    ```

1. Obtain the endpoints for this parallel retrieve cursor:

    ``` sql
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

1. Wait until all endpoints are fully retrieved:

    ``` sql
    SELECT gp_wait_parallel_retrieve_cursor( 'prc1', -1 );
    ```

1. For each endpoint:

    1. Open a retrieve session. For example, to open a retrieve session to the segment instance running on `sdw3`, port number `6001`, run the following command in a *different terminal window*; when prompted for the password, provide the `auth_token` identified in row 7 of the `gp_endpoints` output:

        ``` sql
        $ PGOPTIONS='-c gp_retrieve_conn=true' psql -h sdw3 -U localuser -p 6001 -d testdb
        Password:
        ````

    1. Retrieve data from the endpoint:

        ``` sql
        -- Retrieve 7 rows of data from this session
        RETRIEVE 7 FROM ENDPOINT prc10000003300000003
        -- Retrieve the remaining rows of data from this session
        RETRIEVE ALL FROM ENDPOINT prc10000003300000003
        ```

    1. Exit the retrieve session:

        ``` sql
        \q
        ```

1. In the original `psql` session (the session in which you declared the parallel retrieve cursor), verify that the `gp_wait_parallel_retrieve_cursor()` function returned `t`. Then close the cursor and complete the transaction:

    ``` sql
    CLOSE prc1;
    END;
    ```

