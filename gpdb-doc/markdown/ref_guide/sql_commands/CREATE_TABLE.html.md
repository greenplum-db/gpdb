# CREATE TABLE 

Defines a new table.

> **Note** Referential integrity syntax \(foreign key constraints\) is accepted but not enforced.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}

CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP } | UNLOGGED] TABLE [IF NOT EXISTS] 
  <table_name> ( 
  [ { <column_name> <data_type> [ COLLATE <collation> ] [<column_constraint> [ ... ] ]
[ ENCODING ( <storage_directive> [, ...] ) ]
    | <table_constraint>
    | LIKE <source_table> [ <like_option> ... ] }
    | [ <column_reference_storage_directive> [, ...]
    [, ... ]
] )
[ INHERITS ( <parent_table> [, ... ] ) ]

[ USING ( <access method> ) ]
[ WITH ( <storage_parameter> [=<value>] [, ... ] ) ]
[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
[ TABLESPACE <tablespace_name> ]
[ DISTRIBUTED BY (<column> [<opclass>], [ ... ] ) 
       | DISTRIBUTED RANDOMLY | DISTRIBUTED REPLICATED ]

{ --partitioned table using SUBPARTITION TEMPLATE
[ PARTITION BY <partition_type> (<column>) 
  {  [ SUBPARTITION BY <partition_type> (<column1>) 
       SUBPARTITION TEMPLATE ( <template_spec> ) ]
          [ SUBPARTITION BY partition_type (<column2>) 
            SUBPARTITION TEMPLATE ( <template_spec> ) ]
              [...]  }
  ( <partition_spec> ) ]
} |

{ -- partitioned table without SUBPARTITION TEMPLATE
[ PARTITION BY <partition_type> (<column>)
   [ SUBPARTITION BY <partition_type> (<column1>) ]
      [ SUBPARTITION BY <partition_type> (<column2>) ]
         [...]
  ( <partition_spec>
     [ ( <subpartition_spec_column1>
          [ ( <subpartition_spec_column2>
               [...] ) ] ) ],
  [ <partition_spec>
     [ ( <subpartition_spec_column1>
        [ ( <subpartition_spec_column2>
             [...] ) ] ) ], ]
    [...]
  ) ]
}

CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP} | UNLOGGED ] TABLE [IF NOT EXISTS] 
   <table_name>
    OF <type_name> [ (
  { <column_name> WITH OPTIONS [ <column_constraint> [ ... ] ]
    | <table_constraint> } 
    [, ... ]
) ]

[ USING <access_method> ]
[ WITH ( <storage_parameter> [=<value>] [, ... ] ) ]
[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
[ TABLESPACE <tablespace_name> ]

```

where column\_constraint is:

```
[ CONSTRAINT <constraint_name>]
{ NOT NULL 
  | NULL 
  | CHECK  ( <expression> ) [ NO INHERIT ]
  | DEFAULT <default_expr>
  | UNIQUE <index_parameters>
  | PRIMARY KEY <index_parameters>
  | REFERENCES <reftable> [ ( refcolumn ) ] 
      [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]  
      [ ON DELETE <key_action> ] [ ON UPDATE <key_action> ] }
[ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]
```

and table\_constraint is:

```
[ CONSTRAINT <constraint_name> ]
{ CHECK ( <expression> ) [ NO INHERIT ]
  | UNIQUE ( <column_name> [, ... ] ) <index_parameters>
  | PRIMARY KEY ( <column_name> [, ... ] ) <index_parameters>
  | FOREIGN KEY ( <column_name> [, ... ] ) 
      REFERENCES <reftable> [ ( <refcolumn> [, ... ] ) ]
      [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ] 
      [ ON DELETE <key_action> ] [ ON UPDATE <key_action> ] }
[ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]
```

and *like\_option* is:

``` pre
{INCLUDING|EXCLUDING} {DEFAULTS|CONSTRAINTS|INDEXES|STORAGE|COMMENTS|ALL}
```

and index_parameters in `UNIQUE` and `PRIMARY KEY` constraints are:

```
[ WITH ( <storage_parameter> [=<value>] [, ... ] ) ]
[ USING INDEX TABLESPACE <tablespace_name> ] 
```

and storage_directive for a column is:

```
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
    [compresslevel={0-9}]
    [blocksize={8192-2097152} ]
```

and `storage_parameter` for a table or partition is:

```
   appendoptimized={TRUE|FALSE}
   blocksize={8192-2097152}
   orientation={COLUMN|ROW}
   checksum={TRUE|FALSE}
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
   compresslevel={0-9}
   fillfactor={10-100}
   analyze_hll_non_part_table={TRUE|FALSE}
   reorganize={TRUE|FALSE}
   vacuum_index_cleanup={TRUE|FALSE}
```


and key\_action is:

```
    ON DELETE 
  | ON UPDATE
  | NO ACTION
  | RESTRICT
  | CASCADE
  | SET NULL
  | SET DEFAULT
```

and partition\_type is:

```
    LIST | RANGE
```

and partition\_specification is:

```
<partition_element> [, ...]
```

and partition\_element is:

```
   DEFAULT PARTITION <name>
  | [PARTITION <name>] VALUES (<list_value> [,...] )
  | [PARTITION <name>] 
     START ([<datatype>] '<start_value>') [INCLUSIVE | EXCLUSIVE]
     [ END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE] ]
     [ EVERY ([<datatype>] [<number> | INTERVAL] '<interval_value>') ]
  | [PARTITION <name>] 
     END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
     [ EVERY ([<datatype>] [<number> | INTERVAL] '<interval_value>') ]
