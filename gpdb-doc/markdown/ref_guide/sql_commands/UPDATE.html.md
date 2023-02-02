# UPDATE 

Updates rows of a table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
UPDATE [ONLY] <table> [[AS] <alias>]
   SET {<column> = {<expression> | DEFAULT} |
   (<column> [, ...]) = ({<expression> | DEFAULT} [, ...])} [, ...]
   [FROM <fromlist>]
   [WHERE <condition >| WHERE CURRENT OF <cursor_name> ]
```

## <a id="section3"></a>Description 

`UPDATE` changes the values of the specified columns in all rows that satisfy the condition. Only the columns to be modified need be mentioned in the `SET` clause; columns not explicitly modified retain their previous values.

By default, `UPDATE` will update rows in the specified table and all its subtables. If you wish to only update the specific table mentioned, you must use the `ONLY` clause.

There are two ways to modify a table using information contained in other tables in the database: using sub-selects, or specifying additional tables in the `FROM` clause. Which technique is more appropriate depends on the specific circumstances.

If the `WHERE CURRENT OF` clause is specified, the row that is updated is the one most recently fetched from the specified cursor.

The `WHERE CURRENT OF` clause is not supported with replicated tables.

You must have the `UPDATE` privilege on the table, or at least on the column\(s\) that are listed to be updated. You must also have the `SELECT` privilege on any column whose values are read in the expressions or condition.

> **Note** As the default, Greenplum Database acquires an `EXCLUSIVE` lock on tables for `UPDATE` operations on heap tables. When the Global Deadlock Detector is enabled, the lock mode for `UPDATE` operations on heap tables is `ROW EXCLUSIVE`. See [Global Deadlock Detector](../../admin_guide/dml.html#topic_gdd).

**Outputs**

On successful completion, an `UPDATE` command returns a command tag of the form:

```
UPDATE <count>
```

where count is the number of rows updated. If count is 0, no rows matched the condition \(this is not considered an error\).

## <a id="section5"></a>Parameters 

with\_query
:   The `WITH` clause allows you to specify one or more subqueries that can be referenced by name in the `UPDATE` query.

:   For an `UPDATE` command that includes a `WITH` clause, the clause can only contain `SELECT` commands, the `WITH` clause cannot contain a data-modifying command \(`INSERT`, `UPDATE`, or `DELETE`\).

:   It is possible for the query \(`SELECT` statement\) to also contain a `WITH` clause. In such a case both sets of with\_query can be referenced within the `UPDATE` query, but the second one takes precedence since it is more closely nested.

:   See [WITH Queries \(Common Table Expressions\)](../../admin_guide/query/topics/CTE-query.html#topic_zhs_r1s_w1b) and [SELECT](SELECT.html) for details.

ONLY
:   If specified, update rows from the named table only. When not specified, any tables inheriting from the named table are also processed.

table
:   The name \(optionally schema-qualified\) of an existing table.

alias
:   A substitute name for the target table. When an alias is provided, it completely hides the actual name of the table. For example, given `UPDATE foo AS f`, the remainder of the `UPDATE` statement must refer to this table as `f` not `foo`.

column
:   The name of a column in table. The column name can be qualified with a subfield name or array subscript, if needed. Do not include the table's name in the specification of a target column.

expression
:   An expression to assign to the column. The expression may use the old values of this and other columns in the table.

DEFAULT
:   Set the column to its default value \(which will be NULL if no specific default expression has been assigned to it\).

fromlist
:   A list of table expressions, allowing columns from other tables to appear in the `WHERE` condition and the update expressions. This is similar to the list of tables that can be specified in the `FROM` clause of a `SELECT` statement. Note that the target table must not appear in the fromlist, unless you intend a self-join \(in which case it must appear with an alias in the fromlist\).

condition
:   An expression that returns a value of type boolean. Only rows for which this expression returns true will be updated.

cursor\_name
:   The name of the cursor to use in a `WHERE CURRENT OF` condition. The row to be updated is the one most recently fetched from the cursor. The cursor must be a non-grouping query on the `UPDATE` command target table. See [DECLARE](DECLARE.html) for more information about creating cursors.

:   `WHERE CURRENT OF` cannot be specified together with a Boolean condition.

:   Note that `WHERE CURRENT OF` cannot be specified together with a Boolean condition. The `UPDATE...WHERE CURRENT OF` statement can only be run on the server, for example in an interactive psql session or a script. Language extensions such as PL/pgSQL do not have support for updatable cursors.

:   See [DECLARE](DECLARE.html) for more information about creating cursors.

output\_expression
:   An expression to be computed and returned by the `UPDATE` command after each row is updated. The expression may use any column names of the table or table\(s\) listed in `FROM`. Write `*` to return all columns.

output\_name
:   A name to use for a returned column.

## <a id="section6"></a>Notes 

`SET` is not allowed on the Greenplum distribution key columns of a table.

When a `FROM` clause is present, what essentially happens is that the target table is joined to the tables mentioned in the from list, and each output row of the join represents an update operation for the target table. When using `FROM` you should ensure that the join produces at most one output row for each row to be modified. In other words, a target row should not join to more than one row from the other table\(s\). If it does, then only one of the join rows will be used to update the target row, but which one will be used is not readily predictable.

Because of this indeterminacy, referencing other tables only within sub-selects is safer, though often harder to read and slower than using a join.

Running `UPDATE` and `DELETE` commands directly on a specific partition \(child table\) of a partitioned table is not supported. Instead, run these commands on the root partitioned table, the table created with the `CREATE TABLE` command.

For a partitioned table, all the child tables are locked during the `UPDATE` operation when the Global Deadlock Detector is not enabled \(the default\). Only some of the leaf child tables are locked when the Global Deadlock Detector is enabled. For information about the Global Deadlock Detector, see [Global Deadlock Detector](../../admin_guide/dml.html#topic_gdd).

## <a id="section7"></a>Examples 

Change the word `Drama` to `Dramatic` in the column `kind` of the table `films`:

```
UPDATE films SET kind = 'Dramatic' WHERE kind = 'Drama';
```

Adjust temperature entries and reset precipitation to its default value in one row of the table `weather`:

```
UPDATE weather SET temp_lo = temp_lo+1, temp_hi = 
temp_lo+15, prcp = DEFAULT
WHERE city = 'San Francisco' AND date = '2016-07-03';
```

Use the alternative column-list syntax to do the same update:

```
UPDATE weather SET (temp_lo, temp_hi, prcp) = (temp_lo+1, 
temp_lo+15, DEFAULT)
WHERE city = 'San Francisco' AND date = '2016-07-03';
```

Increment the sales count of the salesperson who manages the account for Acme Corporation, using the `FROM` clause syntax \(assuming both tables being joined are distributed in Greenplum Database on the `id` column\):

```
UPDATE employees SET sales_count = sales_count + 1 FROM 
accounts
WHERE accounts.name = 'Acme Corporation'
AND employees.id = accounts.id;
```

Perform the same operation, using a sub-select in the `WHERE` clause:

```
UPDATE employees SET sales_count = sales_count + 1 WHERE id =
  (SELECT id FROM accounts WHERE name = 'Acme Corporation');
