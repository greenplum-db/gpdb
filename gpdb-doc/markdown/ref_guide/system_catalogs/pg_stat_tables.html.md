# pg_stat_all_tables 

The `pg_stat_all_tables` view shows one row for each table in the current database \(including TOAST tables\) to display statistics about accesses to that specific table.

The `pg_stat_user_tables` and `pg_stat_sys_table` views contain the same information, but filtered to only show user and system tables respectively.

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

**Note:** <sup>1</sup>In Greenplum Database, the autovacuum daemon is deactivated and not supported for user defined databases.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

