# ALTER TABLE 

Changes the definition of a table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER TABLE [IF EXISTS] [ONLY] <name> 
    <action> [, ... ]

ALTER TABLE [IF EXISTS] [ONLY] <name> 
    RENAME [COLUMN] <column_name> TO <new_column_name>

ALTER TABLE [ IF EXISTS ] [ ONLY ] <name> 
    RENAME CONSTRAINT <constraint_name> TO <new_constraint_name>

ALTER TABLE [IF EXISTS] <name> 
    RENAME TO <new_name>

ALTER TABLE [IF EXISTS] <name> 
    SET SCHEMA <new_schema>

ALTER TABLE ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
    SET TABLESPACE <new_tablespace> [ NOWAIT ]

ALTER TABLE [IF EXISTS] [ONLY] <name> SET
   [ WITH (reorganize=true|false) ]
     DISTRIBUTED BY ({<column_name> [<opclass>]} [, ... ] )
   | DISTRIBUTED RANDOMLY
   | DISTRIBUTED REPLICATED 

ALTER TABLE <name>
   [ ALTER PARTITION { <partition_name> | FOR (RANK(<number>)) 
   | FOR (<value>) } [...] ] <partition_action>

where <action> is one of:
                        
  ADD [COLUMN] <column_name data_type> [ DEFAULT <default_expr> ]
      [<column_constraint> [ ... ]]
      [ COLLATE <collation> ]
      [ ENCODING ( <storage_directive [,...] ) ]
  DROP [COLUMN] [IF EXISTS] <column_name> [RESTRICT | CASCADE]
  ALTER [COLUMN] <column_name> [ SET DATA ] TYPE <type> [COLLATE <collation>] [USING <expression>]
  ALTER [COLUMN] <column_name> SET DEFAULT <expression>
  ALTER [COLUMN] <column_name> DROP DEFAULT
  ALTER [COLUMN] <column_name> { SET | DROP } NOT NULL
  ALTER [COLUMN] <column_name> SET STATISTICS <integer>
  ALTER [COLUMN] column SET ( <attribute_option> = <value> [, ... ] )
  ALTER [COLUMN] column RESET ( <attribute_option> [, ... ] )
  ALTER [COLUMN] column SET ENCODNG ( storage_directive [, ...] )
  ADD <table_constraint> [NOT VALID]
  ADD <table_constraint_using_index>
  VALIDATE CONSTRAINT <constraint_name>
  DROP CONSTRAINT [IF EXISTS] <constraint_name> [RESTRICT | CASCADE]
  DISABLE TRIGGER [<trigger_name> | ALL | USER]
  ENABLE TRIGGER [<trigger_name> | ALL | USER]
  CLUSTER ON <index_name>
  SET WITHOUT CLUSTER
  SET WITHOUT OIDS
  SET ACCESS METHOD <access_method>  WITH ( <storage_parameter> = <value> )
  SET (<storage_parameter> = <value>)
  RESET (<storage_parameter> [, ... ])
  SET  WITH (<storage_parameter> = <value>)
  INHERIT <parent_table>
  NO INHERIT <parent_table>
  OF `type_name`
  NOT OF
  OWNER TO <new_owner>
  SET TABLESPACE <new_tablespace>
```

where table\_constraint\_using\_index is:

```
  [ CONSTRAINT constraint\_name ]
  { UNIQUE | PRIMARY KEY } USING INDEX index\_name
  [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]
```

where partition\_action is one of:

```
  ALTER DEFAULT PARTITION
  DROP DEFAULT PARTITION [IF EXISTS]
  DROP PARTITION [IF EXISTS] { <partition_name> | 
      FOR (RANK(<number>)) | FOR (<value>) } [CASCADE]
  TRUNCATE DEFAULT PARTITION
  TRUNCATE PARTITION { <partition_name> | FOR (RANK(<number>)) | 
      FOR (<value>) }
  RENAME DEFAULT PARTITION TO <new_partition_name>
  RENAME PARTITION { <partition_name> | FOR (RANK(<number>)) | 
      FOR (<value>) } TO <new_partition_name>
  ADD DEFAULT PARTITION <name> [ ( <subpartition_spec> ) ]
  ADD PARTITION [<partition_name>] <partition_element>
     [ ( <subpartition_spec> ) ]
  EXCHANGE PARTITION { <partition_name> | FOR (RANK(<number>)) | 
       FOR (<value>) } WITH TABLE <table_name>
        [ WITH | WITHOUT VALIDATION ]
  EXCHANGE DEFAULT PARTITION WITH TABLE <table_name>
   [ WITH | WITHOUT VALIDATION ]
  SET SUBPARTITION TEMPLATE (<subpartition_spec>)
  SPLIT DEFAULT PARTITION
    {  AT (<list_value>)
     | START([<datatype>] <range_value>) [INCLUSIVE | EXCLUSIVE] 
        END([<datatype>] <range_value>) [INCLUSIVE | EXCLUSIVE] }
    [ INTO ( PARTITION <new_partition_name>, 
             PARTITION <default_partition_name> ) ]
  SPLIT PARTITION { <partition_name> | FOR (RANK(<number>)) | 
     FOR (<value>) } AT (<value>) 
    [ INTO (PARTITION <partition_name>, PARTITION <partition_name>)]  
