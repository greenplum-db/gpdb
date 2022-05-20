# pg_stat_all_indexes 

The `pg_stat_all_indexes` view shows one row for each index in the current database that displays statistics about accesses to that specific index.

The `pg_stat_user_indexes` and `pg_stat_sys_indexes` views contain the same information, but filtered to only show user and system indexes respectively.

|Column|Type|Description|
|------|----|-----------|
|`relid`|oid|OID of the table for this index|
|`indexrelid`|oid|OID of this index|
|`schemaname`|name|Name of the schema this index is in|
|`relname`|name|Name of the table for this index|
|`indexrelname`|name|Name of this index|
|`idx_scan`|bigint|Total number of index scans initiated on this index from all segment instances|
|`idx_tup_read`|bigint|Number of index entries returned by scans on this index|
|`idx_tup_fetch`|bigint|Number of live table rows fetched by simple index scans using this index|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