```

Attempt to insert a new stock item along with the quantity of stock. If the item already exists, instead update the stock count of the existing item. To do this without failing the entire transaction, use savepoints.

```
BEGIN;
-- other operations
SAVEPOINT sp1;
INSERT INTO wines VALUES('Chateau Lafite 2003', '24');
-- Assume the above fails because of a unique key violation,
-- so now we issue these commands:
ROLLBACK TO sp1;
UPDATE wines SET stock = stock + 24 WHERE winename = 'Chateau 
Lafite 2003';
-- continue with other operations, and eventually
COMMIT;
```

## <a id="section8"></a>Compatibility 

This command conforms to the SQL standard, except that the `FROM` clause is a Greenplum Database extension.

According to the standard, the column-list syntax should allow a list of columns to be assigned from a single row-valued expression, such as a sub-select:

```
UPDATE accounts SET (contact_last_name, contact_first_name) =
    (SELECT last_name, first_name FROM salesmen
     WHERE salesmen.id = accounts.sales_id);
```

This is not currently implemented — the source must be a list of independent expressions.

Some other database systems offer a `FROM` option in which the target table is supposed to be listed again within `FROM`. That is not how Greenplum Database interprets `FROM`. Be careful when porting applications that use this extension.

## <a id="section9"></a>See Also 

[DECLARE](DECLARE.html), [DELETE](DELETE.html), [SELECT](SELECT.html), [INSERT](INSERT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

