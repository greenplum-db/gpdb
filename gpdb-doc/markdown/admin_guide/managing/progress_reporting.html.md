# Monitoring Long-Running Operations

Greenplum Database can report the progress of `ANALYZE`, `CLUSTER`, `COPY`, `CREATE INDEX`, `REINDEX`, and `VACUUM` commands during command execution, with some caveats. Greeplum can also report the progress of a running base backup (initiated during [gprecoverseg](../../utility_guide/ref/gprecoverseg.html)) or [pg_checksums](../../utility_guide/ref/pg_checksums.html) command invocation, allowing you to monitor the progress of these possibly long-running operations.

Greenplum reports the command progress via ephemeral system views, which return data only while the operations are running. Two sets of progress reporting views are provided:

- `gp_stat_progress_<command>` - displays the progress of running `<command>` invocations on the coordinator and all segments, with a row per segment instance
- `gp_stat_progress_<command>_summary` - aggregates `<command>` progress on the coordinator and all segments, and displays one row per running `<command>` invocation

Greeplum reports progress in phases, where the phases are specific to the command. For example, `acquiring sample rows` is an analyze progress phase, while `building index` is an index creation progress phase. Greenplum reports the progress for both heap and AO/CO tables. For most commands, heap and AO/CO table progress is reported together. For vacuum and cluster operations, Greenplum reports heap and AO/CO table progress in separate phases.


## <a id="analyze_progress"></a>ANALYZE Progress Reporting

The [gp_stat_progress_analyze](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_analyze) system view reports the progress of running `ANALYZE` and `analyzedb` operations. The view displays a row per segment instance that is currently servicing an analyze operation.

For each active analyze operation, the `gp_stat_progress_analyze_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_analyze`, and shares the same schema as this view.

The table below describes how to interpret the phase-specific information reported in the views:

|Phase|Description|
|-----|-----------|
| `initializing` | The command is preparing to begin scanning the heap. This phase is expected to be very brief. |
| `acquiring sample rows` | The command is currently scanning the table given by `relid` to obtain sample rows. |
| `acquiring inherited sample rows` | The command is currently scanning child tables to obtain sample rows. Columns `child_tables_total`, `child_tables_done`, and `current_child_table_relid` contain the progress information for this phase. |
| `computing statistics` | The command is computing statistics from the sample rows obtained during the table scan. |
| `computing extended statistics` | The command is computing extended statistics from the sample rows obtained during the table scan. |
| `finalizing analyze` | The command is updating `pg_class`. When this phase is completed, `ANALYZE` ends. |


## <a id="cluster_progress"></a>CLUSTER and VACUUM FULL Progress Reporting

The [gp_stat_progress_cluster](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_cluster) system view reports the progress of running `CLUSTER`, `clusterdb`, and `VACUUM FULL` operations. (`VACUUM FULL` is similar to `CLUSTER` in that Greenplum performs a re-write of the table.) The view displays a row per segment instance that is currently servicing any of the mentioned commands.

For each active cluster or vacuum full operation, the `gp_stat_progress_cluster_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_cluster`, and shares the same schema as this view.

### <a id="cluster_progress_heap"></a>Heap Table Cluster Phases

The table below describes how to interpret the *heap table* phase-specific information reported in the views:

|Phase|Description|
|-----|-----------|
| `initializing` | The command is preparing to begin scanning the heap. This phase is expected to be very brief. |
| `seq scanning heap` | The command is currently scanning the table using a sequential scan. |
| `index scanning heap` | `CLUSTER` is currently scanning the table using an index scan. |
| `sorting tuples` | `CLUSTER` is currently sorting tuples. |
| `writing new heap` | `CLUSTER` is currently writing the new heap. |
| `swapping relation files` | The command is currently swapping newly-built files into place. |
| `rebuilding index` | The command is currently rebuilding an index. |
| `performing final cleanup` | The command is performing final cleanup. When this phase is completed, `CLUSTER` or `VACUUM FULL` ends. |

### <a id="cluster_progress_ao"></a>AO/CO Table Cluster Phases

The table below describes how to interpret the *AO/CO table* phase-specific information reported in the views:

|Phase|Description|
|-----|-----------|
| `initializing` | The command is preparing to begin scanning. This phase is expected to be very brief. |
| `seq scanning append-optimized` | The command is currently scanning the AO/CO table using a sequential scan. |
| `index scanning heap` | `CLUSTER` is currently scanning the table using an index scan. |
| `sorting tuples` | `CLUSTER` is currently sorting tuples. |
| `writing new append-optimized` | `CLUSTER` is currently writing the new AO/CO table. |
| `swapping relation files` | The command is currently swapping newly-built files into place. |
| `performing final cleanup` | The command is performing final cleanup. When this phase is completed, `CLUSTER` or `VACUUM FULL` ends. |

