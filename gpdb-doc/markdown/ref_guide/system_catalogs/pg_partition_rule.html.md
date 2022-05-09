# pg_partition_rule 

The `pg_partition_rule` system catalog table is used to track partitioned tables, their check constraints, and data containment rules. Each row of `pg_partition_rule` represents either a leaf partition \(the bottom level partitions that contain data\), or a branch partition \(a top or mid-level partition that is used to define the partition hierarchy, but does not contain any data\).

|column|type|references|description|
|------|----|----------|-----------|
|`paroid`|oid|pg\_partition.oid|Row identifier of the partitioning level \(from [pg\_partition](pg_partition.html)\) to which this partition belongs. In the case of a branch partition, the corresponding table \(identified by `pg_partition_rule`\) is an empty container table. In case of a leaf partition, the table contains the rows for that partition containment rule.|
|`parchildrelid`|oid|pg\_class.oid|The table identifier of the partition \(child table\).|
|`parparentrule`|oid|pg\_partition\_rule.paroid|The row identifier of the rule associated with the parent table of this partition.|
|`parname`|name| |The given name of this partition.|
|`parisdefault`|boolean| |Whether or not this partition is a default partition.|
|`parruleord`|smallint| |For range partitioned tables, the rank of this partition on this level of the partition hierarchy.|
|`parrangestartincl`|boolean| |For range partitioned tables, whether or not the starting value is inclusive.|
|`parrangeendincl`|boolean| |For range partitioned tables, whether or not the ending value is inclusive.|
|`parrangestart`|text| |For range partitioned tables, the starting value of the range.|
|`parrangeend`|text| |For range partitioned tables, the ending value of the range.|
|`parrangeevery`|text| |For range partitioned tables, the interval value of the `EVERY` clause.|
|`parlistvalues`|text| |For list partitioned tables, the list of values assigned to this partition.|
|`parreloptions`|text| |An array describing the storage characteristics of the particular partition.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

