# CREATE VIEW 

Defines a new view.

## Synopsis 

``` {#sql_command_synopsis}
CREATE [OR REPLACE] [TEMP | TEMPORARY] VIEW <name>
       [ ( <column_name> [, ...] ) ]
       AS <query>
```

## Description 

`CREATE VIEW` defines a view of a query. The view is not physically materialized. Instead, the query is run every time the view is referenced in a query.

`CREATE OR REPLACE VIEW` is similar, but if a view of the same name already exists, it is replaced. You can only replace a view with a new query that generates the identical set of columns \(same column names and data types\).

If a schema name is given then the view is created in the specified schema. Otherwise it is created in the current schema. Temporary views exist in a special schema, so a schema name may not be given when creating a temporary view. The name of the view must be distinct from the name of any other view, table, sequence, or index in the same schema.

## Parameters 

TEMPORARY \| TEMP
:   If specified, the view is created as a temporary view. Temporary views are automatically dropped at the end of the current session. Existing permanent relations with the same name are not visible to the current session while the temporary view exists, unless they are referenced with schema-qualified names. If any of the tables referenced by the view are temporary, the view is created as a temporary view \(whether `TEMPORARY` is specified or not\).

name
:   The name \(optionally schema-qualified\) of a view to be created.

column\_name
:   An optional list of names to be used for columns of the view. If not given, the column names are deduced from the query.

query
:   A [SELECT](SELECT.html) or [VALUES](VALUES.html) command which will provide the columns and rows of the view.

## Notes 

Views in Greenplum Database are read only. The system will not allow an insert, update, or delete on a view. You can get the effect of an updatable view by creating rewrite rules on the view into appropriate actions on other tables. For more information see `CREATE RULE`.

Be careful that the names and data types of the view's columns will be assigned the way you want. For example:

```
CREATE VIEW vista AS SELECT 'Hello World';
```

is bad form in two ways: the column name defaults to `?column?`, and the column data type defaults to `unknown`. If you want a string literal in a view's result, use something like:

```
CREATE VIEW vista AS SELECT text 'Hello World' AS hello;
```

Access to tables referenced in the view is determined by permissions of the view owner not the current user \(even if the current user is a superuser\). This can be confusing in the case of superusers, since superusers typically have access to all objects. In the case of a view, even superusers must be explicitly granted access to tables referenced in the view if they are not the owner of the view.

However, functions called in the view are treated the same as if they had been called directly from the query using the view. Therefore the user of a view must have permissions to call any functions used by the view.

If you create a view with an `ORDER BY` clause, the `ORDER BY` clause is ignored when you do a `SELECT` from the view.

## Examples 

Create a view consisting of all comedy films:

```
CREATE VIEW comedies AS SELECT * FROM films WHERE kind = 
'comedy';
```

Create a view that gets the top ten ranked baby names:

```
CREATE VIEW topten AS SELECT name, rank, gender, year FROM 
names, rank WHERE rank < '11' AND names.id=rank.id;
```

## Compatibility 

The SQL standard specifies some additional capabilities for the `CREATE VIEW` statement that are not in Greenplum Database. The optional clauses for the full SQL command in the standard are:

-   **CHECK OPTION** — This option has to do with updatable views. All `INSERT` and `UPDATE` commands on the view will be checked to ensure data satisfy the view-defining condition \(that is, the new data would be visible through the view\). If they do not, the update will be rejected.
-   **LOCAL** — Check for integrity on this view.
-   **CASCADED** — Check for integrity on this view and on any dependent view. `CASCADED` is assumed if neither `CASCADED` nor `LOCAL` is specified.

`CREATE OR REPLACE VIEW` is a Greenplum Database language extension. So is the concept of a temporary view.

## See Also 

[SELECT](SELECT.html), [DROP VIEW](DROP_VIEW.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

