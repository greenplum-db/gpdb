# pg_partition_columns 

The `pg_partition_columns` system view is used to show the partition key columns of a partitioned table.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name| |The name of the schema the partitioned table is in.|
|`tablename`|name| |The table name of the top-level parent table.|
|`columnname`|name| |The name of the partition key column.|
|`partitionlevel`|smallint| |The level of this subpartition in the hierarchy.|
|`position_in_partition_key`|integer| |For list partitions you can have a composite \(multi-column\) partition key. This shows the position of the column in a composite key.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

