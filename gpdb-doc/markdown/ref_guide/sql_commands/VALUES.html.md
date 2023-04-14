# VALUES 

Computes a set of rows.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
VALUES ( <expression> [, ...] ) [, ...]
   [ORDER BY <sort_expression> [ ASC | DESC | USING <operator> ] [, ...] ]
   [LIMIT { <count> | ALL } ] 
   [OFFSET <start> [ ROW | ROWS ] ]
   [FETCH { FIRST | NEXT } [<count> ] { ROW | ROWS } ONLY ]
```

## <a id="section3"></a>Description 

`VALUES` computes a row value or set of row values specified by value expressions. It is most commonly used to generate a "constant table" within a larger command, but it can be used on its own.

When more than one row is specified, all the rows must have the same number of elements. The data types of the resulting table's columns are determined by combining the explicit or inferred types of the expressions appearing in that column, using the same rules as for `UNION`.

Within larger commands, `VALUES` is syntactically allowed anywhere that `SELECT` is. Because it is treated like a `SELECT` by the grammar, it is possible to use the `ORDER BY`, `LIMIT` \(or equivalent `FETCH FIRST`\), and `OFFSET` clauses with a `VALUES` command.

## <a id="section4"></a>Parameters 

expression
:   A constant or expression to compute and insert at the indicated place in the resulting table \(set of rows\). In a `VALUES` list appearing at the top level of an `INSERT`, an expression can be replaced by `DEFAULT` to indicate that the destination column's default value should be inserted. `DEFAULT` cannot be used when `VALUES` appears in other contexts.

sort\_expression
:   An expression or integer constant indicating how to sort the result rows. This expression may refer to the columns of the `VALUES` result as `column1`, `column2`, etc. For more details, see "The ORDER BY Clause" in the parameters for [SELECT](SELECT.html).

operator
:   A sorting operator. For more details, see "The ORDER BY Clause" in the parameters for [SELECT](SELECT.html).

LIMIT count
OFFSET start
:   The maximum number of rows to return. For more details, see "The LIMIT Clause" in the parameters for [SELECT](SELECT.html).

## <a id="section5"></a>Notes 

`VALUES` lists with very large numbers of rows should be avoided, as you may encounter out-of-memory failures or poor performance. `VALUES` appearing within `INSERT` is a special case \(because the desired column types are known from the `INSERT`'s target table, and need not be inferred by scanning the `VALUES` list\), so it can handle larger lists than are practical in other contexts.

## <a id="section6"></a>Examples 

A bare `VALUES` command:

```
VALUES (1, 'one'), (2, 'two'), (3, 'three');
```

This will return a table of two columns and three rows. It is effectively equivalent to:

```
SELECT 1 AS column1, 'one' AS column2
UNION ALL
SELECT 2, 'two'
UNION ALL
SELECT 3, 'three';
```

More usually, `VALUES` is used within a larger SQL command. The most common use is in `INSERT`:

```
INSERT INTO films (code, title, did, date_prod, kind)
    VALUES ('T_601', 'Yojimbo', 106, '1961-06-16', 'Drama');
```

In the context of `INSERT`, entries of a `VALUES` list can be `DEFAULT` to indicate that the column default should be used here instead of specifying a value:

```
INSERT INTO films VALUES
    ('UA502', 'Bananas', 105, DEFAULT, 'Comedy', '82 
minutes'),
    ('T_601', 'Yojimbo', 106, DEFAULT, 'Drama', DEFAULT);
```

`VALUES` can also be used where a sub-`SELECT` might be written, for example in a `FROM` clause:

```
SELECT f.* FROM films f, (VALUES('MGM', 'Horror'), ('UA', 
'Sci-Fi')) AS t (studio, kind) WHERE f.studio = t.studio AND 
f.kind = t.kind;
UPDATE employees SET salary = salary * v.increase FROM 
(VALUES(1, 200000, 1.2), (2, 400000, 1.4)) AS v (depno, 
target, increase) WHERE employees.depno = v.depno AND 
employees.sales >= v.target;
```

Note that an `AS` clause is required when `VALUES` is used in a `FROM` clause, just as is true for `SELECT`. It is not required that the `AS` clause specify names for all the columns, but it is good practice to do so. The default column names for `VALUES` are `column1`, `column2`, etc. in Greenplum Database, but these names might be different in other database systems.

When `VALUES` is used in `INSERT`, the values are all automatically coerced to the data type of the corresponding destination column. When it is used in other contexts, it may be necessary to specify the correct data type. If the entries are all quoted literal constants, coercing the first is sufficient to determine the assumed type for all:

```
SELECT * FROM machines WHERE ip_address IN 
(VALUES('192.168.0.1'::inet), ('192.168.0.10'), 
('192.0.2.43'));
```

> **Note** For simple `IN` tests, it is better to rely on the list-of-scalars form of `IN` than to write a `VALUES` query as shown above. The list of scalars method requires less writing and is often more efficient.

## <a id="section7"></a>Compatibility 

`VALUES` conforms to the SQL standard. `LIMIT` and `OFFSET` are Greenplum Database extensions; see also under [SELECT](SELECT.html).

## <a id="section8"></a>See Also 

[INSERT](INSERT.html), [SELECT](SELECT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