## <a id="copy_progress"></a>COPY Progress Reporting

The [gp_stat_progress_copy](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_copy) system view reports the progress of running `COPY` operations. The view displays a row per segment instance that is currently servicing a copy operation.

For each active copy operation, the `gp_stat_progress_copy_summary` aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_copy`, and shares the same schema as this view.

You can also use these views to monitor the data movement progress of utilities that use `COPY` under the hood, such as XXX.

Greenplum Database calculates the `bytes_processed`, `bytes_total`, `tuples_processed`, and `tuples_excluded` column values differently for the `gp_stat_progress_copy_summary` view depending on the type of table and type of `COPY` operation. The table below identifies the table types, the types of `COPY` operations, and which of `sum()` or `average()` Greenplum Database uses to calculate the final value:

| Table type |COPY TO | COPY FROM | COPY TO/FROM ON SEGMENT |
|-----|-----------|------|------|
| Distributed table | `sum()` | `sum()` | `sum()` |
| Replicated table | `sum()` | `average()` | `sum()` |

Greenplum uses `sum()` for `COPY ... ON SEGMENT` as the command explicitly operates on each segment irrespective of the table type.

Greenplum uses `sum()` for `COPY TO` with replicated tables, as the actual copy originates from only a single segment.


## <a id="create_index_progress"></a>CREATE INDEX Progress Reporting

The [gp_stat_progress_create_index](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_create_index) system view reports the progress of running `CREATE INDEX` and `REINDEX` operations. The view displays a row per segment instance that is currently servicing either command.

For each active index operation, the `gp_stat_progress_create_index_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_create_index`, and shares the same schema as this view. 

The table below describes how to interpret the phase-specific information reported in the views:

|Phase|Description|
|-----|-----------|
| `initializing` | `CREATE INDEX` or `REINDEX` is preparing to create the index. This phase is expected to be very brief. |
| `waiting for writers before build` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `building index` | The index is being built by the access method-specific code. In this phase, access methods that support progress reporting fill in their own progress data, and the subphase is indicated in this column. Typically, `blocks_total` and `blocks_done` will contain progress data, as well as potentially `tuples_total` and `tuples_done`. |
| `waiting for writers before validation` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `index validation: scanning index` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `index validation: sorting tuples` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `index validation: scanning table` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `waiting for old snapshots` | This phase is not applicable, Greenplum Database does not support `CREATE INDEX CONCURRENTLY` or `REINDEX CONCURRENTLY`. |
| `waiting for readers before marking dead` | This phase is not applicable, Greenplum Database does not support `REINDEX CONCURRENTLY`. |
| `waiting for readers before dropping` | This phase is not applicable, Greenplum Database does not support `REINDEX CONCURRENTLY`. |

Greenplum Database skips several phases because it does not support concurrent index creation or concurrent reindexing.

## <a id="vacuum_progress"></a>VACUUM Progress Reporting

The [gp_stat_progress_vacuum](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_vacuum) system view reports the progress of running `VACUUM` and `vacuumdb` operations. The view displays a row per segment instance that is currently servicing a vacuum operation.

For each active vacuum operation, the `gp_stat_progress_vacuum_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_vacuum`, and shares the same schema as this view.

(Regular `VACUUM` modifies the table in place. `VACUUM FULL` rewrites the table as does a `CLUSTER` operation. For information about progress reporting for `VACUUM FULL`, see [CLUSTER and VACUUM FULL Progress Reporting](#cluster_progress).)

In Greenplum Database, an AO/CO table vacuum behaves differently than a heap table vacuum. Because Greenplum stores the logical EOF for each segment file, it does not need to scan physical blocks after the logical EOF, so Greenplum can truncate them. Due to this difference, Greenplum reports vacuum progress on heap and AO/CO tables using different phases.

### <a id="vacuum_progress_heap"></a>Heap Table Vacuum Phases

The table below describes how to interpret the *heap table* phase-specific information reported in the views:

|Heap Table Phase|Description|
|-----|-----------|
| `initializing` | `VACUUM` is preparing to begin scanning the heap. This phase is expected to be very brief. |
| `scanning heap` | `VACUUM` is currently scanning the heap. It prunes and defragment seach page if required, and possibly performs freezing activity. Use the `heap_blks_scanned` column to monitor the progress of the scan. |
| `vacuuming indexes` | `VACUUM` is currently vacuuming the indexes. If a table has any indexes, this will happen at least once per vacuum, after the heap has been completely scanned. It may happen multiple times per vacuum if `maintenance_work_mem` (or, in the case of autovacuum, `autovacuum_work_mem` if set) is insufficient to store the number of dead tuples found. |
| `vacuuming heap` | `VACUUM` is currently vacuuming the heap. Vacuuming the heap is distinct from scanning the heap, and occurs after each instance of vacuuming indexes. If `heap_blks_scanned` is less than `heap_blks_total`, the system returns to scanning the heap after this phase is completed; otherwise, it begins cleaning up indexes after this phase is completed. |
| `cleaning up indexes` | `VACUUM` is currently cleaning up indexes. This occurs after the heap has been completely scanned and all vacuuming of the indexes and the heap has been completed. |
| `truncating heap` | `VACUUM` is currently truncating the heap so as to return empty pages at the end of the relation to the operating system. This occurs after cleaning up indexes. |
| `performing final cleanup` | `VACUUM` is performing final cleanup. During this phase, `VACUUM` will vacuum the free space map, update statistics in `pg_class`, and report statistics to the statistics collector. When this phase is completed, the `VACUUM` operation ends. |

### <a id="vacuum_progress_ao"></a>AO/CO Table Vacuum Phases

The table below describes how to interpret the *AO/CO table* phase-specific information reported in the views:

|AO/CO Table Phase|Description|
|-----|-----------|
| `initializing` | `VACUUM` is preparing to begin scanning AO/CO tables. This phase is expected to be very brief. |
| `vacuuming indexes` | For AO/CO tables, the `vacuuming indexes` phase is a sub-phase that may occur during both `append-optimized pre-cleanup` and `append-optimized post-cleanup` phases if the relation has an index and there are invisible awaiting-drop segment files. |
| `append-optimized pre-cleanup` | XXX |
| `append-optimized compact` | XXX |
| `append-optimized post-cleanup` | XXX |


## <a id="basebackup_progress"></a>Base Backup Progress Reporting

The [gp_stat_progress_basebackup](../../ref_guide/system_catalogs/catalog_ref-views.html#gp_stat_progress_basebackup) system view reports the progress of running base backup operations, as is performed by `gpcoverseg`. The view displays a row per segment instance that is currently servicing replication commands.

For each active base backup operation, the `gp_stat_progress_basebackup_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_basebackup`, and shares the same schema as this view.

The table below describes how to interpret the phase-specific information reported in the views:

|Phase|Description|
|-----|-----------|
| `initializing` | The WAL sender process is preparing to begin the backup. This phase is expected to be very brief. |
| `waiting for checkpoint to finish` | The WAL sender process is currently performing `pg_backup_start` to prepare to take a base backup, and waiting for the start-of-backup checkpoint to finish. |
| `estimating backup size` | The WAL sender process is currently estimating the total amount of database files that will be streamed as a base backup. |
| `streaming database files` | The WAL sender process is currently streaming database files as a base backup. |
| `waiting for wal archiving to finish` | The WAL sender process is currently performing `pg_backup_stop` to finish the backup, and waiting for all the WAL files required for the base backup to be successfully archived. The backup ends when this phase is completed. |
| `transferring wal files` | The WAL sender process is currently transferring all WAL logs generated during the backup. This phase may occur after `waiting for wal archiving to finish` phase. The backup ends when this phase is completed. |


## <a id="pg_checksums_progress"></a>pg_checksums Progress Reporting

To obtain run-time progress when enabling, disabling, or checking data checksums in a database, you can provide the `-P/--progress` option to the [pg_checksums](../../utility_guide/ref/pg_checksums.html) Greenplum Database utility command.

## <a id="realtime_progress"></a>Example: Viewing Real-Time Command Progress

The following example commands display the real-time progress of all running `VACUUM` commands every 0.5 seconds:

```
host$ cat > viewer.sql << EOF
SELECT * FROM gp_stat_progress_vacuum ORDER BY gp_segment_id;
SELECT * FROM gp_stat_progress_vacuum_summary;
EOF

host$ watch -n 0.5 "psql -af viewer.sql"
```

## <a id="limitations"></a>Limitations and Known Issues

Progress reporting in Greenplum Database has these known issues and limitations:

- ANY???

