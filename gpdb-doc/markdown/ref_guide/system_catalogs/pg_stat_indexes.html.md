---
title: pg_stat_all_indexes 
---

The `pg_stat_all_indexes` view shows one row for each index in the current database that displays statistics about accesses to that specific index.

The `pg_stat_user_indexes` and `pg_stat_sys_indexes` views contain the same information, but filtered to only show user and system indexes respectively.

In Greenplum Database 6, the `pg_stat_*_indexes` views display access statistics for indexes only from the master instance. Access statistics from segment instances are ignored. You can create views that display usage statistics that combine statistics from the master and the segment instances, see [Index Access Statistics from the Master and Segment Instances](#index_stats_all_6x).

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

## <a id="index_stats_all_6x"></a>Index Access Statistics from the Master and Segment Instances 

To display index access statistics that combine statistics from the master and the segment instances you can create these views. A user requires `SELECT` privilege on the views to use them.

```
-- Create these index access statistics views
--   pg_stat_all_indexes_gpdb6
--   pg_stat_sys_indexes_gpdb6
--   pg_stat_user_indexes_gpdb6

CREATE VIEW pg_stat_all_indexes_gpdb6 AS
SELECT
    s.relid,
    s.indexrelid,
    s.schemaname,
    s.relname,
    s.indexrelname,
    m.idx_scan,
    m.idx_tup_read,
    m.idx_tup_fetch
FROM
    (SELECT
         relid,
         indexrelid,
         schemaname,
         relname,
         indexrelname,
         sum(idx_scan) as idx_scan,
         sum(idx_tup_read) as idx_tup_read,
         sum(idx_tup_fetch) as idx_tup_fetch
     FROM gp_dist_random('pg_stat_all_indexes')
     WHERE relid >= 16384
     GROUP BY relid, indexrelid, schemaname, relname, indexrelname
     UNION ALL
     SELECT *
     FROM pg_stat_all_indexes
     WHERE relid < 16384) m, pg_stat_all_indexes s
WHERE m.relid = s.relid AND m.indexrelid = s.indexrelid;


CREATE VIEW pg_stat_sys_indexes_gpdb6 AS 
    SELECT * FROM pg_stat_all_indexes_gpdb6
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';


CREATE VIEW pg_stat_user_indexes_gpdb6 AS 
    SELECT * FROM pg_stat_all_indexes_gpdb6
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

```

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

