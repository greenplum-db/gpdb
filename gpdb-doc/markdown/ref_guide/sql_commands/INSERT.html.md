# INSERT 

Creates new rows in a table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
INSERT INTO <table> [( <column> [, ...] )]
   {DEFAULT VALUES | VALUES ( {<expression> | DEFAULT} [, ...] ) [, ...] | <query>}
   [RETURNING * | <output_expression> [[AS] <output_name>] [, ...]]
```

## <a id="section3"></a>Description 

`INSERT` inserts new rows into a table. One can insert one or more rows specified by value expressions, or zero or more rows resulting from a query.

The target column names may be listed in any order. If no list of column names is given at all, the default is the columns of the table in their declared order. The values supplied by the `VALUES` clause or query are associated with the explicit or implicit column list left-to-right.

Each column not present in the explicit or implicit column list will be filled with a default value, either its declared default value or null if there is no default.

If the expression for any column is not of the correct data type, automatic type conversion will be attempted.

The optional `RETURNING` clause causes `INSERT` to compute and return value\(s\) based on each row actually inserted. This is primarily useful for obtaining values that were supplied by defaults, such as a serial sequence number. However, any expression using the table's columns is allowed. The syntax of the `RETURNING` list is identical to that of the output list of `SELECT`.

You must have `INSERT` privilege on a table in order to insert into it. When a column list is specified, you need `INSERT` privilege only on the listed columns. Use of the `RETURNING` clause requires `SELECT` privilege on all columns mentioned in `RETURNING`. If you provide a query to insert rows from a query, you must have `SELECT` privilege on any table or column referenced in the query.

**Outputs**

On successful completion, an `INSERT` command returns a command tag of the form:

```
INSERT <oid> <count>
```

The count is the number of rows inserted. If count is exactly one, and the target table has OIDs, then oid is the OID assigned to the inserted row. Otherwise OID is zero.

## <a id="section5"></a>Parameters 

with\_query
:   The `WITH` clause allows you to specify one or more subqueries that can be referenced by name in the `INSERT` query.

:   For an `INSERT` command that includes a `WITH` clause, the clause can only contain `SELECT` statements, the `WITH` clause cannot contain a data-modifying command \(`INSERT`, `UPDATE`, or `DELETE`\).

:   It is possible for the query \(`SELECT` statement\) to also contain a `WITH` clause. In such a case both sets of with\_query can be referenced within the `INSERT` query, but the second one takes precedence since it is more closely nested.

:   See [WITH Queries \(Common Table Expressions\)](../../admin_guide/query/topics/CTE-query.html#topic_zhs_r1s_w1b) and [SELECT](SELECT.html) for details.

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

output\_expression
:   An expression to be computed and returned by the `INSERT` command after each row is inserted. The expression can use any column names of the table. Write \* to return all columns of the inserted row\(s\).

output\_name
:   A name to use for a returned column.

## <a id="section6"></a>Notes 

To insert data into a partitioned table, you specify the root partitioned table, the table created with the `CREATE TABLE` command. You also can specify a leaf child table of the partitioned table in an `INSERT` command. An error is returned if the data is not valid for the specified leaf child table. Specifying a child table that is not a leaf child table in the `INSERT` command is not supported. Execution of other DML commands such as `UPDATE` and `DELETE` on any child table of a partitioned table is not supported. These commands must be run on the root partitioned table, the table created with the `CREATE TABLE` command.

For a partitioned table, all the child tables are locked during the `INSERT` operation when the Global Deadlock Detector is not enabled \(the default\). Only some of the leaf child tables are locked when the Global Deadlock Detector is enabled. For information about the Global Deadlock Detector, see [Global Deadlock Detector](../../admin_guide/dml.html#topic_gdd).

For append-optimized tables, Greenplum Database supports a maximum of 127 concurrent `INSERT` transactions into a single append-optimized table.

For writable S3 external tables, the `INSERT` operation uploads to one or more files in the configured S3 bucket, as described in [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html#amazon-emr). Pressing `Ctrl-c` cancels the `INSERT` and stops uploading to S3.

## <a id="section7"></a>Examples 

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

Insert a single row into table distributors, returning the sequence number generated by the DEFAULT clause:

```
INSERT INTO distributors (did, dname) VALUES (DEFAULT, 'XYZ Widgets')
   RETURNING did;
```

## <a id="section8"></a>Compatibility 

`INSERT` conforms to the SQL standard. The case in which a column name list is omitted, but not all the columns are filled from the `VALUES` clause or query, is disallowed by the standard.

Possible limitations of the query clause are documented under `SELECT`.

## <a id="section9"></a>See Also 

[COPY](COPY.html), [SELECT](SELECT.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html#amazon-emr)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

