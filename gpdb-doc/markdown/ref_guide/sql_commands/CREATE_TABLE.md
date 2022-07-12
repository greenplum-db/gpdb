# CREATE TABLE 

Defines a new table.

**Note:** Referential integrity syntax \(foreign key constraints\) is accepted but not enforced.

## Synopsis 

``` {#sql_command_synopsis}
CREATE [[GLOBAL | LOCAL] {TEMPORARY | TEMP}] TABLE <table_name> ( 
[ { <column_name> <data_type> [ DEFAULT <default_expr> ] 
   [<column_constraint> [ ... ]
[ ENCODING ( <storage_directive> [,...] ) ]
] 
   | <table_constraint>
   | LIKE <other_table> [{INCLUDING | EXCLUDING} 
                      {DEFAULTS | CONSTRAINTS}] ...}
   [, ... ] ]
   )
   [ INHERITS ( <parent_table> [, ... ] ) ]
   [ WITH ( <storage_parameter>=<value> [, ... ] )
   [ ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP} ]
   [ TABLESPACE <tablespace> ]
   [ DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY ]
   [ PARTITION BY <partition_type> (<column>)
       [ SUBPARTITION BY <partition_type> (<column>) ] 
          [ SUBPARTITION TEMPLATE ( <template_spec >) ]
       [...]
    ( <partition_spec> ) 
        | [ SUBPARTITION BY <partition_type> (<column>) ]
          [...]
    ( <partition_spec>
      [ ( <subpartition_spec>
           [(...)] 
         ) ] 
    )
```

where column\_constraint is:

```
   [CONSTRAINT <constraint_name>]
   NOT NULL | NULL 
   | UNIQUE [USING INDEX TABLESPACE <tablespace>]
            [WITH ( FILLFACTOR = <value> )]
   | PRIMARY KEY [USING INDEX TABLESPACE <tablespace>] 
                 [WITH ( FILLFACTOR = <value> )]
   | CHECK ( <expression> )
   | REFERENCES table_name [ ( column_name [, ... ] ) ] 
            [ key_match_type ]
            [ key_action ]
```

where storage\_directive for a column is:

```
   COMPRESSTYPE={ZLIB | QUICKLZ | RLE_TYPE | NONE}
    [COMPRESSLEVEL={0-9} ]
    [BLOCKSIZE={8192-2097152} ]
```

where storage\_parameter for the table is:

```
   APPENDONLY={TRUE|FALSE}
   BLOCKSIZE={8192-2097152}
   ORIENTATION={COLUMN|ROW}
   CHECKSUM={TRUE|FALSE}
   COMPRESSTYPE={ZLIB|QUICKLZ|RLE_TYPE|NONE}
   COMPRESSLEVEL={0-9}
   FILLFACTOR={10-100}
   OIDS[=TRUE|FALSE]
```

and table\_constraint is:

```
   [CONSTRAINT <constraint_name>]
   UNIQUE ( <column_name> [, ... ] )
          [USING INDEX TABLESPACE <tablespace>] 
          [WITH ( FILLFACTOR=<value> )] 
   | PRIMARY KEY ( <column_name> [, ... ] ) 
                 [USING INDEX TABLESPACE <tablespace>] 
                 [WITH ( FILLFACTOR=<value> )] 
   | CHECK ( <expression> )
   | FOREIGN KEY ( <column_name> [, ... ] )
            REFERENCES table_name [ ( column_name [, ... ] ) ]
            [ <key_match_type> ]
            [ <key_action> ]
            [ <key_checking_mode> ]
```

where key\_match\_type is:

```
    MATCH FULL
  | SIMPLE
```

where key\_action is:

```
    ON DELETE 
  | ON UPDATE
  | NO ACTION
  | RESTRICT
  | CASCADE
  | SET NULL
  | SET DEFAULT
```

where key\_checking\_mode is:

```
    DEFERRABLE
  | NOT DEFERRABLE
  | INITIALLY DEFERRED
  | INITIALLY IMMEDIATE
```

where partition\_type is:

```
    LIST
  | RANGE
```

where partition\_specification is:

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
     [ EVERY ([<datatype>] [<number | >INTERVAL] '<interval_value>') ]
  | [PARTITION <name>] 
     END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
     [ EVERY ([<datatype>] [<number | >INTERVAL] '<interval_value>') ]
