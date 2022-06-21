# pg\_stat\_all\_tables 

The `pg_stat_all_tables` view shows one row for each table in the current database \(including TOAST tables\) to display statistics about accesses to that specific table.

The `pg_stat_user_tables` and `pg_stat_sys_table`s views contain the same information, but filtered to only show user and system tables respectively.

In Greenplum Database 5, the `pg_stat_*_tables` views display access statistics for tables only from the master instance. Access statistics from segment instances are ignored. You can create views that display access statistics that combine statistics from the master and the segment instances, see [Table Access Statistics from the Master and Segment Instances](#tbl_stats_all_5x).

|Column|Type|Description|
|------|----|-----------|
|`relid`|oid|OID of a table|
|`schemaname`|name|Name of the schema that this table is in|
|`relname`|name|Name of this table|
|`seq_scan`|bigint|Total number of sequential scans initiated on this table from all segment instances|
|`seq_tup_read`|bigint|Number of live rows fetched by sequential scans|
|`idx_scan`|bigint|Total number of index scans initiated on this table from all segment instances|
|`idx_tup_fetch`|bigint|Number of live rows fetched by index scans|
|`n_tup_ins`|bigint|Number of rows inserted|
|`n_tup_upd`|bigint|Number of rows updated \(includes HOT updated rows\)|
|`n_tup_del`|bigint|Number of rows deleted|
|`n_tup_hot_upd`|bigint|Number of rows HOT updated \(i.e., with no separate index update required\)|
|`n_live_tup`|bigint|Estimated number of live rows|
|`n_dead_tup`|bigint|Estimated number of dead rows|
|`last_vacuum`|timestamp with time zone|Last time this table was manually vacuumed \(not counting `VACUUM FULL`\)|
|`last_autovacuum`|timestamp with time zone|Last time this table was vacuumed by the autovacuum daemon<sup>1</sup>|
|`last_analyze`|timestamp with time zone|Last time this table was manually analyzed|
|`last_autoanalyze`|timestamp with time zone|Last time this table was analyzed by the autovacuum daemon<sup>1</sup>|

**Note:** <sup>1</sup>In Greenplum Database, the autovacuum daemon is disabled and not supported for user defined databases.

## Table Access Statistics from the Master and Segment Instances 

To display table access statistics that combine statistics from the master and the segment instances, you can create these views. A user requires `SELECT` privilege on the views to use them.

```
-- Create these table access statistics views
--   pg_stat_all_tables_gpdb5
--   pg_stat_sys_tables_gpdb5
--   pg_stat_user_tables_gpdb5

CREATE VIEW pg_stat_all_tables_gpdb5 AS
SELECT
    s.relid,
    s.schemaname,
    s.relname,
    m.seq_scan,
    m.seq_tup_read,
    m.idx_scan,
    m.idx_tup_fetch,
    m.n_tup_ins,
    m.n_tup_upd,
    m.n_tup_del,
    m.n_tup_hot_upd,
    m.n_live_tup,
    m.n_dead_tup,
    s.last_vacuum,
    s.last_autovacuum,
    s.last_analyze,
    s.last_autoanalyze
FROM
    (SELECT
         relid,
         schemaname,
         relname,
         sum(seq_scan) as seq_scan,
         sum(seq_tup_read) as seq_tup_read,
         sum(idx_scan) as idx_scan,
         sum(idx_tup_fetch) as idx_tup_fetch,
         sum(n_tup_ins) as n_tup_ins,
         sum(n_tup_upd) as n_tup_upd,
         sum(n_tup_del) as n_tup_del,
         sum(n_tup_hot_upd) as n_tup_hot_upd,
         sum(n_live_tup) as n_live_tup,
         sum(n_dead_tup) as n_dead_tup,
         max(last_vacuum) as last_vacuum,
         max(last_autovacuum) as last_autovacuum,
         max(last_analyze) as last_analyze,
         max(last_autoanalyze) as last_autoanalyze
     FROM gp_dist_random('pg_stat_all_tables')
     WHERE relid >= 16384
     GROUP BY relid, schemaname, relname
     UNION ALL
     SELECT *
     FROM pg_stat_all_tables
     WHERE relid < 16384) m, pg_stat_all_tables s
 WHERE m.relid = s.relid;


CREATE VIEW pg_stat_sys_tables_gpdb5 AS 
    SELECT * FROM pg_stat_all_tables_new
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';


CREATE VIEW pg_stat_user_tables_gpdb5 AS 
    SELECT * FROM pg_stat_all_tables_gpdb5
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';
```

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

