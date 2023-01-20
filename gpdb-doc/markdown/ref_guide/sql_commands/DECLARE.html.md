# DECLARE 

Defines a cursor.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DECLARE <name> [BINARY] [INSENSITIVE] [NO SCROLL] [PARALLEL RETRIEVE] CURSOR 
     [{WITH | WITHOUT} HOLD] 
     FOR <query> [FOR READ ONLY]
```

## <a id="section3"></a>Description 

`DECLARE` allows a user to create a cursor, which can be used to retrieve a small number of rows at a time out of a larger query. Cursors can return data either in text or in binary format using [FETCH](FETCH.html).

> **Note** This page describes usage of cursors at the SQL command level. If you are trying to use cursors inside a PL/pgSQL function, the rules are different, see [PL/pgSQL](../../analytics/pl_sql.html).

Normal cursors return data in text format, the same as a `SELECT` would produce. Since data is stored natively in binary format, the system must do a conversion to produce the text format. Once the information comes back in text form, the client application may need to convert it to a binary format to manipulate it. In addition, data in the text format is often larger in size than in the binary format. Binary cursors return the data in a binary representation that may be more easily manipulated. Nevertheless, if you intend to display the data as text anyway, retrieving it in text form will save you some effort on the client side.

As an example, if a query returns a value of one from an integer column, you would get a string of 1 with a default cursor whereas with a binary cursor you would get a 4-byte field containing the internal representation of the value \(in big-endian byte order\).

Binary cursors should be used carefully. Many applications, including psql, are not prepared to handle binary cursors and expect data to come back in the text format.

> **Note** When the client application uses the 'extended query' protocol to issue a `FETCH` command, the Bind protocol message specifies whether data is to be retrieved in text or binary format. This choice overrides the way that the cursor is defined. The concept of a binary cursor as such is thus obsolete when using extended query protocol — any cursor can be treated as either text or binary.

A cursor can be specified in the `WHERE CURRENT OF` clause of the [UPDATE](UPDATE.html) or [DELETE](DELETE.html) statement to update or delete table data. The `UPDATE` or `DELETE` statement can only be run on the server, for example in an interactive psql session or a script. Language extensions such as PL/pgSQL do not have support for updatable cursors.

**Parallel Retrieve Cursors**

Greenplum Database supports a special type of cursor, a parallel retrieve cursor. You can use a parallel retrieve cursor to retrieve query results, in parallel, directly from the Greenplum Database segments, bypassing the Greenplum coordinator.

Parallel retrieve cursors do not support the `WITH HOLD` clause. Greenplum Database ignores the `BINARY` clause when you declare a parallel retrieve cursor.

You open a special retrieve session to each parallel retrieve cursor endpoint, and use the [RETRIEVE](RETRIEVE.html) command to retrieve the query results from a parallel retrieve cursor.

## <a id="section4"></a>Parameters 

name
:   The name of the cursor to be created.

BINARY
:   Causes the cursor to return data in binary rather than in text format.

    > **Note** Greenplum Database ignores the `BINARY` clause when you declare a `PARALLEL RETRIEVE` cursor.

INSENSITIVE
:   Indicates that data retrieved from the cursor should be unaffected by updates to the tables underlying the cursor while the cursor exists. In Greenplum Database, all cursors are insensitive. This key word currently has no effect and is present for compatibility with the SQL standard.

NO SCROLL
:   A cursor cannot be used to retrieve rows in a nonsequential fashion. This is the default behavior in Greenplum Database, since scrollable cursors \(`SCROLL`\) are not supported.

PARALLEL RETRIEVE
:   Declare a parallel retrieve cursor. A parallel retrieve cursor is a special type of cursor that you can use to retrieve results directly from Greenplum Database segments, in parallel.

WITH HOLD
WITHOUT HOLD
:   `WITH HOLD` specifies that the cursor may continue to be used after the transaction that created it successfully commits. `WITHOUT HOLD` specifies that the cursor cannot be used outside of the transaction that created it. `WITHOUT HOLD` is the default.

    > **Note** Greenplum Database does not support declaring a `PARALLEL RETRIEVE` cursor with the `WITH HOLD` clause. `WITH HOLD` also cannot not be specified when the `query` includes a `FOR UPDATE` or `FOR SHARE` clause.

query
:   A [SELECT](SELECT.html) or [VALUES](VALUES.html) command which will provide the rows to be returned by the cursor.

    If the cursor is used in the `WHERE CURRENT OF` clause of the [UPDATE](UPDATE.html) or [DELETE](DELETE.html) command, the `SELECT` command must satisfy the following conditions:

    -   Cannot reference a view or external table.
    -   References only one table.
        <br/><br/>The table must be updatable. For example, the following are not updatable: table functions, set-returning functions, append-only tables, columnar tables.

    -   Cannot contain any of the following:
        -   A grouping clause
        -   A set operation such as `UNION ALL` or `UNION DISTINCT`
        -   A sorting clause
        -   A windowing clause
        -   A join or a self-join
        <br/><br/>Specifying the `FOR UPDATE` clause in the `SELECT` command prevents other sessions from changing the rows between the time they are fetched and the time they are updated. Without the `FOR UPDATE` clause, a subsequent use of the `UPDATE` or `DELETE` command with the `WHERE CURRENT OF` clause has no effect if the row was changed since the cursor was created.
        <br/><br/>> **Note** Specifying the `FOR UPDATE` clause in the `SELECT` command locks the entire table, not just the selected rows.


FOR READ ONLY
:   `FOR READ ONLY` indicates that the cursor is used in a read-only mode.

## <a id="section5"></a>Notes 

Unless `WITH HOLD` is specified, the cursor created by this command can only be used within the current transaction. Thus, `DECLARE` without `WITH HOLD` is useless outside a transaction block: the cursor would survive only to the completion of the statement. Therefore Greenplum Database reports an error if this command is used outside a transaction block. Use `BEGIN` and `COMMIT` \(or `ROLLBACK`\) to define a transaction block.

If `WITH HOLD` is specified and the transaction that created the cursor successfully commits, the cursor can continue to be accessed by subsequent transactions in the same session. \(But if the creating transaction ends prematurely, the cursor is removed.\) A cursor created with `WITH HOLD` is closed when an explicit `CLOSE` command is issued on it, or the session ends. In the current implementation, the rows represented by a held cursor are copied into a temporary file or memory area so that they remain available for subsequent transactions.

If you create a cursor with the `DECLARE` command in a transaction, you cannot use the `SET` command in the transaction until you close the cursor with the `CLOSE` command.

Scrollable cursors are not currently supported in Greenplum Database. You can only use `FETCH` or `RETRIEVE` to move the cursor position forward, not backwards.

`DECLARE...FOR UPDATE` is not supported with append-optimized tables.

You can see all available cursors by querying the [pg\_cursors](../system_catalogs/pg_cursors.html) system view.

## <a id="section6"></a>Examples 

Declare a cursor:

```
DECLARE mycursor CURSOR FOR SELECT * FROM mytable;
```

Declare a parallel retrieve cursor for the same query:

```
DECLARE myprcursor PARALLEL RETRIEVE CURSOR FOR SELECT * FROM mytable;
```

## <a id="section7"></a>Compatibility 

SQL standard allows cursors only in embedded SQL and in modules. Greenplum Database permits cursors to be used interactively.

Greenplum Database does not implement an `OPEN` statement for cursors. A cursor is considered to be open when it is declared.

The SQL standard allows cursors to move both forward and backward. All Greenplum Database cursors are forward moving only \(not scrollable\).

Binary cursors are a Greenplum Database extension.

The SQL standard makes no provisions for parallel retrieve cursors.

## <a id="section8"></a>See Also 

[CLOSE](CLOSE.html), [DELETE](DELETE.html), [FETCH](FETCH.html), [MOVE](MOVE.html), [RETRIEVE](RETRIEVE.html), [SELECT](SELECT.html), [UPDATE](UPDATE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

