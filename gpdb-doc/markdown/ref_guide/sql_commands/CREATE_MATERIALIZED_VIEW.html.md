# CREATE MATERIALIZED VIEW 

Defines a new materialized view.

## <a id="section1"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE MATERIALIZED VIEW [ IF  NOT EXISTS ] <table_name>
    [ (<column_name> [, ...] ) ]
    [ USING <method> ]
    [ WITH ( <storage_parameter> [= <value>] [, ... ] ) ]
    [ TABLESPACE <tablespace_name> ]
    AS <query>
    [ WITH [ NO ] DATA ]
    [DISTRIBUTED {| BY <column> [<opclass>], [ ... ] | RANDOMLY | REPLICATED }]
```

## <a id="section3"></a>Description 

`CREATE MATERIALIZED VIEW` defines a materialized view of a query. The query is run and used to populate the view at the time the command is issued \(unless `WITH NO DATA` is used\) and can be refreshed later using [REFRESH MATERIALIZED VIEW](REFRESH_MATERIALIZED_VIEW.html).

`CREATE MATERIALIZED VIEW` is similar to `CREATE TABLE AS`, except that it also remembers the query used to initialize the view, so that it can be refreshed later upon demand. To refresh materialized view data, use the `REFRESH MATERIALIZED VIEW` command. A materialized view has many of the same properties as a table, but there is no support for temporary materialized views.

## <a id="section4"></a>Parameters 

IF NOT EXISTS
:   Do not throw an error if a materialized view with the same name already exists. A notice is issued in this case. Note that there is no guarantee that the existing materialized view is anything like the one that would have been created.

table\_name
:   The name \(optionally schema-qualified\) of the materialized view to be created.

column\_name
:   The name of a column in the new materialized view. The column names are assigned based on position. The first column name is assigned to the first column of the query result, and so on. If a column name is not provided, it is taken from the output column names of the query.

USING method
:   This optional clause specifies the table access method to use to store the contents for the new materialized view; the method needs be an access method of type `TABLE`. If this option is not specified, the default table access method is chosen for the new materialized view. See [default_table_access_method](../config_params/guc-list.html) for more information.

WITH \( storage\_parameter \[= value\] \[, ... \] \)
:   This clause specifies optional storage parameters for the materialized view. All parameters supported for `CREATE TABLE` are also supported for `CREATE MATERIALIZED VIEW`. See [CREATE TABLE](CREATE_TABLE.html) for more information.

TABLESPACE tablespace\_name
:   The tablespace\_name is the name of the tablespace in which the new materialized view is to be created. If not specified, server configuration parameter [default\_tablespace](../config_params/guc-list.html) is consulted.

query
:   A [SELECT](SELECT.html), [TABLE](SELECT.html#table-command), or [VALUES](VALUES.html) command. This query will run within a security-restricted operation; in particular, calls to functions that themselves create temporary tables will fail.

WITH \[ NO \] DATA
:   This clause specifies whether or not the materialized view should be populated with data at creation time. `WITH DATA` is the default, populate the materialized view. For `WITH NO DATA`, the materialized view is not populated with data, is flagged as unscannable, and cannot be queried until `REFRESH MATERIALIZED VIEW` is used to populate the materialized view.

DISTRIBUTED BY \(column \[opclass\], \[ ... \] \)
DISTRIBUTED RANDOMLY
DISTRIBUTED REPLICATED
:   Used to declare the Greenplum Database distribution policy for the materialized view data. For information about a table distribution policy, see [CREATE TABLE](CREATE_TABLE.html).

## <a id="section5"></a>Notes 

Materialized views are read only. The system will not allow an `INSERT`, `UPDATE`, or `DELETE` on a materialized view. Use `REFRESH MATERIALIZED VIEW` to update the materialized view data.

If you want the data to be ordered upon generation, you must use an `ORDER BY` clause in the materialized view query. However, if a materialized view query contains an `ORDER BY` or `SORT` clause, the data is not guaranteed to be ordered or sorted if `SELECT` is performed on the materialized view.

## <a id="section6"></a>Examples 

Create a view consisting of all comedy films:

```
CREATE MATERIALIZED VIEW comedies AS SELECT * FROM films 
WHERE kind = 'comedy';
```

This will create a view containing the columns that are in the `film` table at the time of view creation. Though `*` was used to create the materialized view, columns added later to the table will not be part of the view.

Create a view that gets the top ten ranked baby names:

```
CREATE MATERIALIZED VIEW topten AS SELECT name, rank, gender, year FROM 
names, rank WHERE rank < '11' AND names.id=rank.id;
```

## <a id="section7"></a>Compatibility 

`CREATE MATERIALIZED VIEW` is a Greenplum Database extension of the SQL standard.

## <a id="section8"></a>See Also 

[SELECT](SELECT.html), [VALUES](VALUES.html), [CREATE VIEW](CREATE_VIEW.html), [ALTER MATERIALIZED VIEW](ALTER_MATERIALIZED_VIEW.html), [DROP MATERIALIZED VIEW](DROP_MATERIALIZED_VIEW.html), [REFRESH MATERIALIZED VIEW](REFRESH_MATERIALIZED_VIEW.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

