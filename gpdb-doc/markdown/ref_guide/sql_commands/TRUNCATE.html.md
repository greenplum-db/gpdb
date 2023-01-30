# TRUNCATE 

Empties a table of all rows.

> **Note** Greenplum Database does not enforce referential integrity syntax \(foreign key constraints\). As of version 6.12 `TRUNCATE` truncates a table that is referenced in a foreign key constraint even if the `CASCADE` option is omitted.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
TRUNCATE [TABLE] [ONLY] <name> [ * ] [, ...] 
    [ RESTART IDENTITY | CONTINUE IDENTITY ] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`TRUNCATE` quickly removes all rows from a table or set of tables. It has the same effect as an unqualified [DELETE](DELETE.html) on each table, but since it does not actually scan the tables it is faster. This is most useful on large tables.

You must have the `TRUNCATE` privilege on the table to truncate table rows.

`TRUNCATE` acquires an access exclusive lock on the tables it operates on, which blocks all other concurrent operations on the table. When `RESTART IDENTITY` is specified, any sequences that are to be restarted are likewise locked exclusively. If concurrent access to a table is required, then the `DELETE` command should be used instead.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of a table to truncate. If `ONLY` is specified before the table name, only that table is truncated. If `ONLY` is not specified, the table and all its descendant tables \(if any\) are truncated. Optionally, `*` can be specified after the table name to explicitly indicate that descendant tables are included.

CASCADE
:   Because this key word applies to foreign key references \(which are not supported in Greenplum Database\) it has no effect.

RESTART IDENTITY
:   Automatically restart sequences owned by columns of the truncated table\(s\).

CONTINUE IDENTITY
:   Do not change the values of sequences. This is the default.

RESTRICT
:   Because this key word applies to foreign key references \(which are not supported in Greenplum Database\) it has no effect.

## <a id="section5"></a>Notes 

`TRUNCATE` will not run any user-defined `ON DELETE` triggers that might exist for the tables.

`TRUNCATE` will not truncate any tables that inherit from the named table. Only the named table is truncated, not its child tables.

`TRUNCATE` will not truncate any sub-tables of a partitioned table. If you specify a sub-table of a partitioned table, `TRUNCATE` will not remove rows from the sub-table and its child tables.

`TRUNCATE` is not MVCC-safe. After truncation, the table will appear empty to concurrent transactions, if they are using a snapshot taken before the truncation occurred.

`TRUNCATE` is transaction-safe with respect to the data in the tables: the truncation will be safely rolled back if the surrounding transaction does not commit.

`TRUNCATE` acquires an `ACCESS EXCLUSIVE` lock on each table it operates on, which blocks all other concurrent operations on the table. If concurrent access to a table is required, then the `DELETE` command should be used instead.

When `RESTART IDENTITY` is specified, the implied `ALTER SEQUENCE RESTART` operations are also done transactionally; that is, they will be rolled back if the surrounding transaction does not commit. This is unlike the normal behavior of `ALTER SEQUENCE RESTART`. Be aware that if any additional sequence operations are done on the restarted sequences before the transaction rolls back, the effects of these operations on the sequences will be rolled back, but not their effects on `currval()`; that is, after the transaction `currval()` will continue to reflect the last sequence value obtained inside the failed transaction, even though the sequence itself may no longer be consistent with that. This is similar to the usual behavior of `currval()` after a failed transaction.

## <a id="section6"></a>Examples 

Empty the tables `films` and `distributors`:

```
TRUNCATE films, distributors;
```

The same, and also reset any associated sequence generators:

```
TRUNCATE films, distributors RESTART IDENTITY;
```

## <a id="section7"></a>Compatibility 

The SQL:2008 standard includes a `TRUNCATE` command with the syntax `TRUNCATE TABLE tablename`. The clauses `CONTINUE IDENTITY`/`RESTART IDENTITY` also appear in that standard, but have slightly different though related meanings. Some of the concurrency behavior of this command is left implementation-defined by the standard, so the above notes should be considered and compared with other implementations if necessary.

## <a id="section8"></a>See Also 

[DELETE](DELETE.html), [DROP TABLE](DROP_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

