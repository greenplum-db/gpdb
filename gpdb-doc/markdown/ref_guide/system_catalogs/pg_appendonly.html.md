# pg_appendonly 

The `pg_appendonly` table contains information about the storage options and other characteristics of append-optimized tables.

|column|type|references|description|
|------|----|----------|-----------|
|`relid`|oid| |The table object identifier \(OID\) of the table.|
|`segrelid`|oid| |Table on-disk segment file id.|
|`blkdirrelid`|oid| |Block used for on-disk column-oriented table file.|
|`visimaprelid`|oid| |Visibility map for the table.|
|`version`|smallint| |AO relation version.|

> **Note** <sup>1</sup>QuickLZ compression is available only in the commercial release of VMware Greenplum.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

