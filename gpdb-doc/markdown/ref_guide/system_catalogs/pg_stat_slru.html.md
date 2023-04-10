# pg_stat_slru

Greenplum Database accesses certain on-disk information via SLRU (simple least-recently-used) caches. The `pg_stat_slru` view contains one row for each tracked SLRU cache, showing statistics about access to cached pages.

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name of the SLRU.|
|`blks_zeroed`|bigint| |Number of blocks zeroed during initializations.|
|`blks_hit`|bigint| |Number of times disk blocks were found already in the SLRU, so that a read was not necessary (this only includes hits in the SLR, not the operating system's file system cache).|
|`blks_read`|bigint| |Number of disk blocks read for this SLRU.|
|`blks_written`|biging| |Number of disk blocks written for this SLRU.|
|`blks_exists`|biging| |Number of blocks checked for existence for this SLRU.|
|`flushes`|bigint| |Number of flushes of dirty data for this SLRU.|
|`truncates`|bigint| |Number of truncates for this SLRU.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

