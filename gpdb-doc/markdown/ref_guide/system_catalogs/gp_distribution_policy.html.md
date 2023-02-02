# gp_distribution_policy 

The `gp_distribution_policy` table contains information about Greenplum Database tables and their policy for distributing table data across the segments. This table is populated only on the coordinator. This table is not globally shared, meaning each database has its own copy of this table.

|column|type|references|description|
|------|----|----------|-----------|
|`localoid`|oid|pg\_class.oid|The table object identifier \(OID\).|
|`policytype`|char| |The table distribution policy:<br/><br/>`p` - Partitioned policy. Table data is distributed among segment instances.<br/><br/>`r` - Replicated policy. Table data is replicated on each segment instance.|
|`numsegments`|integer| |The number of segment instances on which the table data is distributed.|
|`distkey`|int2vector|pg\_attribute.attnum|The column number\(s\) of the distribution column\(s\).|
|`distclass`|oidvector|pg\_opclass.oid|The operator class identifier\(s\) of the distribution column\(s\).|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

