# pg_stat_wal 

The view `pg_stat_wal` shows data about the WAL activity of the cluster. It contains always a single row.

|column|type|references|description|
|------|----|----------|-----------|
|`wal_records`|bigint| |Total number of WAL records generated.|
|`wal_fpw`|bigint| |Total number of WAL full page images generated.|
|`wal_bytes`|numeric| |Total amount of WAL generated in bytes.|
|`wal_buffers_full`|bigint| |Number of times WAL data was written to disk because WAL buffers became full.|
|`wal_write`|bigint| |Number of times WAL buffers were written out to disk.|
|`wal_sync`|bigint||Number of times WAL files were synced to disk.|
|`wal_write_time`|double precision| |Total amount of time spent writing WAL buffers to disk, in milliseconds (if [track_wal_io_timing](../config_params/guc-list.html#track_wal_io_timing) is enabled, otherwise zero).|
|`wal_sync_time`|double precision| |Total amount of time spent syncing WAL files to disk, in milliseconds (if `track_wal_io_timing` is enabled, otherwise zero).|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

