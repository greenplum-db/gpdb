# gp_partition_template

The `gp_partition_template` system catalog table describes the relationship between a partitioned table and the subpartition template defined at each level in the partition hierarchy.

Each subpartition template has a dependency on the existence of a template at the next lowest level of the hierarchy.

|column|type|references|description|
|------|----|----------|-----------|
|`relid`| oid | [pg_class](pg_class.html).oid| The object identifier of the partitioned table. |
|`level`|smallint| | The level of the partition in the hierarchy. |
|`template`|pg_node_tree| | Expression representation of the subpartition template defined for each partition at this level of the hierarchy. |

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

