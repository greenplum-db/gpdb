# gp_partition_template

The `gp_partition_template` system catalog table describes the relationship between a partitioned table and the sub-partition template defined at each level in the partition hierarchy.

> **Note** Greenplum Data supports sub-partition templates only for partitioned tables that you create with the classic syntax.

Each sub-partition template has a dependency on the existence of a template at the next lower level of the hierarchy.

|column|type|references|description|
|------|----|----------|-----------|
|`relid`| oid | [pg_class](pg_class.html).oid| The object identifier of the root partitioned table. |
|`level`|smallint| | The level of the partition in the hierarchy. The levels are numbered as follows: level `0` is the root partitioned table itself, level `1` represents the direct child/children of the root partitioned table, and so forth. The leaf partitions have the highest level number.|
|`template`|pg_node_tree| | Expression representation of the sub-partition template defined for each partition at this level of the hierarchy. |

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