[ WITH ( <storage_parameter> = <value> [ , ... ] ) ]
[ <column_reference_storage_directive> [ , ... ] ]
[ TABLESPACE <tablespace> ]
```

where subpartition\_spec or template\_spec is:

```
<subpartition_element> [, ...]
```

and subpartition\_element is:

```
   DEFAULT SUBPARTITION <name>
  | [SUBPARTITION <name>] VALUES (<list_value> [,...] )
  | [SUBPARTITION <name>] 
     START ([<datatype>] '<start_value>') [INCLUSIVE | EXCLUSIVE]
     [ END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE] ]
     [ EVERY ([<datatype>] [<number> | INTERVAL] '<interval_value>') ]
  | [SUBPARTITION <name>] 
     END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
     [ EVERY ([<datatype>] [<number> | INTERVAL] '<interval_value>') ]
[ WITH ( <storage_parameter> = <value> [ , ... ] ) ]
[ <column_reference_storage_directive> [, ...] ]
[ TABLESPACE <tablespace> ]
```

## <a id="section3"></a>Description 

`CREATE TABLE` creates an initially empty table in the current database. The user who issues the command owns the table.

To be able to create a table, you must have `USAGE` privilege on all column types or the type in the `OF` clause, respectively.

If you specify a schema name, Greenplum creates the table in the specified schema. Otherwise Greenplum creates the table in the current schema. Temporary tables exist in a special schema, so you cannot specify a schema name when creating a temporary table. Table names must be distinct from the name of any other table, external table, sequence, index, view, or foreign table in the same schema.

`CREATE TABLE` also automatically creates a data type that represents the composite type corresponding to one row of the table. Therefore, tables cannot have the same name as any existing data type in the same schema.

The optional constraint clauses specify conditions that new or updated rows must satisfy for an insert or update operation to succeed. A constraint is an SQL object that helps define the set of valid values in the table in various ways. Constraints apply to tables, not to partitions. You cannot add a constraint to a partition or subpartition.

Referential integrity constraints \(foreign keys\) are accepted but not enforced. The information is kept in the system catalogs but is otherwise ignored.

There are two ways to define constraints: table constraints and column constraints. A column constraint is defined as part of a column definition. A table constraint definition is not tied to a particular column, and it can encompass more than one column. Every column constraint can also be written as a table constraint; a column constraint is only a notational convenience for use when the constraint only affects one column.

When creating a table, there is an additional clause to declare the Greenplum Database distribution policy. If a `DISTRIBUTED BY`, `DISTRIBUTED RANDOMLY`, or `DISTRIBUTED REPLICATED` clause is not supplied, then Greenplum Database assigns a hash distribution policy to the table using either the `PRIMARY KEY` \(if the table has one\) or the first column of the table as the distribution key. Columns of geometric or user-defined data types are not eligible as Greenplum distribution key columns. If a table does not have a column of an eligible data type, the rows are distributed based on a round-robin or random distribution. To ensure an even distribution of data in your Greenplum Database system, you want to choose a distribution key that is unique for each record, or if that is not possible, then choose `DISTRIBUTED RANDOMLY`.

If the `DISTRIBUTED REPLICATED` clause is supplied, Greenplum Database distributes all rows of the table to all segments in the Greenplum Database system. This option can be used in cases where user-defined functions must run on the segments, and the functions require access to all rows of the table. Replicated functions can also be used to improve query performance by preventing broadcast motions for the table. The `DISTRIBUTED REPLICATED` clause cannot be used with the `PARTITION BY` clause or the `INHERITS` clause. A replicated table also cannot be inherited by another table. The hidden system columns \(`ctid`, `cmin`, `cmax`, `xmin`, `xmax`, and `gp_segment_id`\) cannot be referenced in user queries on replicated tables because they have no single, unambiguous value. Greenplum Database returns a `column does not exist` error for the query.

The `PARTITION BY` clause allows you to divide the table into multiple sub-tables \(or parts\) that, taken together, make up the parent table and share its schema. Though the sub-tables exist as independent tables, the Greenplum Database restricts their use in important ways. Internally, partitioning is implemented as a special form of inheritance. Each child table partition is created with a distinct `CHECK` constraint which limits the data the table can contain, based on some defining criteria. The `CHECK` constraints are also used by the query optimizer to determine which table partitions to scan in order to satisfy a given query predicate. These partition constraints are managed automatically by the Greenplum Database.

## <a id="section4"></a>Parameters 

GLOBAL \| LOCAL
:   These keywords are present for SQL standard compatibility, but have no effect in Greenplum Database and are deprecated.

TEMPORARY \| TEMP
:   If specified, the table is created as a temporary table. Temporary tables are automatically dropped at the end of a session, or optionally at the end of the current transaction \(see `ON COMMIT`\). Existing permanent tables with the same name are not visible to the current session while the temporary table exists, unless they are referenced with schema-qualified names. Any indexes created on a temporary table are automatically temporary as well.

UNLOGGED
:   If specified, the table is created as an unlogged table. Data written to unlogged tables is not written to the write-ahead \(WAL\) log, which makes them considerably faster than ordinary tables. However, the contents of an unlogged table are not replicated to mirror segment instances. Also an unlogged table is not crash-safe. After a segment instance crash or unclean shutdown, the data for the unlogged table on that segment is truncated. Any indexes created on an unlogged table are automatically unlogged as well.

table\_name
:   The name \(optionally schema-qualified\) of the table to be created.

OF type\_name
:   Creates a typed table, which takes its structure from the specified composite type \(name optionally schema-qualified\). A typed table is tied to its type; for example the table will be dropped if the type is dropped \(with `DROP TYPE ... CASCADE`\).

:   When a typed table is created, the data types of the columns are determined by the underlying composite type and are not specified by the `CREATE TABLE` command. But the `CREATE TABLE` command can add defaults and constraints to the table and can specify storage parameters.

column\_name
:   The name of a column to be created in the new table.

data\_type
:   The data type of the column. This may include array specifiers.

:   For table columns that contain textual data, Specify the data type `VARCHAR` or `TEXT`. Specifying the data type `CHAR` is not recommended. In Greenplum Database, the data types `VARCHAR` or `TEXT` handles padding added to the data \(space characters added after the last non-space character\) as significant characters, the data type `CHAR` does not. See [Notes](#section5).

COLLATE collation
:   The `COLLATE` clause assigns a collation to the column \(which must be of a collatable data type\). If not specified, the column data type's default collation is used.

    > **Note** GPORCA supports collation only when all columns in the query use the same collation. If columns in the query use different collations, then Greenplum uses the Postgres Planner.

DEFAULT default\_expr
:   The `DEFAULT` clause assigns a default data value for the column whose column definition it appears within. The value is any variable-free expression \(subqueries and cross-references to other columns in the current table are not allowed\). The data type of the default expression must match the data type of the column. The default expression will be used in any insert operation that does not specify a value for the column. If there is no default for a column, then the default is null.

ENCODING \( storage_directive \[, ...\] \)
:   For a column, the optional `ENCODING` clause specifies the type of compression and block size for the column data. See [storage\_options](#with_storage) for `compresstype`, `compresslevel`, and `blocksize` values.

:   The clause is valid only for append-optimized, column-oriented tables.

:   Column compression settings are inherited from the table level to the partition level to the subpartition level. The lowest-level settings have priority.

:   The `column_reference_storage_directive` parameter specifies a column along with its storage directive.

For more information on storage directives, see [Adding Column Level Compression](../../admin_guide/ddl/ddl-storage.html#topic43).

INHERITS \( parent\_table \[, …\]\)
:   The optional `INHERITS` clause specifies a list of tables from which the new table automatically inherits all columns. Use of `INHERITS` creates a persistent relationship between the new child table and its parent table\(s\). Schema modifications to the parent\(s\) normally propagate to children as well, and by default the data of the child table is included in scans of the parent\(s\).

:   In Greenplum Database, the `INHERITS` clause is not used when creating partitioned tables. Although the concept of inheritance is used in partition hierarchies, the inheritance structure of a partitioned table is created using the [PARTITION BY](#part_by) clause.

:   If the same column name exists in more than one parent table, an error is reported unless the data types of the columns match in each of the parent tables. If there is no conflict, then the duplicate columns are merged to form a single column in the new table. If the column name list of the new table contains a column name that is also inherited, the data type must likewise match the inherited column\(s\), and the column definitions are merged into one. If the new table explicitly specifies a default value for the column, this default overrides any defaults from inherited declarations of the column. Otherwise, any parents that specify default values for the column must all specify the same default, or an error will be reported.

:   `CHECK` constraints are merged in essentially the same way as columns: if multiple parent tables or the new table definition contain identically-named `constraints`, these constraints must all have the same check expression, or an error will be reported. Constraints having the same name and expression will be merged into one copy. A constraint marked `NO INHERIT` in a parent will not be considered. Notice that an unnamed `CHECK` constraint in the new table will never be merged, since a unique name will always be chosen for it.

:   Column `STORAGE` settings are also copied from parent tables.

LIKE source\_table like\_option `...`\]
:   The `LIKE` clause specifies a table from which the new table automatically copies all column names, their data types, not-null constraints, and distribution policy. Unlike `INHERITS`, the new table and original table are completely decoupled after creation is complete.

:   > **Note** Storage properties like append-optimized or partition structure are not copied.

:   Default expressions for the copied column definitions will only be copied if `INCLUDING DEFAULTS` is specified. The default behavior is to exclude default expressions, resulting in the copied columns in the new table having null defaults.

:   Not-null constraints are always copied to the new table. `CHECK` constraints will be copied only if `INCLUDING CONSTRAINTS` is specified. No distinction is made between column constraints and table constraints.

:   Indexes, `PRIMARY KEY`, and `UNIQUE` constraints on the original table will be created on the new table only if the `INCLUDING INDEXES` clause is specified. Names for the new indexes and constraints are chosen according to the default rules, regardless of how the originals were named. \(This behavior avoids possible duplicate-name failures for the new indexes.\)

:   Any indexes on the original table will not be created on the new table, unless the `INCLUDING INDEXES` clause is specified.

:   `STORAGE` settings for the copied column definitions will be copied only if `INCLUDING STORAGE` is specified. The default behavior is to exclude `STORAGE` settings, resulting in the copied columns in the new table having type-specific default settings.

:   Comments for the copied columns, constraints, and indexes will be copied only if `INCLUDING COMMENTS` is specified. The default behavior is to exclude comments, resulting in the copied columns and constraints in the new table having no comments.

:   `INCLUDING ALL` is an abbreviated form of `INCLUDING DEFAULTS INCLUDING CONSTRAINTS INCLUDING INDEXES INCLUDING STORAGE INCLUDING COMMENTS`.

:   Note that unlike `INHERITS`, columns and constraints copied by `LIKE` are not merged with similarly named columns and constraints. If the same name is specified explicitly or in another `LIKE` clause, an error is signaled.

:   The `LIKE` clause can also be used to copy columns from views, foreign tables, or composite types. Inapplicable options \(e.g., `INCLUDING INDEXES` from a view\) are ignored.

CONSTRAINT constraint\_name
:   An optional name for a column or table constraint. If the constraint is violated, the constraint name is present in error messages, so constraint names like column must be positive can be used to communicate helpful constraint information to client applications. \(Double-quotes are needed to specify constraint names that contain spaces.\) If a constraint name is not specified, the system generates a name.

    > **Note** The specified constraint\_name is used for the constraint, but a system-generated unique name is used for the index name. In some prior releases, the provided name was used for both the constraint name and the index name.

NULL \| NOT NULL
:   Specifies if the column is or is not allowed to contain null values. `NULL` is the default.

CHECK \(expression\) \[ NO INHERIT \]
:   The `CHECK` clause specifies an expression producing a Boolean result which new or updated rows must satisfy for an insert or update operation to succeed. Expressions evaluating to `TRUE` or `UNKNOWN` succeed. Should any row of an insert or update operation produce a `FALSE` result an error exception is raised and the insert or update does not alter the database. A check constraint specified as a column constraint should reference that column's value only, while an expression appearing in a table constraint can reference multiple columns.

:   A constraint marked with `NO INHERIT` will not propagate to child tables.

:   Currently, `CHECK` expressions cannot contain subqueries nor refer to variables other than columns of the current row.

UNIQUE \( column\_constraint \)
UNIQUE \( column\_name \[, ... \] \) \( table\_constraint \)
:   The `UNIQUE` constraint specifies that a group of one or more columns of a table may contain only unique values. The behavior of the unique table constraint is the same as that for column constraints, with the additional capability to span multiple columns. For the purpose of a unique constraint, null values are not considered equal. The column\(s\) that are unique must contain all the columns of the Greenplum distribution key. In addition, the `<key>` must contain all the columns in the partition key if the table is partitioned. Note that a `<key>` constraint in a partitioned table is not the same as a simple `UNIQUE INDEX`.

:   For information about unique constraint management and limitations, see [Notes](#section5).

PRIMARY KEY \( column constraint \)
PRIMARY KEY \( column\_name \[, ... \] \) \( table constraint \)
:   The `PRIMARY KEY` constraint specifies that a column or columns of a table may contain only unique \(non-duplicate\), non-null values. Only one primary key can be specified for a table, whether as a column constraint or a table constraint.

:   For a table to have a primary key, it must be hash distributed \(not randomly distributed\), and the primary key, the column\(s\) that are unique, must contain all the columns of the Greenplum distribution key. In addition, the `<key>` must contain all the columns in the partition key if the table is partitioned. Note that a `<key>` constraint in a partitioned table is not the same as a simple `UNIQUE INDEX`.

:   `PRIMARY KEY` enforces the same data constraints as a combination of `UNIQUE` and `NOT NULL`, but identifying a set of columns as the primary key also provides metadata about the design of the schema, since a primary key implies that other tables can rely on this set of columns as a unique identifier for rows.

:   For information about primary key management and limitations, see [Notes](#section5).

REFERENCES reftable \[ \( refcolumn \) \]
\[ MATCH FULL \| MATCH PARTIAL \| MATCH SIMPLE \]
\[ON DELETE \| ON UPDATE\] \[key\_action\]
FOREIGN KEY \(column\_name \[, ...\]\)
:   The `REFERENCES` and `FOREIGN KEY` clauses specify referential integrity constraints \(foreign key constraints\). Greenplum accepts referential integrity constraints as specified in PostgreSQL syntax but does not enforce them. See the PostgreSQL documentation for information about referential integrity constraints.

DEFERRABLE
NOT DEFERRABLE
:   The `[NOT] DEFERRABLE` clause controls whether the constraint can be deferred. A constraint that is not deferrable will be checked immediately after every command. Checking of constraints that are deferrable can be postponed until the end of the transaction \(using the [`SET CONSTRAINTS`](SET_CONSTRAINTS.html) command\). `NOT DEFERRABLE` is the default. Currently, only `UNIQUE` and `PRIMARY KEY` constraints are deferrable. `NOT NULL` and `CHECK` constraints are not deferrable. `REFERENCES` \(foreign key\) constraints accept this clause but are not enforced.

INITIALLY IMMEDIATE
INITIALLY DEFERRED
:   If a constraint is deferrable, this clause specifies the default time to check the constraint. If the constraint is `INITIALLY IMMEDIATE`, it is checked after each statement. This is the default. If the constraint is `INITIALLY DEFERRED`, it is checked only at the end of the transaction. The constraint check time can be altered with the `SET CONSTRAINTS` command.

USING <access_method>
:   The `USING` clause specifies the access method for the table you are creating. Set to `heap` to access the table as a heap-storage table, `ao_row` to access the table as an append-optimized table with row-oriented storage (AO), or `ao_column` to access the table as an append-optimized table with column-oriented storage (AOCO).The default is determined by the value of the `default_table_access_method` server configuration parameter.

  <p class="note">
<strong>Note:</strong>
Although you can specify the table's access method using <code>WITH (appendoptimized=true|false, orientation=row|column)</code> VMware recommends that you use <code>USING <access_method></code> instead.
</p>
  
WITH \( storage\_parameter=value \)
:   The `WITH` clause can specify storage parameters for tables, and for indexes associated with a `UNIQUE` or `PRIMARY` constraint. See [Storage Parameters](#storage-parameters), below, for details.  

ON COMMIT
:   The behavior of temporary tables at the end of a transaction block can be controlled using `ON COMMIT`. The three options are:

:   **PRESERVE ROWS** - No special action is taken at the ends of transactions for temporary tables. This is the default behavior.

:   **DELETE ROWS** - All rows in the temporary table will be deleted at the end of each transaction block. Essentially, an automatic `TRUNCATE` is done at each commit.

:   **DROP** - The temporary table will be dropped at the end of the current transaction block.

TABLESPACE tablespace
:   The name of the tablespace in which the new table is to be created. If not specified, the database's default tablespace is used, or [temp\_tablespaces](../config_params/guc-list.html) if the table is temporary.

USING INDEX TABLESPACE tablespace
:   This clause allows selection of the tablespace in which the index associated with a `UNIQUE` or `PRIMARY KEY` constraint will be created. If not specified, the database's default tablespace is used, or [temp\_tablespaces](../config_params/guc-list.html) if the table is temporary.

DISTRIBUTED BY \(column \[opclass\], \[ ... \] \)
DISTRIBUTED RANDOMLY
DISTRIBUTED REPLICATED
:   Used to declare the Greenplum Database distribution policy for the table. `DISTRIBUTED BY` uses hash distribution with one or more columns declared as the distribution key. For the most even data distribution, the distribution key should be the primary key of the table or a unique column \(or set of columns\). If that is not possible, then you may choose `DISTRIBUTED RANDOMLY`, which will send the data round-robin to the segment instances. Additionally, an operator class, `opclass`, can be specified, to use a non-default hash function.

:   The Greenplum Database server configuration parameter `gp_create_table_random_default_distribution` controls the default table distribution policy if the DISTRIBUTED BY clause is not specified when you create a table. Greenplum Database follows these rules to create a table if a distribution policy is not specified.

    If the value of the parameter is `off` \(the default\), Greenplum Database chooses the table distribution key based on the command:

    -   If a `LIKE` or `INHERITS` clause is specified, then Greenplum copies the distribution key from the source or parent table.
    -   If a `PRIMARY KEY` or `UNIQUE` constraints are specified, then Greenplum chooses the largest subset of all the key columns as the distribution key.
    -   If neither constraints nor a `LIKE` or `INHERITS` clause is specified, then Greenplum chooses the first suitable column as the distribution key. \(Columns with geometric or user-defined data types are not eligible as Greenplum distribution key columns.\)

    If the value of the parameter is set to `on`, Greenplum Database follows these rules:

    -   If PRIMARY KEY or UNIQUE columns are not specified, the distribution of the table is random \(DISTRIBUTED RANDOMLY\). Table distribution is random even if the table creation command contains the LIKE or INHERITS clause.
    -   If PRIMARY KEY or UNIQUE columns are specified, a DISTRIBUTED BY clause must also be specified. If a DISTRIBUTED BY clause is not specified as part of the table creation command, the command fails.

:   For more information about setting the default table distribution policy, see [`gp_create_table_random_default_distribution`](../config_params/guc-list.html).

:   The `DISTRIBUTED REPLICATED` clause replicates the entire table to all Greenplum Database segment instances. It can be used when it is necessary to run user-defined functions on segments when the functions require access to all rows in the table, or to improve query performance by preventing broadcast motions.

PARTITION BY
:   Declares one or more columns by which to partition the table.

:   When creating a partitioned table, Greenplum Database creates the root partitioned table \(the root partition\) with the specified table name. Greenplum Database also creates a hierarchy of tables, child tables, that are the subpartitions based on the partitioning options that you specify. The Greenplum Database *pg\_partition*\* system views contain information about the subpartition tables.

:   For each partition level \(each hierarchy level of tables\), a partitioned table can have a maximum of 32,767 partitions.

:   > **Note** Greenplum Database stores partitioned table data in the leaf child tables, the lowest-level tables in the hierarchy of child tables for use by the partitioned table.

:   partition\_type
:   Declares partition type: `LIST` \(list of values\) or `RANGE` \(a numeric or date range\).

partition\_specification
:   Declares the individual partitions to create. Each partition can be defined individually or, for range partitions, you can use the `EVERY` clause \(with a `START` and optional `END` clause\) to define an increment pattern to use to create the individual partitions.

:   **`DEFAULT PARTITION name`** — Declares a default partition. When data does not match to an existing partition, it is inserted into the default partition. Partition designs that do not have a default partition will reject incoming rows that do not match to an existing partition.

:   **`PARTITION name`** — Declares a name to use for the partition. Partitions are created using the following naming convention: `parentname_level\#_prt_givenname`.

