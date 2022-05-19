# gp_fastsequence 

The `gp_fastsequence` table contains information about append-optimized and column-oriented tables. The `last_sequence` value indicates maximum row number currently used by the table.

|column|type|references|description|
|------|----|----------|-----------|
|`objid`|oid|pg\_class.oid|Object id of the `pg_aoseg.pg_aocsseg_*` table used to track append-optimized file segments.|
|`objmod`|bigint| |Object modifier.|
|`last_sequence`|bigint| |The last sequence number used by the object.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

