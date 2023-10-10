---
title: Migrating Partition Maintenance Scripts to the New Greenplum 7 Partitioning Catalogs
---

This topic provides guidance on migrating partition maintenance scripts that you may have written for Greenplum 6 to use the new partitioning system catalogs in Greenplum 7.

## <a id="about_removed"></a> About the Greenplum 6 and 7 Partitioning Catalogs

The following partitioning-related catalog tables, views, and functions available in Greenplum 6 are removed in Greenplum 7:

- [pg_partition](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition.html)
- [pg_partition_columns](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_columns.html)
- [pg_partition_encoding](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_encoding.html)
- [pg_partition_rule](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_rule.html)
- [pg_partition_templates](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_templates.html)
- [pg_partitions](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partitions.html)
- [pg_stat_partition_operations](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_stat_partition_operations.html)
- `pg_partition_def()`

Greenplum 7 adds the following new catalog table and functions:

- [gp_partition_template](../ref_guide/system_catalogs/gp_partition_template.html)
- [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html)
- [pg_partition_tree()](../admin_guide/ddl/ddl-partition.html#topic76)
- [pg_partition_ancestors()](../admin_guide/ddl/ddl-partition.html#topic76)
- [pg_partition_root()](../admin_guide/ddl/ddl-partition.html#topic76)

## <a id="why"></a> Why are Migration Tasks Required?

Because partitioning-related system catalog, view, and function definitions have changed in Greenplum 7, you must update any partition maintenance scripts that you have been using in Greenplum 6.

The following sections identify, for each Greenplum 6 partitioning catalog column, how to obtain similar information in Greenplum 7. These mappings should help ease your partition maintenance migration effort.

In some cases, there is an equivalent field in, or query of, a Greenplum 7 system catalog or view. In other cases, there may be no direct mapping because Greenplum 7 no longer stores the information in the catalog.

## <a id="pg_partitions"></a> pg_partitions

The Greenplum 6 [pg_partitions](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partitions.html) view displays all leaf partitions in the current database.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| schemaname | The name of the schema in which the root partitioned table resides. | Use `pg_partition_root()` to obtain the root object identifier, and then query [pg_namespace](../ref_guide/system_catalogs/pg_namespace.html). |
| tablename | The name of the root partitioned table. | Use `pg_partition_root()` to obtain the root object identifier, and then query [pg_class](../ref_guide/system_catalogs/pg_class.html). |
| partitionschemaname | The namespace of the leaf partition. | [pg_namespace](../ref_guide/system_catalogs/pg_namespace.html) |
| partitiontablename | The table name of the leaf partition (the table name you use to access the partition directly). | [pg_class](../ref_guide/system_catalogs/pg_class.html) |
| partitionname | The partition name of the leaf partition (the name you use to refer to the partition in an `ALTER TABLE` command). | N/A |
| parentpartitiontablename | The table name of the parent table of this partition. | Get the parent object identifier via [pg_inherits](../ref_guide/system_catalogs/pg_inherits.html) and then query [pg_class](../ref_guide/system_catalogs/pg_class.html). |
| parentpartitionname | The partition name of the parent table of this partition. | N/A |
| partitiontype | The type of partition (range or list). | Get the parent object identifier via [pg_inherits](../ref_guide/system_catalogs/pg_inherits.html) and then query [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html). |
| partitionlevel | The level of this partition in the partition hierarchy. | Get the level from `pg_partition_tree()` using the root object identifier.</br>Note that the level differs in Greenplum 6 and 7. In Greenplum 6, the level of the immediate child of a partitioned table is 0. In Greenplum 7, the level of the partitioned table itself is 0, and the level of its immediate child is 1. |
| partitionrank | For range partitions, the rank of the partition compared to other partitions at the same level. | N/A |
| partitionposition | The rule order position of this partition. | N/A |
| partitionlistvalues | For list partitions, the list value(s) associated with this partition. | Get the partition boundary via `pg_get_expr()` and then filter the text. |
| partitionrangestart | For range partitions, the start value of this partition. | Get the partition boundary via `pg_get_expr()` and then filter the text. |
| partitionstartinclusive | Whether or not the start value is included in this partition. `true` if the start value is included. | always inclusive |
| partitionrangeend | For range partitions, the end value of this partition. | Get the partition boundary via `pg_get_expr()` and then filter the text. |
| partitionendinclusive | Whether or not the end value is included in this partition. `true` if the end value is included. | always exclusive |
| partitioneveryclause | The `EVERY` clause (interval) of this partition. | N/A |
| partitionisdefault | Whether or not this is a default partition. `true` if this is the default, otherwise `false`. | Get the partition boundary via `pg_get_expr()` and check if it is `DEFAULT`. Alternatively, use `pg_partitioned_table.partdefid`.  |
| partitionboundary | The entire partition specification for this partition. | `pg_get_expr()`, but note that it is returned in modern syntax. |
| parenttablespace | The tablespace of the parent table of this partition. | Get the parent object identifier via [pg_inherits](../ref_guide/system_catalogs/pg_inherits.html) and then query [pg_tablespace](../ref_guide/system_catalogs/pg_tablespace.html). |
| partitiontablespace | The tablespace of this partition. | [pg_tablespace](../ref_guide/system_catalogs/pg_tablespace.html) |

### <a id="pgparts_retrieve"></a> Examples Retrieving Similar Information in Greenplum 7

Some of the information is quite trivial to retrieve in Greenplum 7, while other information is not. The following sections provide examples for specific Greenplum 6 system catalog columns.

#### <a id="pnppn"></a> partitionname / parentpartitionname

The `partitionname` and `parentpartitionname` columns provide the Greenplum 6 "partition name" in constrast to the actual table name. Because Greenplum 7 no longer stores the partition name in the catalog (previously in `pg_partition_rule`), it cannot retrieve that information now. Use the table names directly to refer to the partitions. However, if you really need the partition name, you can achieve that via some text massaging:

```
SELECT
    c.relname AS table_name,
    split_part(substr(c.relname, position(inh.inhparent::regclass::text in c.relname) + length(inh.inhparent::regclass::text)), '_', 4) AS partition_name
FROM
    pg_class c
LEFT JOIN
    pg_inherits inh ON inh.inhrelid = c.oid
WHERE
    c.relname LIKE concat(inh.inhparent::regclass::text, '%')
    AND c.relname LIKE '<your_partition_name>';
```

This query is valid only if the table name is implicitly generated by Greenplum from the partition name and is in the form of `<parent_tablename>_prt_<level>_<partition_name>`. This query will not work for a partitioned table created with modern syntax (where you always specify the table name when adding a partition) or for a partitioned table created with classic syntax where you explicitly specify the table name in a `WITH` clause for the partition.

#### <a id="pppr"></a> partitionposition / partitionrank

The `partitionposition` and `partitionrank` columns in the Greenplum 6 `pg_partitions` view are based on the "order" number for each partition in the `pg_partition_rule` catalog, but that catalog is removed in Greenplum 7. However, if you are simply interested in retrieving the sorted order for range partitions, you can order the range value. For example, if the Greenplum 6 query below provides the highest rank partition:

```
SELECT 
    CASE 
        WHEN partitionstartinclusive THEN partitionrangestart
        ELSE partitionrangeend
    END AS old_part_value
FROM 
    pg_catalog.pg_partitions p
WHERE 
    p.schemaname = '" + cfg.schema + "'
    AND p.tablename = '" + cfg.table + "'
    AND p.partitiontype = 'range'
    AND p.partitionlevel = " + str(cfg.partition_level) + "
    AND " + chk + "
ORDER BY 
    partitionrank
LIMIT 1;
```

A similar query in Greenplum 7 follows:

```
SELECT
   rank() OVER (
       PARTITION BY pc.oid
       ORDER BY CAST(
           (regexp_matches(pg_get_expr(c.relpartbound, c.oid), 'FOR VALUES FROM \(([0-9]+)\) TO \(([0-9]+)\)'))[1]
           AS INTEGER
       )
   ) AS rank,
    (regexp_matches(pg_get_expr(c.relpartbound, c.oid), 'FOR VALUES FROM \(([0-9]+)\) TO \(([0-9]+)\)'))[1] AS old_part_value
FROM
    pg_class c
LEFT JOIN
    pg_inherits inh ON inh.inhrelid = c.oid
LEFT JOIN
    pg_partitioned_table pt ON inh.inhparent = pt.partrelid
LEFT JOIN
    pg_class pc ON pc.oid = pt.partrelid
WHERE
    c.relispartition = 't' AND pc.relname LIKE ('<partition_root_schema>.<partition_root_name>')
ORDER BY
    rank
LIMIT 1;
```

Assuming that the partitioning method for the partitioned table is `RANGE`, the above query calculates the ranks for each child partition, and prints the range `START` value (which is always inclusive) with the highest rank.

You cannot retrieve the order number for list partitions with this query, however, because the order is based on the time the partition is added, and Greenplum 7 does not store that information. A query for list partitions must not be dependent on order information.

#### <a id="pec"></a> partitioneveryclause

Because Greenplum 7 does not store the use of the `EVERY` clause in the catalog, it can not discern if a partition was created with the clause. The main use of this column is to reconstruct a partition definition clause using the classic-syntax for display or `pg_dump` purposes. No mapping is provided at this time.

#### <a id="psipei"></a> partitionstartinclusive / partitionendinclusive

Greenplum 7 supports the classic syntax `INCLUSIVE` and `EXCLUSIVE` clauses through the adjustment of `START` and `END` values. It dos not record whether a range `START` or `END` is inclusive or not, because the `START` is always inclusive and the `END` is always exclusive. So the `partitionstartinclusive` and `partitionendinclusive` columns are simply redundant.

#### <a id="pb"></a> partitionboundary

You can retrieve the partition boundary definition in Greenplum 7 via the `pg_get_expr()` function:

```
SELECT 
    pg_get_expr(relpartbound, oid)
FROM 
    pg_class 
WHERE 
    relispartition = 't';
```

### <a id="pgparts_compose"></a> Composing a Similar View in Greenplum 7

An approximate `pg_partitions` view follows.  This view, given all limitations addressed above, prints the information that is possible to retrieve in Greenplum 7.

> **Caution** This example is for illustrative purposes only, not for practical use.

```
SELECT
    (SELECT relnamespace::regnamespace FROM pg_class WHERE oid = pg_partition_root(c.oid)) AS schemaname,
    (SELECT pg_partition_root(c.oid)::regclass) AS tablename,
    n.nspname AS partitionschemaname,
    c.relname AS partitiontablename,
    -- assuming the table name is implicitly generated like <parent>_prt_<id>_<partition>
    split_part(
        substr(c.relname, position(pc.relname in c.relname) + length(pc.relname)),
        '_',
        4
    ) AS partitionname,
    pc.relname AS parentpartitiontablename,
    -- same assumption as above
    split_part(
        substr(pc.relname, position(ppc.relname in pc.relname) + length(ppc.relname)),
        '_',
        4
    ) AS parentpartitionname,
    CASE
        WHEN pt.partstrat = 'r' THEN 'range'
        ELSE 'list'
    END AS partitiontype,
    (SELECT level FROM pg_partition_tree(pg_partition_root(c.oid)) WHERE relid = c.oid) AS partitionlevel,
    -- can be calculated like a previous example
    NULL AS partitionrank,
    -- cannot be trusted because no real order for list partitions
    NULL AS partitionposition,
    (regexp_matches(pg_get_expr(c.relpartbound, c.oid), 'FOR VALUES IN \((.*?)\)'))[1] AS partitionlistvalues,
    -- assuming range values are normal integers instead of expressions which Greenplum 7 supports
    (regexp_matches(pg_get_expr(c.relpartbound, c.oid), 'FOR VALUES FROM \(([0-9]+)\) TO \(([0-9]+)\)'))[1] AS partitionrangestart,
    't' AS partitionstartinclusive,
    (regexp_matches(pg_get_expr(c.relpartbound, c.oid), 'FOR VALUES FROM \(([0-9]+)\) TO \(([0-9]+)\)'))[2] AS partitionrangeend,
    'f' AS partitionendinclusive,
    -- information cannot be retrieved
    NULL AS partitioneveryclause,
    CASE
        WHEN pg_get_expr(c.relpartbound, c.oid) = 'DEFAULT' THEN 't'
        ELSE 'f'
    END AS partitionisdefault,
    (regexp_matches(pg_get_expr(c.relpartbound, c.oid), '.+'))[1] AS partitionboundary,
    CASE
        WHEN pc.reltablespace = 0 THEN 'pg_default'
        ELSE (SELECT spcname FROM pg_tablespace WHERE oid = pc.reltablespace)
    END AS parenttablespace,
    CASE
        WHEN c.reltablespace = 0 THEN 'pg_default'
        ELSE (SELECT spcname FROM pg_tablespace WHERE oid = c.reltablespace)
    END AS partitiontablespace
FROM
    pg_class c
LEFT JOIN
    pg_namespace n ON n.oid = c.relnamespace
LEFT JOIN
    pg_inherits inh ON inh.inhrelid = c.oid
LEFT JOIN
    pg_partitioned_table pt ON inh.inhparent = pt.partrelid
LEFT JOIN
    pg_class pc ON pc.oid = pt.partrelid
LEFT JOIN
    pg_inherits pinh ON pinh.inhrelid = pc.oid
LEFT JOIN
    pg_partitioned_table ppt ON pinh.inhparent = ppt.partrelid
LEFT JOIN
    pg_class ppc ON ppc.oid = ppt.partrelid
WHERE
    c.relispartition = 't';
```

## <a id="pg_partition_columns"></a> pg_partition_columns

The Greenplum 6 [pg_partition_columns](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_columns.html) view displays the partition key columns of a partitioned table.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| schemaname | The name of the schema in which the partitioned table resides. | Get the root partitioned table object identifier via `pg_partition_root()` and then query [pg_namespace](../ref_guide/system_catalogs/pg_namespace.html). |
| tablename | The table name of the partitioned table. | [pg_class](../ref_guide/system_catalogs/pg_class.html) |
| columnname | The name of the partition key column. | [pg_attribute](../ref_guide/system_catalogs/pg_attribute.html) |
| partitionlevel | The level of this subpartition in the partition hierarchy. | Get the level from `pg_partition_tree() `using the parent object identifier. |
| position_in_partition_key | List partitions can have a composite (multi-column) partition key. This attribute identifies the position of the column in a composite key. | always 1 |

The `position_in_partition_key` column is no longer relevant in Greenplum 7. Greenplum 7 supports a multi-column partition key via multi-column type, so the relative position of the column in the partition key will always be one. Another reason that this no longer holds true in Greenplum 7: the `pg_partition_columnns` view assumes a homogenous partition structure, while Greenplum 7 also supports heterogenous partition structures.

### <a id="pgpartcols_compose"></a> Composing a Similar View in Greenplum 7

As long as these two assumptions hold:

1. The partitioned table does not have a multi-column partition key.
1. All partition structures are a homogeneous.

A query to generate a comparable view in Greenplum 7 follows:

```
SELECT 
    c.relnamespace::regnamespace AS schemaname,
    c.relname AS tablename,
    att.attname AS columnname,
    (
        SELECT level 
        FROM pg_partition_tree(pg_partition_root(c.oid)) 
        WHERE relid = c.oid
    ) AS partitionlevel,
    1 AS position_in_partition_key
FROM 
    pg_partitioned_table pt 
LEFT JOIN 
    pg_class c ON c.oid = pt.partrelid 
JOIN 
    pg_attribute att ON c.oid = att.attrelid AND att.attnum = pt.partattrs[0];
```

## <a id="pg_partition_rule"></a> pg_partition_rule

The Greenplum 6 [pg_partition_rule](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_rule.html) table tracks partitioned tables, their check constraints, and their data containment rules.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| paroid | The row identifier of the partitioning level (from `pg_partition`) to which this partition belongs. In the case of a branch partition, the corresponding table (identified by `pg_partition_rule`) is an empty container table. In the case of a leaf partition, the table contains the rows for that partition containment rule. | N/A (removed object) |
| parchildrelid | The table identifier of the partition. | [pg_class](../ref_guide/system_catalogs/pg_class.html) |
| parparentrule | The row identifier of the rule associated with the parent table of this partition. | N/A (removed object) |
| parname | The given name of this partition. | [pg_class](../ref_guide/system_catalogs/pg_class.html) |
| parisdefault | Specifies whether or not this partition is a default partition. | See [pg_partitions](#pg_partitions).partitionisdefault. |
| parruleord | For range partitioned tables, the rank of this partition in this level of the partition hierarchy. | N/A (See the explanation for the `partitionposition`/`partitionrank` columns in [pg_partitions](#pg_partitions).) |
| parrangestartincl | For range partitioned tables, specifies whether or not the starting value is inclusive. | always inclusive |
| parrangeendincl | For range partitioned tables, specifies whether or not the ending value is inclusive. | always exclusive |
| parrangestart | For range partitioned tables, the starting value of the range. | See [pg_partitions](#pg_partitions).partitionrangestart. |
| parrangeend | For range partitioned tables, the ending value of the range. | See [pg_partitions](#pg_partitions).partitionrangeend. |
| parrangeevery | For range partitioned tables, the interval value of the `EVERY` clause. | N/A (See [pg_partitions](#pg_partitions).partitioneveryclause.) |
| parlistvalues | For list partitioned tables, the list of values assigned to this partition. | See [pg_partitions](#pg_partitions).partitionlistvalues. |
| parreloptions | An array describing the storage characteristics of the particular partition. | [pg_class](../ref_guide/system_catalogs/pg_class.html).reloptions |


## <a id="pg_partition"></a> pg_partition

The Greenplum 6 [pg_partition](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition.html) system catalog table tracks partitioned tables and their inheritance level relationships.

The majority of this information resides in [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html) in Greenplum 7.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| parrelid | The object identifier of the table. | [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partrelid |
| parkind | The partition type - R for range or L for list. | [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partstrat |
| parlevel | The partition level of this row: 0 for the top-level parent table, 1 for the first level under the parent table, 2 for the second level, and so on. | Cross-check [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html) and `pg_partition_root()/pg_partition_tree()`, similar to how you retrieve [pg_partitions](#pg_partitions).partitionlevel. |
| paristemplate | Whether or not this row represents a subpartition template definition (`true`) or an actual partitioning level (`false`). | Cross-check [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partrelid with [gp_partition_template](../ref_guide/system_catalogs/gp_partition_template.html). |
| parnatts | The number of attributes that define this level. | [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partnatts |
| paratts | An array of the attribute numbers (as in `pg_attribute.attnum`) of the attributes that participate in defining this level. | [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partattrs |
| parclass | The operator class identifier(s) of the partition columns. | [pg_partitioned_table](../ref_guide/system_catalogs/pg_partitioned_table.html).partclass |

## <a id="pg_partition_templates"></a> pg_partition_templates

The Greenplum 6 [pg_partition_templates](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_templates.html) view displays the subpartitions that were created using a subpartition template.

While the information is same as that of the `pg_partitions` view, `pg_partition_templates` displays only those subpartitions created by a subpartition template (cross-check with `pg_partition.paristemplate` of the root partitioned table).

> **Note** Greenplum 6 assumes that if the root partitioned table has a subpartition template, then all of its subpartitions are created by this subpartition template. In Greenplum 7 this assumption no longer holds true - you can add any table to a partitioned table with `ATTACH PARTITION`. Ensure that you take this into account when you rewrite your scripts.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| schemaname | The name of the schema in which the partitioned table resides. | See [pg_partitions](#pg_partitions).schemaname. |
| tablename | The table name of the top-level parent table. | See [pg_partitions](#pg_partitions).tablename. |
| partitionname | The name of the subpartition (this is the name to use if referring to the partition in an `ALTER TABLE` command). NULL if the partition was not given a name at create time or generated by an `EVERY` clause. | See [pg_partitions](#pg_partitions).partitionname. |
| partitiontype | The type of subpartition (range or list). | See [pg_partitions](#pg_partitions).partitiontype. |
| partitionlevel | The level of this subpartition in the hierarchy. | See [pg_partitions](#pg_partitions).partitionlevel. |
| partitionrank | For range partitions, the rank of the partition compared to other partitions of the same level. | See [pg_partitions](#pg_partitions).partitionrank. |
| partitionposition | The rule order position of this subpartition. | See [pg_partitions](#pg_partitions).partitionposition. |
| partitionlistvalues | For list partitions, the list value(s) associated with this subpartition. | See [pg_partitions](#pg_partitions).partitionlistvalues. |
| partitionrangestart | For range partitions, the start value of this subpartition. | See [pg_partitions](#pg_partitions).partitionrangestart. |
| partitionstartinclusive | Whether or not the start value is included in this partition. `true` if the start value is included. `false` if it is excluded. | See [pg_partitions](#pg_partitions).partitionstartinclusive. |
| partitionrangeend | For range partitions, the end value of this subpartition. | See [pg_partitions](#pg_partitions).partitionrangend. |
| partitionendinclusive | Whether or not the end value is included in this partition. `true` if the end value is included. `false` if it is excluded. | See [pg_partitions](#pg_partitions).partitionendinclusive. |
| partitioneveryclause | The `EVERY` clause (interval) of this subpartition. | See [pg_partitions](#pg_partitions).partitioneveryclause. |
| partitionisdefault | Whether or not this is a default subpartition. `true` if this is the default, otherwise `false`. | See [pg_partitions](#pg_partitions).partitionisdefault. |
| partitionboundary | The entire partition specification for this subpartition. | See [pg_partitions](#pg_partitions).partitionboundary. |


## <a id="pg_partition_encoding"></a> pg_partition_encoding

The Greenplum 6 [pg_partition_encoding](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_partition_encoding.html) table describes the available column compression options for a subpartition template. This information is stored in [gp_partition_template](../ref_guide/system_catalogs/gp_partition_template.html) in Greenplum 7.

You must perform some text filtering to retrieve the per-column encoding information in Greenplum 7 in the same format as that of `pg_partition_encoding`.

| Column Name | Greenplum 6 Description | Greenplum 7 Equivalent |
|-------------|-----------------------|----------------|
| parencoid | The object identifier of the parent partition of this subpartition template. | `gp_partition_template` |
| parencattnum | The attribute number of the column for which the encoding option applies. | `pg_get_expr(template, <root_partition_oid>)` |
| parencattoptions | The storage option of this column. | `pg_get_expr(template, <root_partition_oid>)` |

### <a id="pgpartenc_compose"></a> Composing a Similar View in Greenplum 7

You can retrieve a one-line definition of the template using `pg_get_expr()` as follows:

```
SELECT level, pg_get_expr(template, relid) from gp_partition_template where relid::regclass::text = '<tablename>';
 level |                                                                                                          pg_get_expr
-------+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     1 | SUBPARTITION TEMPLATE(SUBPARTITION sp1 VALUES (1, 2, 3, 4, 5), COLUMN i ENCODING (compresstype=zlib), COLUMN j ENCODING (compresstype=rle_type), COLUMN k ENCODING (compresstype=zlib), COLUMN l ENCODING (compresstype=zlib))
```

## <a id="pg_stat_partition_operations"></a> pg_stat_partition_operations

The Greenplum 6 [pg_stat_partition_operations](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_stat_partition_operations.html) view displays details about the last operation performed on a partitioned table.

The view displays the same information as [pg_stat_operations](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-system_catalogs-pg_stat_operations.html), but only for the partitioned table and its child partitions. You can retrieve similar information in Greenplum 7 with a query such as:

```
SELECT
    so.*,
    (SELECT level FROM pg_partition_tree(pg_partition_root(c.oid)) WHERE relid = c.oid) AS partitionlevel,
    pc.relname AS parentpartitiontablename,
    pc.relnamespace::regnamespace AS parentschemaname,
    pc.oid AS parent_relid
FROM
    pg_stat_operations so
LEFT JOIN
    pg_class c ON so.objid = c.oid
LEFT JOIN
    pg_namespace n ON n.oid = c.relnamespace
LEFT JOIN
    pg_inherits inh ON inh.inhrelid = c.oid
LEFT JOIN
    pg_partitioned_table pt ON inh.inhparent = pt.partrelid
LEFT JOIN
    pg_class pc ON pc.oid = pt.partrelid
WHERE
    c.relispartition = 't' OR c.oid IN (SELECT partrelid FROM pg_partitioned_table);
```