:   **`VALUES`** — For list partitions, defines the value\(s\) that the partition will contain.

:   **`START`** — For range partitions, defines the starting range value for the partition. By default, start values are `INCLUSIVE`. For example, if you declared a start date of '`2016-01-01`', then the partition would contain all dates greater than or equal to '`2016-01-01`'. Typically the data type of the `START` expression is the same type as the partition key column. If that is not the case, then you must explicitly cast to the intended data type.

:   **`END`** — For range partitions, defines the ending range value for the partition. By default, end values are `EXCLUSIVE`. For example, if you declared an end date of '`2016-02-01`', then the partition would contain all dates less than but not equal to '`2016-02-01`'. Typically the data type of the `END` expression is the same type as the partition key column. If that is not the case, then you must explicitly cast to the intended data type.

:   **`EVERY`** — For range partitions, defines how to increment the values from `START` to `END` to create individual partitions. Typically the data type of the `EVERY` expression is the same type as the partition key column. If that is not the case, then you must explicitly cast to the intended data type.

:   **`WITH`**— Sets the table storage options for a partition. For example, you may want older partitions to be append-optimized tables and newer partitions to be regular heap tables. See [Storage Parameters](#storage-parameters), below.

:   **`TABLESPACE`** — The name of the tablespace in which the partition is to be created.

SUBPARTITION BY
:   Declares one or more columns by which to subpartition the first-level partitions of the table. The format of the subpartition specification is similar to that of a partition specification described above.

SUBPARTITION TEMPLATE
:   Instead of declaring each subpartition definition individually for each partition, you can optionally declare a subpartition template to be used to create the subpartitions \(lower level child tables\). This subpartition specification would then apply to all parent partitions.

### <a id="storage_parameters"></a>Storage Parameters

The `WITH` clause can specify storage parameters for tables, and for indexes associated with a `UNIQUE` or `PRIMARY` constraint. Storage parameters for indexes are documented on the [CREATE INDEX](CREATE_INDEX.html.md) reference page. Note that you can also set storage parameters for a particular partition or subpartition by declaring the `WITH` clause in the partition specification. The lowest-level partition's settings have priority. 

The defaults for some of the table storage options can be specified with the server configuration parameter `gp_default_storage_options`. For information about setting default storage options, see [Notes](#section5).

Greenplum Database supports the following storage parameters for tables and partitions:

appendoptimized
: Set to `TRUE` to create the table as an append-optimized table. If `FALSE` or not declared, the table will be created as a regular heap-storage table.

blocksize
: Set to the size, in bytes, for each block in a table. The `blocksize` must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default is 32768. The `blocksize` option is valid only if `appendoptimized=TRUE`.

orientation
: Set to `column` for column-oriented storage, or `row` \(the default\) for row-oriented storage. This option is only valid if `appendoptimized=TRUE`. Heap-storage tables can only be row-oriented.

checksum
: This option is valid only for append-optimized tables \(`appendoptimized=TRUE`\). The value `TRUE` is the default and enables CRC checksum validation for append-optimized tables. The checksum is calculated during block creation and is stored on disk. Checksum validation is performed during block reads. If the checksum calculated during the read does not match the stored checksum, the transaction is cancelled. If you set the value to `FALSE` to deactivate checksum validation, checking the table data for on-disk corruption will not be performed.

compresstype
: Set to `ZLIB` \(the default\), `ZSTD`, `RLE_TYPE`, or `QUICKLZ` <sup>1</sup> to specify the type of compression used. The value `NONE` deactivates compression. Zstd provides for both speed and a good compression ratio, tunable with the `compresslevel` option. QuickLZ and zlib are provided for backwards-compatibility. Zstd outperforms these compression types on usual workloads. The `compresstype` option is only valid if `appendoptimized=TRUE`.

<sup>1</sup>QuickLZ compression is available only in the commercial release of VMware Greenplum.

:    The value `RLE_TYPE`, which is supported only if `orientation`=`column` is specified, enables the run-length encoding \(RLE\) compression algorithm. RLE compresses data better than the Zstd, zlib, or QuickLZ compression algorithms when the same data value occurs in many consecutive rows.

:    For columns of type `BIGINT`, `INTEGER`, `DATE`, `TIME`, or `TIMESTAMP`, delta compression is also applied if the `compresstype` option is set to `RLE_TYPE` compression. The delta compression algorithm is based on the delta between column values in consecutive rows and is designed to improve compression when data is loaded in sorted order or the compression is applied to column data that is in sorted order.

:    For information about using table compression, see [Choosing the Table Storage Model](../../admin_guide/ddl/ddl-storage.html#topic1) in the *Greenplum Database Administrator Guide*.

compresslevel
: For Zstd compression of append-optimized tables, set to an integer value from 1 \(fastest compression\) to 19 \(highest compression ratio\). For zlib compression, the valid range is from 1 to 9. QuickLZ compression level can only be set to 1. If not declared, the default is 1. For `RLE_TYPE`, the compression level can be an integer value from 1 \(fastest compression\) to 4 \(highest compression ratio\).

:   The `compresslevel` option is valid only if `appendoptimized=TRUE`.

fillfactor
: The fillfactor for a table is a percentage between 10 and 100. 100 \(complete packing\) is the default. When a smaller fillfactor is specified, `INSERT` operations pack table pages only to the indicated percentage; the remaining space on each page is reserved for updating rows on that page. This gives `UPDATE` a chance to place the updated copy of a row on the same page as the original, which is more efficient than placing it on a different page. For a table whose entries are never updated, complete packing is the best choice, but in heavily updated tables smaller fillfactors are appropriate. This parameter cannot be set for TOAST tables.

analyze_hll_non_part_table
: Set this storage parameter to `true` to force collection of HLL statistics even if the table is not part of a partitioned table. This is useful if the table will be exchanged or added to a partitioned table, so that the table does not need to be re-analyzed. The default is `false`.

reorganize
: Set this storage parameter to `true` when the hash distribution policy has not changed or when you have changed from a hash to a random distribution, and you want to redistribute the data anyway.

vacuum_index_cleanup
: Specifies whether `VACUUM` attempts to remove index entries pointing to dead tuples. The default is `true`. Setting this to false may be useful when you need to run `VACUUM` as quickly as possible, for example to prevent imminent transaction ID wraparound. However, if you do not perform index cleanup regularly, performance may suffer, because as the table is modified, indexes accumulate dead tuples and the table itself accumulates dead line pointers that cannot be removed until index cleanup completes.

## <a id="section5"></a>Notes 

-   In Greenplum Database \(a Postgres-based system\) the data types `VARCHAR` or `TEXT` handle padding added to the textual data \(space characters added after the last non-space character\) as significant characters; the data type `CHAR` does not.

    In Greenplum Database, values of type `CHAR(n)` are padded with trailing spaces to the specified width n. The values are stored and displayed with the spaces. However, the padding spaces are treated as semantically insignificant. When the values are distributed, the trailing spaces are disregarded. The trailing spaces are also treated as semantically insignificant when comparing two values of data type `CHAR`, and the trailing spaces are removed when converting a character value to one of the other string types.

-   VMware does not support using `WITH OIDS` or `oids=TRUE` to assign an OID system column.Using OIDs in new applications is not recommended. This syntax is deprecated and will be removed in a future Greenplum release. As an alternative, use a `SERIAL` or other sequence generator as the table's primary key. However, if your application does make use of OIDs to identify specific rows of a table, it is recommended to create a unique constraint on the OID column of that table, to ensure that OIDs in the table will indeed uniquely identify rows even after counter wrap-around. Avoid assuming that OIDs are unique across tables; if you need a database-wide unique identifier, use the combination of table OID and row OID for that purpose.
-   Greenplum Database has some special conditions for primary key and unique constraints with regards to columns that are the *distribution key* in a Greenplum table. For a unique constraint to be enforced in Greenplum Database, the table must be hash-distributed \(not `DISTRIBUTED RANDOMLY`\), and the constraint columns must be the same as \(or a superset of\) the table's distribution key columns.

    Replicated tables \(`DISTRIBUTED REPLICATED`\) can have both `PRIMARY KEY` and `UNIQUE`column constraints.

    A primary key constraint is simply a combination of a unique constraint and a not-null constraint.

    Greenplum Database automatically creates a `UNIQUE` index for each `UNIQUE` or `PRIMARY KEY` constraint to enforce uniqueness. Thus, it is not necessary to create an index explicitly for primary key columns.

    Foreign key constraints are not supported in Greenplum Database.

    For inherited tables, unique constraints, primary key constraints, indexes and table privileges are *not* inherited in the current implementation.

-   For append-optimized tables, `UPDATE` and `DELETE` are not allowed in a repeatable read or serializable transaction and will cause the transaction to end prematurely. `DECLARE...FOR UPDATE`, and triggers are not supported with append-optimized tables. `CLUSTER` on append-optimized tables is only supported over B-tree indexes.
-   To insert data into a partitioned table, you specify the root partitioned table, the table created with the `CREATE TABLE` command. You also can specify a leaf child table of the partitioned table in an `INSERT` command. An error is returned if the data is not valid for the specified leaf child table. Specifying a child table that is not a leaf child table in the `INSERT` command is not supported. Execution of other DML commands such as `UPDATE` and `DELETE` on any child table of a partitioned table is not supported. These commands must be run on the root partitioned table, the table created with the `CREATE TABLE` command.
-   The default values for these table storage options can be specified with the server configuration parameter `gp_default_storage_option`.

    -   appendoptimized
    -   blocksize
    -   checksum
    -   compresstype
    -   compresslevel
    -   orientation
    The defaults can be set for the system, a database, or a user. For information about setting storage options, see the server configuration parameter [gp\_default\_storage\_options](../config_params/guc-list.html).


> **Important** The current Postgres Planner allows list partitions with multi-column \(composite\) partition keys. GPORCA does not support composite keys, so using composite partition keys is not recommended.

## <a id="section6"></a>Examples 

Create a table named `rank` in the schema named `baby` and distribute the data using the columns `rank`, `gender`, and `year`:

```
CREATE TABLE baby.rank (id int, rank int, year smallint, 
gender char(1), count int ) DISTRIBUTED BY (rank, gender, 
year);
```

Create table films and table distributors \(the primary key will be used as the Greenplum distribution key by default\):

```
CREATE TABLE films (
code        char(5) CONSTRAINT firstkey PRIMARY KEY,
title       varchar(40) NOT NULL,
did         integer NOT NULL,
date_prod   date,
kind        varchar(10),
len         interval hour to minute
);

CREATE TABLE distributors (
did    integer PRIMARY KEY DEFAULT nextval('serial'),
name   varchar(40) NOT NULL CHECK (name <> '')
);
```

Create a gzip-compressed, append-optimized table:

```
CREATE TABLE sales (txn_id int, qty int, date date) 
WITH (appendoptimized=true, compresslevel=5) 
DISTRIBUTED BY (txn_id);
```

Create a simple, single level partitioned table:

```
CREATE TABLE sales (id int, year int, qtr int, c_rank int, code char(1), region text)
DISTRIBUTED BY (id)
PARTITION BY LIST (code)
( PARTITION sales VALUES ('S'),
  PARTITION returns VALUES ('R')
);
```

Create a three level partitioned table that defines subpartitions without the `SUBPARTITION TEMPLATE` clause:

```
CREATE TABLE sales (id int, year int, qtr int, c_rank int, code char(1), region text)
DISTRIBUTED BY (id)
PARTITION BY LIST (code)
  SUBPARTITION BY RANGE (c_rank)
    SUBPARTITION by LIST (region)

( PARTITION sales VALUES ('S')
   ( SUBPARTITION cr1 START (1) END (2)
      ( SUBPARTITION ca VALUES ('CA') ), 
      SUBPARTITION cr2 START (3) END (4)
        ( SUBPARTITION ca VALUES ('CA') ) ),

 PARTITION returns VALUES ('R')
   ( SUBPARTITION cr1 START (1) END (2)
      ( SUBPARTITION ca VALUES ('CA') ), 
     SUBPARTITION cr2 START (3) END (4)
        ( SUBPARTITION ca VALUES ('CA') ) )
);
```

Create the same partitioned table as the previous table using the `SUBPARTITION TEMPLATE` clause:

```
CREATE TABLE sales1 (id int, year int, qtr int, c_rank int, code char(1), region text)
DISTRIBUTED BY (id)
PARTITION BY LIST (code)

   SUBPARTITION BY RANGE (c_rank)
     SUBPARTITION TEMPLATE (
     SUBPARTITION cr1 START (1) END (2),
     SUBPARTITION cr2 START (3) END (4) )

     SUBPARTITION BY LIST (region)
       SUBPARTITION TEMPLATE (
       SUBPARTITION ca VALUES ('CA') )

( PARTITION sales VALUES ('S'),
  PARTITION  returns VALUES ('R')
);
```

Create a three level partitioned table using subpartition templates and default partitions at each level:

```
CREATE TABLE sales (id int, year int, qtr int, c_rank int, code char(1), region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)

  SUBPARTITION BY RANGE (qtr)
    SUBPARTITION TEMPLATE (
    START (1) END (5) EVERY (1), 
    DEFAULT SUBPARTITION bad_qtr )

    SUBPARTITION BY LIST (region)
      SUBPARTITION TEMPLATE (
      SUBPARTITION usa VALUES ('usa'),
      SUBPARTITION europe VALUES ('europe'),
      SUBPARTITION asia VALUES ('asia'),
      DEFAULT SUBPARTITION other_regions)

( START (2009) END (2011) EVERY (1),
  DEFAULT PARTITION outlying_years);
```

## <a id="section7"></a>Compatibility 

`CREATE TABLE` command conforms to the SQL standard, with the following exceptions:

-   **Temporary Tables** — In the SQL standard, temporary tables are defined just once and automatically exist \(starting with empty contents\) in every session that needs them. Greenplum Database instead requires each session to issue its own `CREATE TEMPORARY TABLE` command for each temporary table to be used. This allows different sessions to use the same temporary table name for different purposes, whereas the standard's approach constrains all instances of a given temporary table name to have the same table structure.

    The standard's distinction between global and local temporary tables is not in Greenplum Database. Greenplum Database will accept the `GLOBAL` and `LOCAL` keywords in a temporary table declaration, but they have no effect and are deprecated.

    If the `ON COMMIT` clause is omitted, the SQL standard specifies that the default behavior as `ON COMMIT DELETE ROWS`. However, the default behavior in Greenplum Database is `ON COMMIT PRESERVE ROWS`. The `ON COMMIT DROP` option does not exist in the SQL standard.

-   **Column Check Constraints** — The SQL standard says that `CHECK` column constraints may only refer to the column they apply to; only `CHECK` table constraints may refer to multiple columns. Greenplum Database does not enforce this restriction; it treats column and table check constraints alike.
-   **NULL Constraint** — The `NULL` constraint is a Greenplum Database extension to the SQL standard that is included for compatibility with some other database systems \(and for symmetry with the `NOT NULL` constraint\). Since it is the default for any column, its presence is not required.
-   **Inheritance** — Multiple inheritance via the `INHERITS` clause is a Greenplum Database language extension. SQL:1999 and later define single inheritance using a different syntax and different semantics. SQL:1999-style inheritance is not yet supported by Greenplum Database.
-   **Partitioning** — Table partitioning via the `PARTITION BY` clause is a Greenplum Database language extension.
-   **Zero-column tables** — Greenplum Database allows a table of no columns to be created \(for example, `CREATE TABLE foo();`\). This is an extension from the SQL standard, which does not allow zero-column tables. Zero-column tables are not in themselves very useful, but disallowing them creates odd special cases for `ALTER TABLE DROP COLUMN`, so Greenplum decided to ignore this spec restriction.
-   **LIKE** — While a `LIKE` clause exists in the SQL standard, many of the options that Greenplum Database accepts for it are not in the standard, and some of the standard's options are not implemented by Greenplum Database.
-   **WITH clause** — The `WITH` clause is a Greenplum Database extension; neither storage parameters nor OIDs are in the standard.
-   **Tablespaces** — The Greenplum Database concept of tablespaces is not part of the SQL standard. The clauses `TABLESPACE` and `USING INDEX TABLESPACE` are extensions.
-   **Data Distribution** — The Greenplum Database concept of a parallel or distributed database is not part of the SQL standard. The `DISTRIBUTED` clauses are extensions.

## <a id="section8"></a>See Also 

[ALTER TABLE](ALTER_TABLE.html), [DROP TABLE](DROP_TABLE.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE TABLE AS](CREATE_TABLE_AS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

