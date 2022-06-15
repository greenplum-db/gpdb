# gp\_id 

The `gp_id` system catalog table identifies the Greenplum Database system name and number of segments for the system. It also has `local` values for the particular database instance \(segment or master\) on which the table resides. This table is defined in the `pg_global` tablespace, meaning it is globally shared across all databases in the system.

|column|type|references|description|
|------|----|----------|-----------|
|`gpname`|name| |The name of this Greenplum Database system.|
|`numsegments`|integer| |The number of segments in the Greenplum Database system.|
|`dbid`|integer| |The unique identifier of this segment \(or master\) instance.|
|`content`|integer| |The ID for the portion of data on this segment instance. A primary and its mirror will have the same content ID.For a segment the value is from 0-*N-1*, where *N* is the number of segments in Greenplum Database.

For the master, the value is -1.

|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

