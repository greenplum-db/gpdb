# gp\_pgdatabase 

The `gp_pgdatabase` view shows status information about the Greenplum segment instances and whether they are acting as the mirror or the primary. This view is used internally by the Greenplum fault detection and recovery utilities to determine failed segments.

|column|type|references|description|
|------|----|----------|-----------|
|`dbid`|smallint|gp\_segment\_configuration.dbid|System-assigned ID. The unique identifier of a segment \(or master\) instance.|
|`isprimary`|boolean|gp\_segment\_configuration.role|Whether or not this instance is active. Is it currently acting as the primary segment \(as opposed to the mirror\).|
|`content`|smallint|gp\_segment\_ configuration.content|The ID for the portion of data on an instance. A primary segment instance and its mirror will have the same content ID.For a segment the value is from 0-*N-1*, where *N* is the number of segments in Greenplum Database.

For the master, the value is -1.

|
|`definedprimary`|boolean|gp\_segment\_ configuration.preferred\_role|Whether or not this instance was defined as the primary \(as opposed to the mirror\) at the time the system was initialized.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

