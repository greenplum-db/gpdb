# CREATE FOREIGN TABLE 

Defines a new foreign table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table_name> ( [
  { <column_name> <data_type> [ OPTIONS ( <option> '<value>' [, ... ] ) ] [ COLLATE <collation> ] [ <column_constraint> [ ... ] ]
    | <table_constraint> }
    [, ... ]
] )
[ INHERITS ( <parent_table> [, ... ] ) ]
  SERVER <server_name>
  [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ] <option> '<value>' [, ... ] ) ]

CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table_name>
  PARTITION OF <parent_table> [ (
  { <column_name> [ WITH OPTIONS ] [ <column_constraint> [ ... ] ]
    | <table_constraint> }
    [, ... ]
) ]
{ FOR VALUES <partition_bound_spec> | DEFAULT }
  SERVER <server_name>
  [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ] <option> '<value>' [, ... ] ) ]

where <column_constraint> is:

[ CONSTRAINT <constraint_name> ]
{ NOT NULL |
  NULL |
  DEFAULT <default_expr> |
  GENERATED ALWAYS AS ( <generation_expr> ) STORED}

and <table_constraint> is:

[ CONSTRAINT <constraint_name> ]
CHECK ( <expression> ) [ NO INHERIT ]

and <partition_bound_spec> is:

IN ( <partition_bound_expr> [, ...] ) |
FROM ( { <partition_bound_expr> | MINVALUE | MAXVALUE } [, ...] )
  TO ( { <partition_bound_expr> | MINVALUE | MAXVALUE } [, ...] ) |
