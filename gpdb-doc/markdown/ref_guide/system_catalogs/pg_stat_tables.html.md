# pg_stat_all_tables 

The `pg_stat_all_tables` view shows one row for each table in the current database \(including TOAST tables\) to display statistics about accesses to that specific table.

The `pg_stat_user_tables` and `pg_stat_sys_table` views contain the same information, but filtered to only show user and system tables respectively.

In Greenplum Database 6, the `pg_stat_*_tables` views display access statistics for tables only from the master instance. Access statistics from segment instances are ignored. You can create views that display usage statistics, see [Table Access Statistics from the Master and Segment Instances](#tbl_stats_all_6x).

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
|`n_mod_since_analyze`|bigint|Estimated number of rows modified since this table was last analyzed|
|`last_vacuum`|timestamp with time zone|Last time this table was manually vacuumed \(not counting `VACUUM FULL`\)|
|`last_autovacuum`|timestamp with time zone|Last time this table was vacuumed by the autovacuum daemon<sup>1</sup>|
|`last_analyze`|timestamp with time zone|Last time this table was manually analyzed|
|`last_autoanalyze`|timestamp with time zone|Last time this table was analyzed by the autovacuum daemon<sup>1</sup>|
|`vacuum_count`|bigint|Number of times this table has been manually vacuumed \(not counting `VACUUM FULL`\)|
|`autovacuum_count`|bigint|Number of times this table has been vacuumed by the autovacuum daemon<sup>1</sup>|
|`analyze_count`|bigint|Number of times this table has been manually analyzed|
|`autoanalyze_count`|bigint|Number of times this table has been analyzed by the autovacuum daemon <sup>1</sup>|

> **Note** <sup>1</sup>In Greenplum Database, the autovacuum daemon is deactivated and not supported for user defined databases.

## <a id="tbl_stats_all_6x"></a>Table Access Statistics from the Master and Segment Instances

To display table access statistics that combine statistics from the master and the segment instances you can create these views. A user requires `SELECT` privilege on the views to use them.

```
-- Create these table access statistics views
--   pg_stat_all_tables_gpdb6
--   pg_stat_sys_tables_gpdb6
--   pg_stat_user_tables_gpdb6

CREATE VIEW pg_stat_all_tables_gpdb6 AS
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
    s.n_mod_since_analyze,
    s.last_vacuum,
    s.last_autovacuum,
    s.last_analyze,
    s.last_autoanalyze,
    s.vacuum_count,
    s.autovacuum_count,
    s.analyze_count,
    s.autoanalyze_count
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
         max(n_mod_since_analyze) as n_mod_since_analyze,
         max(last_vacuum) as last_vacuum,
         max(last_autovacuum) as last_autovacuum,
         max(last_analyze) as last_analyze,
         max(last_autoanalyze) as last_autoanalyze,
         max(vacuum_count) as vacuum_count,
         max(autovacuum_count) as autovacuum_count,
         max(analyze_count) as analyze_count,
         max(autoanalyze_count) as autoanalyze_count
     FROM gp_dist_random('pg_stat_all_tables')
     WHERE relid >= 16384
     GROUP BY relid, schemaname, relname
     UNION ALL
     SELECT *
     FROM pg_stat_all_tables
     WHERE relid &lt; 16384) m, pg_stat_all_tables s
 WHERE m.relid = s.relid;


CREATE VIEW pg_stat_sys_tables_gpdb6 AS
    SELECT * FROM pg_stat_all_tables_gpdb6
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';


CREATE VIEW pg_stat_user_tables_gpdb6 AS
    SELECT * FROM pg_stat_all_tables_gpdb6
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';
```

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)