```

where partition\_element is:

```
    VALUES (<list_value> [,...] )
  | START ([<datatype>] '<start_value>') [INCLUSIVE | EXCLUSIVE]
     [ END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE] ]
  | END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
[ WITH ( <storage_parameter> = value> [ , ... ] ) ]
[ TABLESPACE <tablespace> ]
```

where subpartition\_spec is:

```
<subpartition_element> [, ...]
```

and subpartition\_element is:

```
   DEFAULT SUBPARTITION <subpartition_name>
  | [SUBPARTITION <subpartition_name>] VALUES (<list_value> [,...] )
  | [SUBPARTITION <subpartition_name>] 
     START ([<datatype>] '<start_value>') [INCLUSIVE | EXCLUSIVE]
     [ END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE] ]
     [ EVERY ( [<number | datatype>] '<interval_value>') ]
  | [SUBPARTITION <subpartition_name>] 
     END ([<datatype>] '<end_value>') [INCLUSIVE | EXCLUSIVE]
     [ EVERY ( [<number | datatype>] '<interval_value>') ]
[ WITH ( <storage_parameter>=<value> [, ... ] ) ]
[ TABLESPACE <tablespace> ]
```

where storage_directive is:

```
   blocksize={8192-2097152}
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
   compresslevel={0-9}
``` 

where storage\_parameter when used with the `SET` command is:

```
   blocksize={8192-2097152}
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
   compresslevel={0-9}
   fillfactor={10-100}
   checksum= {true | false }
   analyze_hll_non_part_table={true | false }
```

where storage\_parameter when used with the `SET WITH` command is:

```
   appendoptimized={true | false }
   blocksize={8192-2097152}
   orientation={COLUMN|ROW}
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
   compresslevel={0-9}
   fillfactor={10-100}
   checksum={true | false }
   reorganize={true | false }
   vacuum_index_cleanup { true | false } 