[ WITH ( <partition_storage_parameter>=<value> [, ... ] ) ]
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
     [ EVERY ([<datatype>] [<number | >INTERVAL] '<interval_value>') ]
  | [SUBPARTITION <name>] 
     END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
     [ EVERY ([<datatype>] [<number | >INTERVAL] '<interval_value>') ]
[ WITH ( <partition_storage_parameter>=<value> [, ... ] ) ]
[ TABLESPACE <tablespace> ]
```

where storage\_parameter for a partition is:

```
   APPENDONLY={TRUE|FALSE}
   BLOCKSIZE={8192-2097152}
   ORIENTATION={COLUMN|ROW}
   CHECKSUM={TRUE|FALSE}
   COMPRESSTYPE={ZLIB|QUICKLZ|RLE_TYPE|NONE}
   COMPRESSLEVEL={1-9}
   FILLFACTOR={10-100}
   OIDS[=TRUE|FALSE]
```

## Description 

`CREATE TABLE` creates an initially empty table in the current database. The user who issues the command owns the table.

If you specify a schema name, Greenplum creates the table in the specified schema. Otherwise Greenplum creates the table in the current schema. Temporary tables exist in a special schema, so you cannot specify a schema name when creating a temporary table. Table names must be distinct from the name of any other table, external table, sequence, index, or view in the same schema.

The optional constraint clauses specify conditions that new or updated rows must satisfy for an insert or update operation to succeed. A constraint is an SQL object that helps define the set of valid values in the table in various ways. Constraints apply to tables, not to partitions. You cannot add a constraint to a partition or subpartition.

Referential integrity constraints \(foreign keys\) are accepted but not enforced. The information is kept in the system catalogs but is otherwise ignored.

There are two ways to define constraints: table constraints and column constraints. A column constraint is defined as part of a column definition. A table constraint definition is not tied to a particular column, and it can encompass more than one column. Every column constraint can also be written as a table constraint; a column constraint is only a notational convenience for use when the constraint only affects one column.

When creating a table, there is an additional clause to declare the Greenplum Database distribution policy. If a `DISTRIBUTED BY` or `DISTRIBUTED RANDOMLY` clause is not supplied, then Greenplum assigns a hash distribution policy to the table using either the `PRIMARY KEY` \(if the table has one\) or the first column of the table as the distribution key. Columns of geometric or user-defined data types are not eligible as Greenplum distribution key columns. If a table does not have a column of an eligible data type, the rows are distributed based on a round-robin or random distribution. To ensure an even distribution of data in your Greenplum Database system, you want to choose a distribution key that is unique for each record, or if that is not possible, then choose `DISTRIBUTED RANDOMLY`.

The `PARTITION BY` clause allows you to divide the table into multiple sub-tables \(or parts\) that, taken together, make up the parent table and share its schema. Though the sub-tables exist as independent tables, the Greenplum Database restricts their use in important ways. Internally, partitioning is implemented as a special form of inheritance. Each child table partition is created with a distinct `CHECK` constraint which limits the data the table can contain, based on some defining criteria. The `CHECK` constraints are also used by the query optimizer to determine which table partitions to scan in order to satisfy a given query predicate. These partition constraints are managed automatically by the Greenplum Database.

## Parameters 

GLOBAL \| LOCAL
:   These keywords are present for SQL standard compatibility, but have no effect in Greenplum Database.

TEMPORARY \| TEMP
:   If specified, the table is created as a temporary table. Temporary tables are automatically dropped at the end of a session, or optionally at the end of the current transaction \(see `ON COMMIT`\). Existing permanent tables with the same name are not visible to the current session while the temporary table exists, unless they are referenced with schema-qualified names. Any indexes created on a temporary table are automatically temporary as well.

table\_name
:   The name \(optionally schema-qualified\) of the table to be created.

column\_name
:   The name of a column to be created in the new table.

data\_type
:   The data type of the column. This may include array specifiers.

:   For table columns that contain textual data, Specify the data type `VARCHAR` or `TEXT`. Specifying the data type `CHAR` is not recommended. In Greenplum Database, the data types `VARCHAR` or `TEXT` handles padding added to the data \(space characters added after the last non-space character\) as significant characters, the data type `CHAR` does not. See [Notes](#section5).

DEFAULT default\_expr
:   The `DEFAULT` clause assigns a default data value for the column whose column definition it appears within. The value is any variable-free expression \(subqueries and cross-references to other columns in the current table are not allowed\). The data type of the default expression must match the data type of the column. The default expression will be used in any insert operation that does not specify a value for the column. If there is no default for a column, then the default is null.

ENCODING \( storage\_directive \[, ...\] \)
:   For a column, the optional `ENCODING` clause specifies the type of compression and block size for the column data. See [storage\_options](#with_storage) for `COMPRESSTYPE`, `COMPRESSLEVEL`, and `BLOCKSIZE` values.

:   The clause is valid only for append-optimized, column-oriented tables.

:   Column compression settings are inherited from the table level to the partition level to the subpartition level. The lowest-level settings have priority.

INHERITS
:   The optional `INHERITS` clause specifies a list of tables from which the new table automatically inherits all columns. Use of `INHERITS` creates a persistent relationship between the new child table and its parent table\(s\). Schema modifications to the parent\(s\) normally propagate to children as well, and by default the data of the child table is included in scans of the parent\(s\).

:   In Greenplum Database, the `INHERITS` clause is not used when creating partitioned tables. Although the concept of inheritance is used in partition hierarchies, the inheritance structure of a partitioned table is created using the [PARTITION BY](#part_by) clause.

:   If the same column name exists in more than one parent table, an error is reported unless the data types of the columns match in each of the parent tables. If there is no conflict, then the duplicate columns are merged to form a single column in the new table. If the column name list of the new table contains a column name that is also inherited, the data type must likewise match the inherited column\(s\), and the column definitions are merged into one. However, inherited and new column declarations of the same name need not specify identical constraints: all constraints provided from any declaration are merged together and all are applied to the new table. If the new table explicitly specifies a default value for the column, this default overrides any defaults from inherited declarations of the column. Otherwise, any parents that specify default values for the column must all specify the same default, or an error will be reported.

LIKE other\_table \[\{INCLUDING \| EXCLUDING\} \{DEFAULTS \| CONSTRAINTS\}\]
:   The `LIKE` clause specifies a table from which the new table automatically copies all column names, data types, not-null constraints, and distribution policy. Storage properties like append-optimized or partition structure are not copied. Unlike `INHERITS`, the new table and original table are completely decoupled after creation is complete.

:   Default expressions for the copied column definitions will only be copied if `INCLUDING DEFAULTS` is specified. The default behavior is to exclude default expressions, resulting in the copied columns in the new table having null defaults.

:   Not-null constraints are always copied to the new table. `CHECK` constraints will only be copied if `INCLUDING CONSTRAINTS` is specified; other types of constraints will never be copied. Also, no distinction is made between column constraints and table constraints — when constraints are requested, all check constraints are copied.

:   Note also that unlike `INHERITS`, copied columns and constraints are not merged with similarly named columns and constraints. If the same name is specified explicitly or in another `LIKE` clause an error is signalled.

CONSTRAINT constraint\_name
:   An optional name for a column or table constraint. If the constraint is violated, the constraint name is present in error messages, so constraint names like column must be positive can be used to communicate helpful constraint information to client applications. \(Double-quotes are needed to specify constraint names that contain spaces.\) If a constraint name is not specified, the system generates a name.

    **Note:** The specified constraint\_name is used for the constraint, but a system-generated unique name is used for the index name. In some prior releases, the provided name was used for both the constraint name and the index name.

NULL \| NOT NULL
:   Specifies if the column is or is not allowed to contain null values. `NULL` is the default.

UNIQUE \( column constraint \)
UNIQUE \( column\_name \[, ... \] \) \( table constraint \)
:   The `UNIQUE` constraint specifies that a group of one or more columns of a table may contain only unique values. The behavior of the unique table constraint is the same as that for column constraints, with the additional capability to span multiple columns. For the purpose of a unique constraint, null values are not considered equal. The column\(s\) that are unique must contain all the columns of the Greenplum distribution key. In addition, the `<key>` must contain all the columns in the partition key if the table is partitioned. Note that a `<key>` constraint in a partitioned table is not the same as a simple `UNIQUE INDEX`.

:   For information about unique constraint management and limitations, see [Notes](#section5).

PRIMARY KEY \( column constraint \)
PRIMARY KEY \( column\_name \[, ... \] \) \( table constraint \)
:   The primary key constraint specifies that a column or columns of a table may contain only unique \(non-duplicate\), non-null values. Technically, `PRIMARY KEY` is merely a combination of `UNIQUE` and `NOT NULL`, but identifying a set of columns as primary key also provides metadata about the design of the schema, as a primary key implies that other tables may rely on this set of columns as a unique identifier for rows. For a table to have a primary key, it must be hash distributed \(not randomly distributed\), and the primary key The column\(s\) that are unique must contain all the columns of the Greenplum distribution key. In addition, the `<key>` must contain all the columns in the partition key if the table is partitioned. Note that a `<key>` constraint in a partitioned table is not the same as a simple `UNIQUE INDEX`.

:   For information about primary key management and limitations, see [Notes](#section5).

CHECK \( expression \)
:   The `CHECK` clause specifies an expression producing a Boolean result which new or updated rows must satisfy for an insert or update operation to succeed. Expressions evaluating to `TRUE` or `UNKNOWN` succeed. Should any row of an insert or update operation produce a `FALSE` result an error exception is raised and the insert or update does not alter the database. A check constraint specified as a column constraint should reference that column's value only, while an expression appearing in a table constraint may reference multiple columns. `CHECK` expressions cannot contain subqueries nor refer to variables other than columns of the current row.

REFERENCES table\_name \[ \( column\_name \[, ... \] \) \]
\[key\_match\_type\] \[ key\_action \]
FOREIGN KEY \( column\_name \[, ... \] \)
  REFERENCES table\_name \[ \( column\_name \[, ... \] \)
\[key\_match\_type\] \[ key\_action \[ key\_checking\_mode \]
:   The `REFERENCES` and `FOREIGN KEY` clauses specify referential integrity constraints \(foreign key constraints\). Greenplum accepts referential integrity constraints as specified in PostgreSQL syntax but does not enforce them. See the PostgreSQL documentation for information about referential integrity constraints.

WITH \( storage\_option=value \)
:   The `WITH` clause can be used to set storage options for the table or its indexes. Note that you can also set storage parameters on a particular partition or subpartition by declaring the `WITH` clause in the partition specification. The lowest-level settings have priority.

:   The defaults for some of the table storage options can be specified with the server configuration parameter `gp_default_storage_options`. For information about setting default storage options, see [Notes](#section5).

:   The following storage options are available:

:   **APPENDONLY** — Set to `TRUE` to create the table as an append-optimized table. If `FALSE` or not declared, the table will be created as a regular heap-storage table.

:   **BLOCKSIZE** — Set to the size, in bytes for each block in a table. The `BLOCKSIZE` must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default is 32768.

:   **ORIENTATION** — Set to `column` for column-oriented storage, or `row` \(the default\) for row-oriented storage. This option is only valid if `APPENDONLY=TRUE`. Heap-storage tables can only be row-oriented.

:   **CHECKSUM** — This option is valid only for append-optimized tables \(`APPENDONLY=TRUE`\). The value `TRUE` is the default and enables CRC checksum validation for append-optimized tables. The checksum is calculated during block creation and is stored on disk. Checksum validation is performed during block reads. If the checksum calculated during the read does not match the stored checksum, the transaction is aborted. If you set the value to `FALSE` to disable checksum validation, checking the table data for on-disk corruption will not be performed.

:   **COMPRESSTYPE** — Set to `ZLIB` \(the default\), `RLE-TYPE`, or `QUICKLZ`<sup>1</sup> to specify the type of compression used. The value `NONE`disables compression. QuickLZ uses less CPU power and compresses data faster at a lower compression ratio than zlib. Conversely, zlib provides more compact compression ratios at lower speeds. This option is only valid if `APPENDONLY=TRUE`.

    **Note:** <sup>1</sup>QuickLZ compression is available only in the commercial release of Tanzu Greenplum.

    The value `RLE_TYPE` is supported only if `ORIENTATION` =`column` is specified, Greenplum Database uses the run-length encoding \(RLE\) compression algorithm. RLE compresses data better than the zlib or QuickLZ compression algorithm when the same data value occurs in many consecutive rows.

    For columns of type `BIGINT`, `INTEGER`, `DATE`, `TIME`, or `TIMESTAMP`, delta compression is also applied if the `COMPRESSTYPE` option is set to `RLE-TYPE` compression. The delta compression algorithm is based on the delta between column values in consecutive rows and is designed to improve compression when data is loaded in sorted order or the compression is applied to column data that is in sorted order.

    For information about using table compression, see "Choosing the Table Storage Model" in the *Greenplum Database Administrator Guide*.

:   **COMPRESSLEVEL** — For zlib compression of append-optimized tables, set to an integer value between 1 \(fastest compression\) to 9 \(highest compression ratio\). QuickLZ compression level can only be set to 1. If not declared, the default is 1. For `RLE_TYPE`, the compression level can be set an integer value between 1 \(fastest compression\) to 4 \(highest compression ratio\).

:   This option is valid only if `APPENDONLY=TRUE`.

:   **FILLFACTOR** — See [CREATE INDEX](CREATE_INDEX.html) for more information about this index storage parameter.

:   **OIDS** — Set to `OIDS=FALSE` \(the default\) so that rows do not have object identifiers assigned to them. Greenplum strongly recommends that you do not enable OIDS when creating a table. On large tables, such as those in a typical Greenplum Database system, using OIDs for table rows can cause wrap-around of the 32-bit OID counter. Once the counter wraps around, OIDs can no longer be assumed to be unique, which not only makes them useless to user applications, but can also cause problems in the Greenplum Database system catalog tables. In addition, excluding OIDs from a table reduces the space required to store the table on disk by 4 bytes per row, slightly improving performance. OIDS are not allowed on partitioned tables or append-optimized column-oriented tables.

ON COMMIT
:   The behavior of temporary tables at the end of a transaction block can be controlled using `ON COMMIT`. The three options are:

:   **PRESERVE ROWS** - No special action is taken at the ends of transactions for temporary tables. This is the default behavior.

:   **DELETE ROWS** - All rows in the temporary table will be deleted at the end of each transaction block. Essentially, an automatic `TRUNCATE` is done at each commit.

:   **DROP** - The temporary table will be dropped at the end of the current transaction block.

TABLESPACE tablespace
:   The name of the tablespace in which the new table is to be created. If not specified, the database's default tablespace is used.

USING INDEX TABLESPACE tablespace
:   This clause allows selection of the tablespace in which the index associated with a `UNIQUE` or `PRIMARY KEY` constraint will be created. If not specified, the database's default tablespace is used.

DISTRIBUTED BY \(column, \[ ... \] \)
DISTRIBUTED RANDOMLY
:   Used to declare the Greenplum Database distribution policy for the table. `DISTRIBUTED BY` uses hash distribution with one or more columns declared as the distribution key. For the most even data distribution, the distribution key should be the primary key of the table or a unique column \(or set of columns\). If that is not possible, then you may choose `DISTRIBUTED RANDOMLY`, which will send the data round-robin to the segment instances.

:   The Greenplum Database server configuration parameter `gp_create_table_random_default_distribution` controls the default table distribution policy if the DISTRIBUTED BY clause is not specified when you create a table. Greenplum Database follows these rules to create a table if a distribution policy is not specified.

    If the value of the parameter is `off` \(the default\), Greenplum Database chooses the table distribution key based on the command. If the LIKE or INHERITS clause is specified in table creation command, the created table uses the same distribution key as the source or parent table.

    If the value of the parameter is set to `on`, Greenplum Database follows these rules:

    -   If PRIMARY KEY or UNIQUE columns are not specified, the distribution of the table is random \(DISTRIBUTED RANDOMLY\). Table distribution is random even if the table creation command contains the LIKE or INHERITS clause.
    -   If PRIMARY KEY or UNIQUE columns are specified, a DISTRIBUTED BY clause must also be specified. If a DISTRIBUTED BY clause is not specified as part of the table creation command, the command fails.

:   For information about the parameter, see "Server Configuration Parameters."

PARTITION BY
:   Declares one or more columns by which to partition the table.

:   When creating a partitioned table, Greenplum Database creates the root partitioned table \(the root partition\) with the specified table name. Greenplum Database also creates a hierarchy of tables, child tables, that are the subpartitions based on the partitioning options that you specify. The Greenplum Database *pg\_partition*\* system views contain information about the subpartition tables.

:   For each partition level \(each hierarchy level of tables\), a partitioned table can have a maximum of 32,767 partitions.

:   **Note:** Greenplum Database stores partitioned table data in the leaf child tables, the lowest-level tables in the hierarchy of child tables for use by the partitioned table.

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

:   **`WITH`**— Sets the table storage options for a partition. For example, you may want older partitions to be append-optimized tables and newer partitions to be regular heap tables.

:   **`TABLESPACE`** — The name of the tablespace in which the partition is to be created.

SUBPARTITION BY
:   Declares one or more columns by which to subpartition the first-level partitions of the table. The format of the subpartition specification is similar to that of a partition specification described above.

SUBPARTITION TEMPLATE
:   Instead of declaring each subpartition definition individually for each partition, you can optionally declare a subpartition template to be used to create the subpartitions \(lower level child tables\). This subpartition specification would then apply to all parent partitions.

## Notes 

-   In Greenplum Database \(a Postgres-based system\) the data types `VARCHAR` or `TEXT` handles padding added to the textual data \(space characters added after the last non-space character\) as significant characters, the data type `CHAR` does not.

    In Greenplum Database, values of type `CHAR(n)` are padded with trailing spaces to the specified width n. The values are stored and displayed with the spaces. However, the padding spaces are treated as semantically insignificant. When the values are distributed, the trailing spaces are disregarded. The trailing spaces are also treated as semantically insignificant when comparing two values of data type `CHAR`, and the trailing spaces are removed when converting a character value to one of the other string types.

-   Using OIDs in new applications is not recommended: where possible, using a `SERIAL` or other sequence generator as the table's primary key is preferred. However, if your application does make use of OIDs to identify specific rows of a table, it is recommended to create a unique constraint on the OID column of that table, to ensure that OIDs in the table will indeed uniquely identify rows even after counter wrap-around. Avoid assuming that OIDs are unique across tables; if you need a database-wide unique identifier, use the combination of table OID and row OID for the purpose.
-   Greenplum Database has some special conditions for primary key and unique constraints with regards to columns that are the *distribution key* in a Greenplum table. For a unique constraint to be enforced in Greenplum Database, the table must be hash-distributed \(not `DISTRIBUTED RANDOMLY`\), and the constraint columns must be the same as \(or a superset of\) the table's distribution key columns. Also, the distribution key must be a left-subset of the constraint columns with the columns in the correct order. For example, if the primary key is \(a,b,c\), the distribution key can be only one of the following: \(a\), \(a,b\), or \(a,b,c\).

    A primary key constraint is simply a combination of a unique constraint and a not-null constraint.

    Greenplum Database automatically creates a `UNIQUE` index for each `UNIQUE` or `PRIMARY KEY` constraint to enforce uniqueness. Thus, it is not necessary to create an index explicitly for primary key columns. `UNIQUE` and `PRIMARY KEY` constraints are not allowed on append-optimized tables because the `UNIQUE` indexes that are created by the constraints are not allowed on append-optimized tables.

    Foreign key constraints are not supported in Greenplum Database.

    For inherited tables, unique constraints, primary key constraints, indexes and table privileges are *not* inherited in the current implementation.

-   For append-optimized tables, `UPDATE` and `DELETE` are not allowed in a serializable transaction and will cause the transaction to abort. `CLUSTER`, `DECLARE...FOR``UPDATE`, and triggers are not supported with append-optimized tables.
-   To insert data into a partitioned table, you specify the root partitioned table, the table created with the `CREATE TABLE` command. You also can specify a leaf child table of the partitioned table in an `INSERT` command. An error is returned if the data is not valid for the specified leaf child table. Specifying a child table that is not a leaf child table in the `INSERT` command is not supported. Execution of other DML commands such as `UPDATE` and `DELETE` on any child table of a partitioned table is not supported. These commands must be executed on the root partitioned table, the table created with the `CREATE TABLE` command.
-   The default values for these table storage options can be specified with the server configuration parameter `gp_default_storage_option`.

    -   APPENDONLY
    -   BLOCKSIZE
    -   CHECKSUM
    -   COMPRESSTYPE
    -   COMPRESSLEVEL
    -   ORIENTATION
    The defaults can be set for the system, a database, or a user. For information about setting storage options, see the server configuration parameter [gp\_default\_storage\_options](../config_params/guc-list.html).


**Important:** The current Greenplum Database legacy optimizer allows list partitions with multi-column \(composite\) partition keys. GPORCA does not support composite keys, so using composite partition keys is not recommended.

## Examples 

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
WITH (appendonly=true, compresslevel=5) 
DISTRIBUTED BY (txn_id);
```