WITH ( MODULUS <numeric_literal>, REMAINDER <numeric_literal> )
```

## <a id="section3"></a>Description 

`CREATE FOREIGN TABLE` creates a new foreign table in the current database. The user who creates the foreign table becomes its owner.

If you schema-qualify the table name \(for example, `CREATE FOREIGN TABLE myschema.mytable ...`\), Greenplum Database creates the table in the specified schema. Otherwise, the foreign table is created in the current schema. The name of the foreign table must be distinct from the name of any other foreign table, table, sequence, index, view, or materialized view in the same schema.

Because `CREATE FOREIGN TABLE` automatically creates a data type that represents the composite type corresponding to one row of the foreign table, foreign tables cannot have the same name as any existing data type in the same schema.

If the `PARTITION OF` clause is specified, then the table is created as a partition of parent\_table with specified bounds.

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

COLLATE collation
:   The `COLLATE` clause assigns a collation to the column \(which must be of a collatable data type\). If not specified, the column data type's default collation is used.

INHERITS \( parent\_table [, ... ] \)
:   The optional `INHERITS` clause specifies a list of tables from which the new foreign table automatically inherits all columns. Parent tables can be plain tables or foreign tables. See the similar form of [CREATE TABLE](CREATE_TABLE.html) for more details.

PARTITION OF parent\_table { FOR VALUES partition\_bound\_spec | DEFAULT }
:   This form can be used to create the foreign table as partition of the given parent table with specified partition bound values. See the similar form of [CREATE TABLE](CREATE_TABLE.html) for more details. Note that it is currently not allowed to create the foreign table as a partition of the parent table if there are `UNIQUE` indexes on the parent table. \(See also [ALTER TABLE ATTACH PARTITION](ALTER_TABLE.html).\)

CONSTRAINT constraint\_name
:   An optional name for a column or table constraint. If the constraint is violated, the constraint name is present in error messages, so constraint names like `col must be positive` can be used to communicate helpful constraint information to client applications. \(Double-quotes are needed to specify constraint names that contain spaces.\) If a constraint name is not specified, the system generates a name.

NOT NULL
:   The column is not allowed to contain null values.

NULL
:   The column is allowed to contain null values. This is the default.

    This clause is provided only for compatibility with non-standard SQL databases. Its use is discouraged in new applications.

CHECK ( expression ) [ NO INHERIT ]
:   The `CHECK` clause specifies an expression producing a Boolean result which each row in the foreign table is expected to satisfy; that is, the expression should produce TRUE or UNKNOWN, never FALSE, for all rows in the foreign table. A check constraint specified as a column constraint should reference that column's value only, while an expression appearing in a table constraint can reference multiple columns.
:   Currently, `CHECK` expressions cannot contain subqueries nor refer to variables other than columns of the current row. The system column `tableoid` may be referenced, but not any other system column.
:   A constraint marked with `NO INHERIT` will not propagate to child tables.

DEFAULT default\_expr
:   The `DEFAULT` clause assigns a default value for the column whose definition it appears within. The value is any variable-free expression; Greenplum Database does not allow subqueries and cross-references to other columns in the current table. The data type of the default expression must match the data type of the column.
:   Greenplum Database uses the default expression in any insert operation that does not specify a value for the column. If there is no default for a column, then the default is null.

GENERATED ALWAYS AS \( generation\_expr \) STORED
:   This clause creates the column as a generated column. The column cannot be written to, and when read the result of the specified expression will be returned.
:   The keyword `STORED` is required to signify that the column will be computed on write. \(The computed value will be presented to the foreign-data wrapper for storage and must be returned on reading.\)
:   The generation expression can refer to other columns in the table, but not other generated columns. Any functions and operators used must be immutable. References to other tables are not allowed.

server\_name
:   The name of an existing server to use for the foreign table. For details on defining a server, see [CREATE SERVER](CREATE_SERVER.html).

OPTIONS \( option 'value' \[, ... \] \)
:   The options for the new foreign table or one of its columns. While option names must be unique, a table option and a column option may have the same name. The option names and values are foreign-data wrapper-specific. Greenplum Database validates the options and values using the foreign-data wrapper's validator\_function.

mpp\_execute \{ 'master' \| 'any' \| 'all segments' \}
:   A Greenplum Database-specific option that identifies the host from which the foreign-data wrapper reads or writes data:

    -   `master` \(the default\)—Read or write data from the coordinator host.
    -   `any`—Read data from either the coordinator host or any one segment, depending on which path costs less.
    -   `all segments`—Read or write data from all segments. To support this option value, the foreign-data wrapper must have a policy that matches the segments to data.

    > **Note** Greenplum Database supports parallel writes to foreign tables only when you set `mpp_execute 'all segments'`.

    Support for the foreign table `mpp_execute` option, and the specific modes, is foreign-data wrapper-specific.

    The `mpp_execute` option can be specified in multiple commands: `CREATE FOREIGN TABLE`, `CREATE SERVER`, and `CREATE FOREIGN DATA WRAPPER`. The foreign table setting takes precedence over the foreign server setting, followed by the foreign-data wrapper setting.

## <a id="section5"></a>Notes 

Constraints on foreign tables \(such as `CHECK` or `NOT NULL` clauses\) are not enforced by Greenplum Database, and most foreign-data wrappers do not attempt to enforce them either; that is, the constraint is simply assumed to hold true. There would be little point in such enforcement since it would only apply to rows inserted or updated via the foreign table, and not to rows modified by other means, such as directly on the remote server. Instead, a constraint attached to a foreign table should represent a constraint that is being enforced by the remote server.

Some special-purpose foreign-data wrappers might be the only access mechanism for the data they access, and in that case it might be appropriate for the foreign-data wrapper itself to perform constraint enforcement. But you should not assume that a wrapper does that unless its documentation says so.

Although Greenplum Database does not attempt to enforce constraints on foreign tables, it does assume that they are correct for purposes of query optimization. If there are rows visible in the foreign table that do not satisfy a declared constraint, queries on the table might produce errors or incorrect answers. It is the user's responsibility to ensure that the constraint definition matches reality.

<div class="note">When a foreign table is used as a partition of a partitioned table, there is an implicit constraint that its contents must satisfy the partitioning rule. Again, it is the user's responsibility to ensure that that is true, which is best done by installing a matching constraint on the remote server.</div>

Within a partitioned table containing foreign-table partitions, an `UPDATE` that changes the partition key value can cause a row to be moved from a local partition to a foreign-table partition, provided the foreign-data wrapper supports tuple routing. However it is not currently possible to move a row from a foreign-table partition to another partition. An `UPDATE` that would require doing that will fail due to the partitioning constraint, assuming that that is properly enforced by the remote server.

Similar considerations apply to generated columns. Stored generated columns are computed on insert or update on the local Greenplum Database server and handed to the foreign-data wrapper for writing out to the foreign data store, but it is not enforced that a query of the foreign table returns values for stored generated columns that are consistent with the generation expression. Again, this might result in incorrect query results.

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

Create foreign table `measurement_y2016m07`, which will be accessed through the server `server_07`, as a partition of the range partitioned table measurement:

```
CREATE FOREIGN TABLE measurement_y2016m07
    PARTITION OF measurement FOR VALUES FROM ('2016-07-01') TO ('2016-08-01')
    SERVER server_07;
```

## <a id="section7"></a>Compatibility 

`CREATE FOREIGN TABLE` largely conforms to the SQL standard; however, much as with [CREATE TABLE](CREATE_TABLE.html), Greenplum Database permits `NULL` constraints and zero-column foreign tables. The ability to specify column default values is a Greenplum Database extension, as is the `mpp_execute` option. Table inheritance, in the form defined by Greenplum Database, is nonstandard.

## <a id="section8"></a>See Also 

[ALTER FOREIGN TABLE](ALTER_FOREIGN_TABLE.html), [DROP FOREIGN TABLE](DROP_FOREIGN_TABLE.html), [CREATE SERVER](CREATE_SERVER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

