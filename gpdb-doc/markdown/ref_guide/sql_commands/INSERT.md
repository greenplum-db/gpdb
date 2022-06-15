# INSERT 

Creates new rows in a table.

## Synopsis 

``` {#sql_command_synopsis}
INSERT INTO <table> [( <column> [, ...] )]
   {DEFAULT VALUES | VALUES ( {<expression> | DEFAULT} [, ...] ) 
   [, ...] | <query>}
```

## Description 

`INSERT` inserts new rows into a table. One can insert one or more rows specified by value expressions, or zero or more rows resulting from a query.

The target column names may be listed in any order. If no list of column names is given at all, the default is the columns of the table in their declared order. The values supplied by the `VALUES` clause or query are associated with the explicit or implicit column list left-to-right.

Each column not present in the explicit or implicit column list will be filled with a default value, either its declared default value or null if there is no default.

If the expression for any column is not of the correct data type, automatic type conversion will be attempted.

You must have `INSERT` privilege on a table in order to insert into it.

**Outputs**

On successful completion, an `INSERT` command returns a command tag of the form:

```
INSERT <oid> <count>
```

The count is the number of rows inserted. If count is exactly one, and the target table has OIDs, then oid is the OID assigned to the inserted row. Otherwise oid is zero.

## Parameters 

table
:   The name \(optionally schema-qualified\) of an existing table.

column
:   The name of a column in table. The column name can be qualified with a subfield name or array subscript, if needed. \(Inserting into only some fields of a composite column leaves the other fields null.\)

DEFAULT VALUES
:   All columns will be filled with their default values.

expression
:   An expression or value to assign to the corresponding column.

DEFAULT
:   The corresponding column will be filled with its default value.

query
:   A query \(`SELECT` statement\) that supplies the rows to be inserted. Refer to the `SELECT` statement for a description of the syntax.

## Notes 

To insert data into a partitioned table, you specify the root partitioned table, the table created with the `CREATE TABLE` command. You also can specify a leaf child table of the partitioned table in an `INSERT` command. An error is returned if the data is not valid for the specified leaf child table. Specifying a child table that is not a leaf child table in the `INSERT` command is not supported. Execution of other DML commands such as `UPDATE` and `DELETE` on any child table of a partitioned table is not supported. These commands must be executed on the root partitioned table, the table created with the `CREATE TABLE` command.

For a partitioned table, all the child tables are locked during the `INSERT` operation.

For append-optimized tables, Greenplum Database supports a maximum of 127 concurrent `INSERT` transactions into a single append-optimized table.

For writable S3 external tables, the `INSERT` operation uploads to one or more files in the configured S3 bucket, as described in [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html). Pressing `Ctrl-c` cancels the `INSERT` and stops uploading to S3.

## Examples 

Insert a single row into table `films`:

```
INSERT INTO films VALUES ('UA502', 'Bananas', 105, 
'1971-07-13', 'Comedy', '82 minutes');
```

In this example, the `length` column is omitted and therefore it will have the default value:

```
INSERT INTO films (code, title, did, date_prod, kind) VALUES 
('T_601', 'Yojimbo', 106, '1961-06-16', 'Drama');
```

This example uses the `DEFAULT` clause for the `date_prod` column rather than specifying a value:

```
INSERT INTO films VALUES ('UA502', 'Bananas', 105, DEFAULT, 
'Comedy', '82 minutes');
```

To insert a row consisting entirely of default values:

```
INSERT INTO films DEFAULT VALUES;
```

To insert multiple rows using the multirow `VALUES` syntax:

```
INSERT INTO films (code, title, did, date_prod, kind) VALUES
    ('B6717', 'Tampopo', 110, '1985-02-10', 'Comedy'),
    ('HG120', 'The Dinner Game', 140, DEFAULT, 'Comedy');
```

This example inserts some rows into table `films` from a table `tmp_films` with the same column layout as `films`:

```
INSERT INTO films SELECT * FROM tmp_films WHERE date_prod < 
'2004-05-07';
```

## Compatibility 

`INSERT` conforms to the SQL standard. The case in which a column name list is omitted, but not all the columns are filled from the `VALUES` clause or query, is disallowed by the standard.

Possible limitations of the query clause are documented under `SELECT`.

## See Also 

[COPY](COPY.html), [SELECT](SELECT.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

