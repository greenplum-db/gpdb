---
title: CREATE FOREIGN TABLE 
---

Defines a new foreign table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table_name> ( [
    <column_name> <data_type> [ OPTIONS ( <option> '<value>' [, ... ] ) ] [ COLLATE <collation> ] [ <column_constraint> [ ... ] ]
      [, ... ]
] )
    SERVER <server_name>
  [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ] <option> '<value>' [, ... ] ) ]
```

where column\_constraint is:

```

[ CONSTRAINT <constraint_name> ]
{ NOT NULL |
  NULL |
  DEFAULT <default_expr> }
```

## <a id="section3"></a>Description 

`CREATE FOREIGN TABLE` creates a new foreign table in the current database. The user who creates the foreign table becomes its owner.

If you schema-qualify the table name \(for example, `CREATE FOREIGN TABLE myschema.mytable ...`\), Greenplum Database creates the table in the specified schema. Otherwise, the foreign table is created in the current schema. The name of the foreign table must be distinct from the name of any other foreign table, table, sequence, index, or view in the same schema.

Because `CREATE FOREIGN TABLE` automatically creates a data type that represents the composite type corresponding to one row of the foreign table, foreign tables cannot have the same name as any existing data type in the same schema.

To create a foreign table, you must have `USAGE` privilege on the foreign server, as well as `USAGE` privilege on all column types used in the table.

## <a id="section4"></a>Parameters 

IF NOT EXISTS
:   Do not throw an error if a relation with the same name already exists. Greenplum Database issues a notice in this case. Note that there is no guarantee that the existing relation is anything like the one that would have been created.

table\_name
:   The name \(optionally schema-qualified\) of the foreign table to create.

column\_name
:   The name of a column to create in the new foreign table.

data\_type
:   The data type of the column, including array specifiers.

NOT NULL
:   The column is not allowed to contain null values.

NULL
:   The column is allowed to contain null values. This is the default.

    This clause is provided only for compatibility with non-standard SQL databases. Its use is discouraged in new applications.

DEFAULT default\_expr
:   The `DEFAULT` clause assigns a default value for the column whose definition it appears within. The value is any variable-free expression; Greenplum Database does not allow subqueries and cross-references to other columns in the current table. The data type of the default expression must match the data type of the column.

    Greenplum Database uses the default expression in any insert operation that does not specify a value for the column. If there is no default for a column, then the default is null.

server\_name
:   The name of an existing server to use for the foreign table. For details on defining a server, see [CREATE SERVER](CREATE_SERVER.html).

OPTIONS \( option 'value' \[, ... \] \)
:   The options for the new foreign table or one of its columns. While option names must be unique, a table option and a column option may have the same name. The option names and values are foreign-data wrapper-specific. Greenplum Database validates the options and values using the foreign-data wrapper's validator\_function.

mpp\_execute \{ 'master' \| 'any' \| 'all segments' \}
:   A Greenplum Database-specific option that identifies the host from which the foreign-data wrapper reads or writes data:

    -   `master` \(the default\)—Read or write data from the master host.
    -   `any`—Read data from either the master host or any one segment, depending on which path costs less.
    -   `all segments`—Read or write data from all segments. To support this option value, the foreign-data wrapper must have a policy that matches the segments to data.
    <br/><br/>**Note:** Greenplum Database supports parallel writes to foreign tables only when you set `mpp_execute 'all segments'`.

    Support for the foreign table `mpp_execute` option, and the specific modes, is foreign-data wrapper-specific.

    The `mpp_execute` option can be specified in multiple commands: `CREATE FOREIGN TABLE`, `CREATE SERVER`, and `CREATE FOREIGN DATA WRAPPER`. The foreign table setting takes precedence over the foreign server setting, followed by the foreign-data wrapper setting.

## <a id="section5"></a>Notes 

The Tanzu Greenplum Query Optimizer, GPORCA, does not support foreign tables. A query on a foreign table always falls back to the Postgres Planner.

## <a id="section6"></a>Examples 

Create a foreign table named `films` with the server named `film_server`:

```
CREATE FOREIGN TABLE films (
    code        char(5) NOT NULL,
    title       varchar(40) NOT NULL,
    did         integer NOT NULL,
    date_prod   date,
    kind        varchar(10),
    len         interval hour to minute
)
SERVER film_server;
```

## <a id="section7"></a>Compatibility 

`CREATE FOREIGN TABLE` largely conforms to the SQL standard; however, much as with [CREATE TABLE](CREATE_TABLE.html), Greenplum Database permits `NULL` constraints and zero-column foreign tables. The ability to specify a default value is a Greenplum Database extension, as is the `mpp_execute` option.

## <a id="section8"></a>See Also 

[ALTER FOREIGN TABLE](ALTER_FOREIGN_TABLE.html), [DROP FOREIGN TABLE](DROP_FOREIGN_TABLE.html), [CREATE SERVER](CREATE_SERVER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

