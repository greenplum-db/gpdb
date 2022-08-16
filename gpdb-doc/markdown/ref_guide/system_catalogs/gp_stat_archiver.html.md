---
title: gp_stat_archiver 
---

The `gp_stat_archiver` view contains data about the WAL archiver process of the cluster. It displays one row per segment.

|column|type|references|description|
|------|----|----------|-----------|
|`archived_count`|bigint|Number of WAL files that have been successfully archived.|
|`last_archived_wal`|text|Name of the last WAL file successfully archived.|
|`last_archived_time`|timestamp with time zone|Time of the last successful archive operation.|
|`failed_count`|bigint|Number of failed attempts for archiving WAL files.|
|`last_failed_wal`|text|Name of the WAL file of the last failed archival operation.|
|`last_failed_time`|timestamp with time zone|Time of the last failed archival operation.|
|`stats_reset`|timestamp with time zone|Time at which these statistics were last reset.|
|`gp_segment_id`|int|The id of the segment to which the data being archived belongs.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

