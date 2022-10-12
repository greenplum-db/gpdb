---
title: pg_partition 
---

The `pg_partition` system catalog table is used to track partitioned tables and their inheritance level relationships. Each row of `pg_partition` represents either the level of a partitioned table in the partition hierarchy, or a subpartition template description. The value of the attribute `paristemplate` determines what a particular row represents.

|column|type|references|description|
|------|----|----------|-----------|
|`parrelid`|oid|pg\_class.oid|The object identifier of the table.|
|`parkind`|char| |The partition type - `R` for range or `L` for list.|
|`parlevel`|smallint| |The partition level of this row: 0 for the top-level parent table, 1 for the first level under the parent table, 2 for the second level, and so on.|
|`paristemplate`|boolean| |Whether or not this row represents a subpartition template definition \(true\) or an actual partitioning level \(false\).|
|`parnatts`|smallint| |The number of attributes that define this level.|
|`paratts`|int2vector| |An array of the attribute numbers \(as in `pg_attribute.attnum`\) of the attributes that participate in defining this level.|
|`parclass`|oidvector|pg\_opclass.oid|The operator class identifier\(s\) of the partition columns.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