```

  <p class="note">
<strong>Note:</strong>
Although you can specify the table's access method using the <code>appendoptimized</code> storage parameter, VMware recommends that you use <code>SET ACCESS METHOD &lt;access method></code> instead.
</p>

## <a id="section3"></a>Description 

`ALTER TABLE` changes the definition of an existing table. There are several subforms:

-   **ADD COLUMN** — Adds a new column to the table, using the same syntax as [CREATE TABLE](CREATE_TABLE.html). The `ENCODING` clause is valid only for append-optimized, column-oriented tables.

    When you add a column to an append-optimized, column-oriented table, Greenplum Database sets each data compression parameter for the column \(`compresstype`, `compresslevel`, and `blocksize`\) based on the following setting, in order of preference.

    1.  The compression parameter setting specified in the `ALTER TABLE` command `ENCODING` clause.
    2.  The table's data compression setting specified in the `WITH` clause when the table was created.
    3.  The compression parameter setting specified in the server configuration parameter [gp\_default\_storage\_option](../config_params/guc-list.html).
    4.  The default compression parameter value.
    For append-optimized and hash tables, `ADD COLUMN` requires a table rewrite. For information about table rewrites performed by `ALTER TABLE`, see [Notes](#section5).

-   **DROP COLUMN \[IF EXISTS\]** — Drops a column from a table. Note that if you drop table columns that are being used as the Greenplum Database distribution key, the distribution policy for the table will be changed to `DISTRIBUTED RANDOMLY`. Indexes and table constraints involving the column are automatically dropped as well. You need to say `CASCADE` if anything outside the table depends on the column \(such as views\). If `IF EXISTS` is specified and the column does not exist, no error is thrown; a notice is issued instead.
-   **IF EXISTS** — Do not throw an error if the table does not exist. A notice is issued in this case.
-   **SET DATA TYPE** — This form changes the data type of a column of a table. Note that you cannot alter column data types that are being used as distribution or partitioning keys. Indexes and simple table constraints involving the column will be automatically converted to use the new column type by reparsing the originally supplied expression. The optional `COLLATE` clause specifies a collation for the new column; if omitted, the collation is the default for the new column type. The optional `USING` clause specifies how to compute the new column value from the old. If omitted, the default conversion is the same as an assignment cast from old data type to new. A `USING` clause must be provided if there is no implicit or assignment cast from old to new type.

    > **Note** GPORCA supports collation only when all columns in the query use the same collation. If columns in the query use different collations, then Greenplum uses the Postgres Planner.

    Changing a column data type requires a table rewrite. For information about table rewrites performed by `ALTER TABLE`, see [Notes](#section5).

-   **SET/DROP DEFAULT** — Sets or removes the default value for a column. Default values only apply in subsequent `INSERT` or `UPDATE` commands; they do not cause rows already in the table to change.
-   **SET/DROP NOT NULL** — Changes whether a column is marked to allow null values or to reject null values. You can only use `SET NOT NULL` when the column contains no null values.
-   **SET STATISTICS** — Sets the per-column statistics-gathering target for subsequent `ANALYZE` operations. The target can be set in the range 0 to 10000, or set to -1 to revert to using the system default statistics target \(`default_statistics_target`\). When set to 0, no statistics are collected.
-   **SET \( attribute\_option = value \[, ... \]\)**

    **RESET \( attribute\_option \[, ...\] \)**— Sets or resets per-attribute options. Currently, the only defined per-attribute options are `n_distinct` and `n_distinct_inherited`, which override the number-of-distinct-values estimates made by subsequent [ANALYZE](ANALYZE.html) operations. `n_distinct` affects the statistics for the table itself, while `n_distinct_inherited` affects the statistics gathered for the table plus its inheritance children. When set to a positive value, `ANALYZE` will assume that the column contains exactly the specified number of distinct non-null values. When set to a negative value, which must be greater than or equal to -1, `ANALYZE` will assume that the number of distinct non-null values in the column is linear in the size of the table; the exact count is to be computed by multiplying the estimated table size by the absolute value of the given number. For example, a value of -1 implies that all values in the column are distinct, while a value of -0.5 implies that each value appears twice on the average. This can be useful when the size of the table changes over time, since the multiplication by the number of rows in the table is not performed until query planning time. Specify a value of 0 to revert to estimating the number of distinct values normally.

-   **ADD table\_constraint \[NOT VALID\]** — Adds a new constraint to a table \(not just a partition\) using the same syntax as `CREATE TABLE`. The `NOT VALID` option is currently only allowed for foreign key and `CHECK` constraints. If the constraint is marked `NOT VALID`, Greenplum Database skips the potentially-lengthy initial check to verify that all rows in the table satisfy the constraint. The constraint will still be enforced against subsequent inserts or updates \(that is, they'll fail unless there is a matching row in the referenced table, in the case of foreign keys; and they'll fail unless the new row matches the specified check constraints\). But the database will not assume that the constraint holds for all rows in the table, until it is validated by using the `VALIDATE CONSTRAINT` option. Constraint checks are skipped at create table time, so the [CREATE TABLE](CREATE_TABLE.html) syntax does not include this option.
-   **VALIDATE CONSTRAINT** — This form validates a foreign key constraint that was previously created as `NOT VALID`, by scanning the table to ensure there are no rows for which the constraint is not satisfied. Nothing happens if the constraint is already marked valid. The advantage of separating validation from initial creation of the constraint is that validation requires a lesser lock on the table than constraint creation does.
-   **ADD table\_constraint\_using\_index** — Adds a new `PRIMARY KEY` or `UNIQUE` constraint to a table based on an existing unique index. All the columns of the index will be included in the constraint. The index cannot have expression columns nor be a partial index. Also, it must be a b-tree index with default sort ordering. These restrictions ensure that the index is equivalent to one that would be built by a regular `ADD PRIMARY KEY` or `ADD UNIQUE` command.

    Adding a `PRIMARY KEY` or `UNIQUE` constraint to a table based on an existing unique index is not supported on a partitioned table.

    If `PRIMARY KEY` is specified, and the index's columns are not already marked `NOT NULL`, then this command will attempt to do `ALTER COLUMN SET NOT NULL` against each such column. That requires a full table scan to verify the column\(s\) contain no nulls. In all other cases, this is a fast operation.

    If a constraint name is provided then the index will be renamed to match the constraint name. Otherwise the constraint will be named the same as the index.

    After this command is run, the index is "owned" by the constraint, in the same way as if the index had been built by a regular `ADD PRIMARY KEY` or `ADD UNIQUE` command. In particular, dropping the constraint will make the index disappear too.

-   **DROP CONSTRAINT \[IF EXISTS\]** — Drops the specified constraint on a table. If `IF EXISTS` is specified and the constraint does not exist, no error is thrown. In this case a notice is issued instead.
-   **DISABLE/ENABLE TRIGGER** — Deactivates or activates trigger\(s\) belonging to the table. A deactivated trigger is still known to the system, but is not run when its triggering event occurs. For a deferred trigger, the enable status is checked when the event occurs, not when the trigger function is actually run. One may deactivate or activate a single trigger specified by name, or all triggers on the table, or only user-created triggers. Deactivating or activating constraint triggers requires superuser privileges.

    > **Note** triggers are not supported in Greenplum Database. Triggers in general have very limited functionality due to the parallelism of Greenplum Database.

-   **CLUSTER ON/SET WITHOUT CLUSTER** — Selects or removes the default index for future `CLUSTER` operations. It does not actually re-cluster the table.

-   **SET WITHOUT OIDS** — Removes the OID system column from the table.

    > **Caution** VMware does not support using `SET WITH OIDS` or `oids=TRUE` to assign an OID system column.On large tables, such as those in a typical Greenplum Database system, using OIDs for table rows can cause wrap-around of the 32-bit OID counter. Once the counter wraps around, OIDs can no longer be assumed to be unique, which not only makes them useless to user applications, but can also cause problems in the Greenplum Database system catalog tables. In addition, excluding OIDs from a table reduces the space required to store the table on disk by 4 bytes per row, slightly improving performance. You cannot create OIDS on a partitioned or column-oriented table \(an error is displayed\). This syntax is deprecated and will be removed in a future Greenplum release.

-   **SET \( FILLFACTOR = value\) / RESET \(FILLFACTOR\)** — Changes the fillfactor for the table. The fillfactor for a table is a percentage between 10 and 100. 100 \(complete packing\) is the default. When a smaller fillfactor is specified, `INSERT` operations pack table pages only to the indicated percentage; the remaining space on each page is reserved for updating rows on that page. This gives `UPDATE` a chance to place the updated copy of a row on the same page as the original, which is more efficient than placing it on a different page. For a table whose entries are never updated, complete packing is the best choice, but in heavily updated tables smaller fillfactors are appropriate. Note that the table contents will not be modified immediately by this command. You will need to rewrite the table to get the desired effects. That can be done with [VACUUM](VACUUM.html) or one of the forms of `ALTER TABLE` that forces a table rewrite. For information about the forms of `ALTER TABLE` that perform a table rewrite, see [Notes](#section5).
-   **SET DISTRIBUTED** — Changes the distribution policy of a table. Changing a hash distribution policy, or changing to or from a replicated policy, will cause the table data to be physically redistributed on disk, which can be resource intensive. *While Greenplum Database permits changing the distribution policy of a writable external table, the operation never results in physical redistribution of the external data.*
-   **INHERIT parent\_table / NO INHERIT parent\_table** — Adds or removes the target table as a child of the specified parent table. Queries against the parent will include records of its child table. To be added as a child, the target table must already contain all the same columns as the parent \(it could have additional columns, too\). The columns must have matching data types, and if they have `NOT NULL` constraints in the parent then they must also have `NOT NULL` constraints in the child. There must also be matching child-table constraints for all `CHECK` constraints of the parent, except those marked non-inheritable \(that is, created with `ALTER TABLE ... ADD CONSTRAINT ... NO INHERIT`\) in the parent, which are ignored; all child-table constraints matched must not be marked non-inheritable. Currently `UNIQUE`, `PRIMARY KEY`, and `FOREIGN KEY` constraints are not considered, but this may change in the future.
-   OF type\_name — This form links the table to a composite type as though `CREATE TABLE OF` had formed it. The table's list of column names and types must precisely match that of the composite type; the presence of an `oid` system column is permitted to differ. The table must not inherit from any other table. These restrictions ensure that `CREATE TABLE OF` would permit an equivalent table definition.
-   **NOT OF** — This form dissociates a typed table from its type.
-   **OWNER** — Changes the owner of the table, sequence, or view to the specified user.
-   **SET TABLESPACE** — Changes the table's tablespace to the specified tablespace and moves the data file\(s\) associated with the table to the new tablespace. Indexes on the table, if any, are not moved; but they can be moved separately with additional `SET TABLESPACE` commands. All tables in the current database in a tablespace can be moved by using the `ALL IN TABLESPACE` form, which will lock all tables to be moved first and then move each one. This form also supports `OWNED BY`, which will only move tables owned by the roles specified. If the `NOWAIT` option is specified then the command will fail if it is unable to acquire all of the locks required immediately. Note that system catalogs are not moved by this command, use `ALTER DATABASE` or explicit `ALTER TABLE` invocations instead if desired. The `information_schema` relations are not considered part of the system catalogs and will be moved. See also `CREATE TABLESPACE`. If changing the tablespace of a partitioned table, all child table partitions will also be moved to the new tablespace.
-   **RENAME** — Changes the name of a table \(or an index, sequence, view, or materialized view\), the name of an individual column in a table, or the name of a constraint of the table. There is no effect on the stored data. Note that Greenplum Database distribution key columns cannot be renamed.
-   **SET SCHEMA** — Moves the table into another schema. Associated indexes, constraints, and sequences owned by table columns are moved as well.
-   **ALTER PARTITION \| DROP PARTITION \| RENAME PARTITION \| TRUNCATE PARTITION \| ADD PARTITION \| SPLIT PARTITION \| EXCHANGE PARTITION \| SET SUBPARTITION TEMPLATE**— Changes the structure of a partitioned table. In most cases, you must go through the parent table to alter one of its child table partitions.

> **Note** If you add a partition to a table that has subpartition encodings, the new partition inherits the storage directives for the subpartitions. For more information about the precedence of compression settings, see [Using Compression](../../admin_guide/ddl/ddl-storage.html#topic40).

All the forms of `ALTER TABLE` that act on a single table, except `RENAME` and `SET SCHEMA`, can be combined into a list of multiple alterations to apply together. For example, it is possible to add several columns and/or alter the type of several columns in a single command. This is particularly useful with large tables, since only one pass over the table need be made.

You must own the table to use `ALTER TABLE`. To change the schema or tablespace of a table, you must also have `CREATE` privilege on the new schema or tablespace. To add the table as a new child of a parent table, you must own the parent table as well. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the table's schema. To add a column or alter a column type or use the `OF` clause, you must also have `USAGE` privilege on the data type. A superuser has these privileges automatically.

> **Note** Memory usage increases significantly when a table has many partitions, if a table has compression, or if the blocksize for a table is large. If the number of relations associated with the table is large, this condition can force an operation on the table to use more memory. For example, if the table is a CO table and has a large number of columns, each column is a relation. An operation like `ALTER TABLE ALTER COLUMN` opens all the columns in the table allocates associated buffers. If a CO table has 40 columns and 100 partitions, and the columns are compressed and the blocksize is 2 MB \(with a system factor of 3\), the system attempts to allocate 24 GB, that is \(40 ×100\) × \(2 ×3\) MB or 24 GB.

## <a id="section4"></a>Parameters 

access method
:   The method to use for accessing the table. Set to `heap` to access the table as a heap-storage table, `ao_row` to access the table as an append-optimized table with row-oriented storage (AO), or `ao_column` to access the table as an append-optimized table with column-oriented storage (AOCO).

  <p class="note">
<strong>Note:</strong>
Although you can specify the table's access method using <code>SET &lt;storage_parameter></code>, VMware recommends that you use <code>SET ACCESS METHOD &lt;access_method></code> instead.
</p>

ONLY
:   Only perform the operation on the table name specified. If the `ONLY` keyword is not used, the operation will be performed on the named table and any child table partitions associated with that table.

    > **Note** Adding or dropping a column, or changing a column's type, in a parent or descendant table only is not permitted. The parent table and its descendents must always have the same columns and types.

name
:   The name \(possibly schema-qualified\) of an existing table to alter. If `ONLY` is specified, only that table is altered. If `ONLY` is not specified, the table and all its descendant tables \(if any\) are updated.

    > **Note** Constraints can only be added to an entire table, not to a partition. Because of that restriction, the name parameter can only contain a table name, not a partition name.

column\_name
:   Name of a new or existing column. Note that Greenplum Database distribution key columns must be treated with special care. Altering or dropping these columns can change the distribution policy for the table.

new\_column\_name
:   New name for an existing column.

new\_name
:   New name for the table.

type
:   Data type of the new column, or new data type for an existing column. If changing the data type of a Greenplum distribution key column, you are only allowed to change it to a compatible type \(for example, `text` to `varchar` is OK, but `text` to `int` is not\).

table\_constraint
:   New table constraint for the table. Note that foreign key constraints are currently not supported in Greenplum Database. Also a table is only allowed one unique constraint and the uniqueness must be within the Greenplum Database distribution key.

constraint\_name
:   Name of an existing constraint to drop.

CASCADE
:   Automatically drop objects that depend on the dropped column or constraint \(for example, views referencing the column\).

RESTRICT
:   Refuse to drop the column or constraint if there are any dependent objects. This is the default behavior.

trigger\_name
:   Name of a single trigger to deactivate or enable. Note that Greenplum Database does not support triggers.

ALL
:   Deactivate or activate all triggers belonging to the table including constraint related triggers. This requires superuser privilege if any of the triggers are internally generated constraint triggers such as those that are used to implement foreign key constraints or deferrable uniqueness and exclusion constraints.

USER
:   Deactivate or activate all triggers belonging to the table except for internally generated constraint triggers such as those that are used to implement foreign key constraints or deferrable uniqueness and exclusion constraints.

index\_name
:   The index name on which the table should be marked for clustering.

FILLFACTOR
:   Set the fillfactor percentage for a table.

value
:   The new value for the `FILLFACTOR` parameter, which is a percentage between 10 and 100. 100 is the default.

DISTRIBUTED BY \(\{column\_name \[opclass\]\}\) \| DISTRIBUTED RANDOMLY \| DISTRIBUTED REPLICATED
:   Specifies the distribution policy for a table. Changing a hash distribution policy causes the table data to be physically redistributed, which can be resource intensive. If you declare the same hash distribution policy or change from hash to random distribution, data will not be redistributed unless you declare `SET WITH (reorganize=true)`.
:   Changing to or from a replicated distribution policy always causes the table data to be redistributed.

analyze_hll_non_part_table=true|false
:   Use `analyze_hll_non_part_table=true` to force collection of HLL statistics even if the table is not part of a partitioned table. The default is `false`.

reorganize=true\|false
:   Use `reorganize=true` when the hash distribution policy has not changed or when you have changed from a hash to a random distribution, and you want to redistribute the data anyway.
:   If you are setting the distribution policy, you must specify the `WITH (reorganize=<value>)` clause before the `DISTRIBUTED ...` clause.   
:   Any attempt to reorganize an external table fails with an error.

parent\_table
:   A parent table to associate or de-associate with this table.

new\_owner
:   The role name of the new owner of the table.

new\_tablespace
:   The name of the tablespace to which the table will be moved.

new\_schema
:   The name of the schema to which the table will be moved.

parent\_table\_name
:   When altering a partitioned table, the name of the top-level parent table.

ALTER \[DEFAULT\] PARTITION
:   If altering a partition deeper than the first level of partitions, use `ALTER PARTITION` clauses to specify which subpartition in the hierarchy you want to alter. For each partition level in the table hierarchy that is above the target partition, specify the partition that is related to the target partition in an `ALTER PARTITION` clause.

DROP \[DEFAULT\] PARTITION
:   Drops the specified partition. If the partition has subpartitions, the subpartitions are automatically dropped as well.

TRUNCATE \[DEFAULT\] PARTITION
:   Truncates the specified partition. If the partition has subpartitions, the subpartitions are automatically truncated as well.

RENAME \[DEFAULT\] PARTITION
:   Changes the partition name of a partition \(not the relation name\). Partitioned tables are created using the naming convention: `<`parentname`>_<`level`>_prt_<`partition\_name`>`.

ADD DEFAULT PARTITION
:   Adds a default partition to an existing partition design. When data does not match to an existing partition, it is inserted into the default partition. Partition designs that do not have a default partition will reject incoming rows that do not match to an existing partition. Default partitions must be given a name.

ADD PARTITION
:   partition\_element - Using the existing partition type of the table \(range or list\), defines the boundaries of new partition you are adding.

:   name - A name for this new partition.

:   **VALUES** - For list partitions, defines the value\(s\) that the partition will contain.

:   **START** - For range partitions, defines the starting range value for the partition. By default, start values are `INCLUSIVE`. For example, if you declared a start date of '`2016-01-01`', then the partition would contain all dates greater than or equal to '`2016-01-01`'. Typically the data type of the `START` expression is the same type as the partition key column. If that is not the case, then you must explicitly cast to the intended data type.

:   **END** - For range partitions, defines the ending range value for the partition. By default, end values are `EXCLUSIVE`. For example, if you declared an end date of '`2016-02-01`', then the partition would contain all dates less than but not equal to '`2016-02-01`'. Typically the data type of the `END` expression is the same type as the partition key column. If that is not the case, then you must explicitly cast to the intended data type.

:   **WITH** - Sets the table storage options for a partition. For example, you may want older partitions to be append-optimized tables and newer partitions to be regular heap tables. See [CREATE TABLE](CREATE_TABLE.html) for a description of the storage options.

:   **TABLESPACE** - The name of the tablespace in which the partition is to be created.

:   subpartition\_spec - Only allowed on partition designs that were created without a subpartition template. Declares a subpartition specification for the new partition you are adding. If the partitioned table was originally defined using a subpartition template, then the template will be used to generate the subpartitions automatically.

EXCHANGE \[DEFAULT\] PARTITION
:   Exchanges another table into the partition hierarchy into the place of an existing partition. In a multi-level partition design, you can only exchange the lowest level partitions \(those that contain data\).

:   **WITH TABLE** table\_name - The name of the table you are swapping into the partition design. You can exchange a table where the table data is stored in the database. For example, the table is created with the `CREATE TABLE` command. The table must have the same number of columns, column order, column names, column types, and distribution policy as the parent table.

:   With the `EXCHANGE PARTITION` clause, you can also exchange a readable external table \(created with the `CREATE EXTERNAL TABLE` command\) into the partition hierarchy in the place of an existing leaf child partition. If you specify a readable external table, you must also specify the `WITHOUT VALIDATION` clause to skip table validation against the `CHECK` constraint of the partition you are exchanging.

:   Exchanging a leaf child partition with an external table is not supported if the partitioned table contains a column with a check constraint or a `NOT NULL` constraint.

:   You cannot exchange a partition with a replicated table. Exchanging a partition with a partitioned table or a child partition of a partitioned table is not supported.

:   **WITH** \| **WITHOUT VALIDATION** - Validates that the data in the table matches the `CHECK` constraint of the partition you are exchanging. The default is to validate the data against the `CHECK` constraint.

    > **Caution** If you specify the `WITHOUT VALIDATION` clause, you must ensure that the data in table that you are exchanging for an existing child leaf partition is valid against the `CHECK` constraints on the partition. Otherwise, queries against the partitioned table might return incorrect results.

SET SUBPARTITION TEMPLATE
:   Modifies the subpartition template for an existing partition. After a new subpartition template is set, all new partitions added will have the new subpartition design \(existing partitions are not modified\).

SPLIT DEFAULT PARTITION
:   Splits a default partition. In a multi-level partition, only a range partition can be split, not a list partition, and you can only split the lowest level default partitions \(those that contain data\). Splitting a default partition creates a new partition containing the values specified and leaves the default partition containing any values that do not match to an existing partition.

:   **AT** - For list partitioned tables, specifies a single list value that should be used as the criteria for the split.

:   **START** - For range partitioned tables, specifies a starting value for the new partition.

:   **END** - For range partitioned tables, specifies an ending value for the new partition.

:   **INTO** - Allows you to specify a name for the new partition. When using the `INTO` clause to split a default partition, the second partition name specified should always be that of the existing default partition. If you do not know the name of the default partition, you can look it up using the pg\_partitions view.

SPLIT PARTITION
:   Splits an existing partition into two partitions. In a multi-level partition, only a range partition can be split, not a list partition, and you can only split the lowest level partitions \(those that contain data\).

:   **AT** - Specifies a single value that should be used as the criteria for the split. The partition will be divided into two new partitions with the split value specified being the starting range for the latter partition.

:   **INTO** - Allows you to specify names for the two new partitions created by the split.

partition\_name
:   The given name of a partition. The given partition name is the `partitionname` column value in the *[pg\_partitions](../system_catalogs/pg_partitions.html)* system view.

FOR \(RANK\(number\)\)
:   For range partitions, the rank of the partition in the range.

FOR \('value'\)
:   Specifies a partition by declaring a value that falls within the partition boundary specification. If the value declared with `FOR` matches to both a partition and one of its subpartitions \(for example, if the value is a date and the table is partitioned by month and then by day\), then `FOR` will operate on the first level where a match is found \(for example, the monthly partition\). If your intent is to operate on a subpartition, you must declare so as follows: `ALTER TABLE name ALTER PARTITION FOR ('2016-10-01') DROP PARTITION FOR ('2016-10-01');`

## <a id="section5"></a>Notes 

The table name specified in the `ALTER TABLE` command cannot be the name of a partition within a table.

Take special care when altering or dropping columns that are part of the Greenplum Database distribution key as this can change the distribution policy for the table.

Greenplum Database does not currently support foreign key constraints. For a unique constraint to be enforced in Greenplum Database, the table must be hash-distributed \(not `DISTRIBUTED RANDOMLY`\), and all of the distribution key columns must be the same as the initial columns of the unique constraint columns.

Adding a `CHECK` or `NOT NULL` constraint requires scanning the table to verify that existing rows meet the constraint, but does not require a table rewrite.

This table lists the `ALTER TABLE` operations that require a table rewrite when performed on tables defined with the specified type of table storage.

|Operation \(See Note\)|Append-Optimized, Column-Oriented|Append-Optimized|Heap|
|----------------------|---------------------------------|----------------|----|
|`ALTER COLUMN TYPE`|Yes|Yes|Yes|
|`ADD COLUMN`|No|Yes|Yes|
| `ALTER COLUMN SET ENCODING`|Yes|N/A|N/A|

> **Note** Dropping a system `oid` column also requires a table rewrite.

When a column is added with `ADD COLUMN`, all existing rows in the table are initialized with the column's default value, or `NULL` if no `DEFAULT` clause is specified. Adding a column with a non-null default or changing the type of an existing column will require the entire table and indexes to be rewritten. As an exception, if the `USING` clause does not change the column contents and the old type is either binary coercible to the new type or an unconstrained domain over the new type, a table rewrite is not needed, but any indexes on the affected columns must still be rebuilt. Table and/or index rebuilds may take a significant amount of time for a large table; and will temporarily require as much as double the disk space.

> **Important** The forms of `ALTER TABLE` that perform a table rewrite on an append-optimized table are not MVCC-safe. After a table rewrite, the table will appear empty to concurrent transactions if they are using a snapshot taken before the rewrite occurred. See [MVCC Caveats](https://www.postgresql.org/docs/12/mvcc-caveats.html) for more details.

You can specify multiple changes in a single `ALTER TABLE` command, which will be done in a single pass over the table.

The `DROP COLUMN` form does not physically remove the column, but simply makes it invisible to SQL operations. Subsequent insert and update operations in the table will store a null value for the column. Thus, dropping a column is quick but it will not immediately reduce the on-disk size of your table, as the space occupied by the dropped column is not reclaimed. The space will be reclaimed over time as existing rows are updated. If you drop the system `oid` column, however, the table is rewritten immediately.

To force immediate reclamation of space occupied by a dropped column, you can run one of the forms of `ALTER TABLE` that performs a rewrite of the whole table. This results in reconstructing each row with the dropped column replaced by a null value.

The `USING` option of `SET DATA TYPE` can actually specify any expression involving the old values of the row; that is, it can refer to other columns as well as the one being converted. This allows very general conversions to be done with the `SET DATA TYPE` syntax. Because of this flexibility, the `USING` expression is not applied to the column's default value \(if any\); the result might not be a constant expression as required for a default. This means that when there is no implicit or assignment cast from old to new type, `SET DATA TYPE` might fail to convert the default even though a `USING` clause is supplied. In such cases, drop the default with `DROP DEFAULT`, perform the `ALTER TYPE`, and then use `SET DEFAULT` to add a suitable new default. Similar considerations apply to indexes and constraints involving the column.

If a table is partitioned or has any descendant tables, it is not permitted to add, rename, or change the type of a column, or rename an inherited constraint in the parent table without doing the same to the descendants. This ensures that the descendants always have columns matching the parent.

To see the structure of a partitioned table, you can use the view [pg\_partitions](../system_catalogs/pg_partitions.html). This view can help identify the particular partitions you may want to alter.

A recursive `DROP COLUMN` operation will remove a descendant table's column only if the descendant does not inherit that column from any other parents and never had an independent definition of the column. A nonrecursive `DROP COLUMN` \(`ALTER TABLE ONLY ... DROP COLUMN`\) never removes any descendant columns, but instead marks them as independently defined rather than inherited.

The `TRIGGER`, `CLUSTER`, `OWNER`, and `TABLESPACE` actions never recurse to descendant tables; that is, they always act as though `ONLY` were specified. Adding a constraint recurses only for `CHECK` constraints that are not marked `NO INHERIT`.

These `ALTER PARTITION` operations are supported if no data is changed on a partitioned table that contains a leaf child partition that has been exchanged to use an external table. Otherwise, an error is returned.

-   Adding or dropping a column.
-   Changing the data type of column.

These `ALTER PARTITION` operations are not supported for a partitioned table that contains a leaf child partition that has been exchanged to use an external table:

-   Setting a subpartition template.
-   Altering the partition properties.
-   Creating a default partition.
-   Setting a distribution policy.
-   Setting or dropping a `NOT NULL` constraint of column.
-   Adding or dropping constraints.
-   Splitting an external partition.

Changing any part of a system catalog table is not permitted.

## <a id="section6"></a>Examples 

Add a column to a table:

```
ALTER TABLE distributors ADD COLUMN address varchar(30);
```

Rename an existing column:

```
ALTER TABLE distributors RENAME COLUMN address TO city;
```

Rename an existing table:

```
ALTER TABLE distributors RENAME TO suppliers;
```

Add a not-null constraint to a column:

```
ALTER TABLE distributors ALTER COLUMN street SET NOT NULL;
```

Rename an existing constraint:

```
ALTER TABLE distributors RENAME CONSTRAINT zipchk TO zip_check;
```

Add a check constraint to a table and all of its children:

```
ALTER TABLE distributors ADD CONSTRAINT zipchk CHECK 
(char_length(zipcode) = 5);
```

To add a check constraint only to a table and not to its children:

```
ALTER TABLE distributors ADD CONSTRAINT zipchk CHECK (char_length(zipcode) = 5) NO INHERIT;
```

\(The check constraint will not be inherited by future children, either.\)

Remove a check constraint from a table and all of its children:

```
ALTER TABLE distributors DROP CONSTRAINT zipchk;
```

Remove a check constraint from one table only:

```
ALTER TABLE ONLY distributors DROP CONSTRAINT zipchk;
```

\(The check constraint remains in place for any child tables that inherit `distributors`.\)

Move a table to a different schema:

```
ALTER TABLE myschema.distributors SET SCHEMA yourschema;
```

Change a table's access method to `ao_row`:

```
ALTER TABLE distributors SET ACCESS METHOD ao_row;
```

Change a table's blocksize to 32768:

```
ALTER TABLE distributors SET (blocksize = 32768);
```

Change a table's access method to ao_row, compression type to zstd and compression level to 4:

```
ALTER TABLE sales SET ACCESS METHOD ao_row with (compresstype=zstd,compresslevel=4);
```

Change access method for all existing partitions of a table:

```
ALTER TABLE sales SET ACCESS METHOD ao_row;
```

Change all future partitions of a table to have an access method of `heap`, leaving the access method of current partitions as is:

ALTER TABLE ONLY sales SET ACCESS METHOD heap;

Add a column and change the table's access method:

```
ALTER TABLE distributors SET ACCESS METHOD ao_row, ADD column j int;
```

Add a column and change table storage parameters:

```
ALTER TABLE distributors SET (compresslevel=7), ADD COLUMN k int;
```

Change the distribution policy of a table to replicated:

```
ALTER TABLE myschema.distributors SET DISTRIBUTED REPLICATED;
```

Add a new partition to a partitioned table:

```
ALTER TABLE sales ADD PARTITION 
            START (date '2017-02-01') INCLUSIVE 
            END (date '2017-03-01') EXCLUSIVE;