Create a three level partitioned table using subpartition templates and default partitions at each level:

```
CREATE TABLE sales (id int, year int, month int, day int, 
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)

  SUBPARTITION BY RANGE (month)
    SUBPARTITION TEMPLATE (
       START (1) END (13) EVERY (1), 
       DEFAULT SUBPARTITION other_months )

  SUBPARTITION BY LIST (region)
    SUBPARTITION TEMPLATE (
       SUBPARTITION usa VALUES ('usa'),
       SUBPARTITION europe VALUES ('europe'),
       SUBPARTITION asia VALUES ('asia'),
       DEFAULT SUBPARTITION other_regions)

( START (2008) END (2016) EVERY (1),
  DEFAULT PARTITION outlying_years);
```

## Compatibility 

`CREATE TABLE` command conforms to the SQL standard, with the following exceptions:

-   **Temporary Tables** — In the SQL standard, temporary tables are defined just once and automatically exist \(starting with empty contents\) in every session that needs them. Greenplum Database instead requires each session to issue its own `CREATE TEMPORARY TABLE` command for each temporary table to be used. This allows different sessions to use the same temporary table name for different purposes, whereas the standard's approach constrains all instances of a given temporary table name to have the same table structure.

    The standard's distinction between global and local temporary tables is not in Greenplum Database. Greenplum Database will accept the `GLOBAL` and `LOCAL` keywords in a temporary table declaration, but they have no effect.

    If the `ON COMMIT` clause is omitted, the SQL standard specifies that the default behavior as `ON COMMIT DELETE ROWS`. However, the default behavior in Greenplum Database is `ON COMMIT PRESERVE ROWS`. The `ON COMMIT DROP` option does not exist in the SQL standard.

-   **Column Check Constraints** — The SQL standard says that `CHECK` column constraints may only refer to the column they apply to; only `CHECK` table constraints may refer to multiple columns. Greenplum Database does not enforce this restriction; it treats column and table check constraints alike.
-   **NULL Constraint** — The `NULL` constraint is a Greenplum Database extension to the SQL standard that is included for compatibility with some other database systems \(and for symmetry with the `NOT NULL` constraint\). Since it is the default for any column, its presence is not required.
-   **Inheritance** — Multiple inheritance via the `INHERITS` clause is a Greenplum Database language extension. SQL:1999 and later define single inheritance using a different syntax and different semantics. SQL:1999-style inheritance is not yet supported by Greenplum Database.
-   **Partitioning** — Table partitioning via the `PARTITION BY` clause is a Greenplum Database language extension.
-   **Zero-column tables** — Greenplum Database allows a table of no columns to be created \(for example, `CREATE TABLE foo();`\). This is an extension from the SQL standard, which does not allow zero-column tables. Zero-column tables are not in themselves very useful, but disallowing them creates odd special cases for `ALTER TABLE DROP COLUMN`, so Greenplum decided to ignore this spec restriction.
-   **WITH clause** — The `WITH` clause is a Greenplum Database extension; neither storage parameters nor OIDs are in the standard.
-   **Tablespaces** — The Greenplum Database concept of tablespaces is not part of the SQL standard. The clauses `TABLESPACE` and `USING INDEX TABLESPACE` are extensions.
-   **Data Distribution** — The Greenplum Database concept of a parallel or distributed database is not part of the SQL standard. The `DISTRIBUTED` clauses are extensions.

## See Also 

[ALTER TABLE](ALTER_TABLE.html), [DROP TABLE](DROP_TABLE.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE TABLE AS](CREATE_TABLE_AS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