```

Add a default partition to an existing partition design:

```
ALTER TABLE sales ADD DEFAULT PARTITION other;
```

Rename a partition:

```
ALTER TABLE sales RENAME PARTITION FOR ('2016-01-01') TO 
jan08;
```

Drop the first \(oldest\) partition in a range sequence:

```
ALTER TABLE sales DROP PARTITION FOR (RANK(1));
```

Exchange a table into your partition design:

```
ALTER TABLE sales EXCHANGE PARTITION FOR ('2016-01-01') WITH 
TABLE jan08;
```

Split the default partition \(where the existing default partition's name is `other`\) to add a new monthly partition for January 2017:

```
ALTER TABLE sales SPLIT DEFAULT PARTITION 
START ('2017-01-01') INCLUSIVE 
END ('2017-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);
```

Split a monthly partition into two with the first partition containing dates January 1-15 and the second partition containing dates January 16-31:

```
ALTER TABLE sales SPLIT PARTITION FOR ('2016-01-01')
AT ('2016-01-16')
INTO (PARTITION jan081to15, PARTITION jan0816to31);
```

For a multi-level partitioned table that consists of three levels, year, quarter, and region, exchange a leaf partition `region` with the table `region_new`.

```
ALTER TABLE sales ALTER PARTITION year_1 ALTER PARTITION quarter_4 EXCHANGE PARTITION region WITH TABLE region_new ;
```

In the previous command, the two `ALTER PARTITION` clauses identify which `region` partition to exchange. Both clauses are required to identify the specific partition to exchange.

## <a id="section7"></a>Compatibility 

The forms `ADD` \(without `USING INDEX`\), `DROP`, `SET DEFAULT`, and `SET DATA TYPE` \(without `USING`\) conform with the SQL standard. The other forms are Greenplum Database extensions of the SQL standard. Also, the ability to specify more than one manipulation in a single `ALTER TABLE` command is an extension.

`ALTER TABLE DROP COLUMN` can be used to drop the only column of a table, leaving a zero-column table. This is an extension of SQL, which disallows zero-column tables.

## <a id="section8"></a>See Also 

[CREATE TABLE](CREATE_TABLE.html), [DROP TABLE](DROP_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

