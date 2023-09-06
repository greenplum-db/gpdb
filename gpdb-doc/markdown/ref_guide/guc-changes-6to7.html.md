## <a id="guc_changes"></a>Server Configuration Parameter Changes from Greenplum 6 to 7

Greenplum 7 includes new server configuration parameters, and removes or updates the default values of certain server configuration parameters as described below.

### <a id="new"></a>New Parameters

The following new server configuration parameters are available in Greenplum 7:

- `autovacuum_freeze_max_age`
- `autovacuum_vacuum_cost_delay`
- `autovacuum_vacuum_scale_factor` 
- `autovacuum_vacuum_threshold`
- `gp_autovacuum_scope`
- `gp_quicklz_fallback`
- `vacuum_cleanup_index_scale_factor`
- `default_table_access_method`
- `enable_partition_pruning`
- `gp_explain_jit`
- `jit`
- `jit_above_cost`
- `jit_debugging_support`
- `jit_dump_bitcode`
- `jit_expressions`
- `jit_inline_above_cost`
- `jit_optimize_above_cost`
- `jit_profiling_support`
- `jit_provider`
- `jit_tuple_deforming`
- `optimizer_jit_above_cost`
- `optimizer_jit_inline_above_cost`
- `optimizer_jit_optimize_above_cost`
- `vacuum_cleanup_index_scale_factor`
- `track_wal_io_timing`
- `gp_resgroup_memory_query_fixed_mem`
- `gp_resource_group_bypass_direct_dispatch`


### <a id="removed"></a>Removed Parameters

The following server configuration parameters are removed in Greenplum 7:

- `checkpoint_segments`
- `gp_hashagg_default_nbatches`
- `gp_hashagg_groups_per_bucket`
- `gp_resource_group_enable_recalculate_query_mem`
- `gp_resource_group_memory_limit`
- `gp_resource_group_cpu_ceiling_enforcement`
- `debug_latch`
- `gp_eager_dqa_pruning`
- `gp_eager_one_phase_agg`
- `gp_enable_exchange_default_partition`
- `gp_enable_gpperfmon`
- `gp_enable_sort_distinct`
- `gp_gpperfmon_send_interval`
- `gp_hashagg_streambottom`
- `gp_perfmon_print_packet_info` (debug)
- `gpperfmon_log_alert_level`
- `gpperfmon_port`
- `max_appendonly_tables`
- `optimizer_enable_dml_triggers`
- `password_hash_algorithm`

### <a id="changed"></a>Changed Parameters

These server configuration parameters are changed in Greenplum 7:

- The server configuration parameter `vacuum_cost_page_miss` has a new default value of 2.
- The default value for the server configuration parameter `gp_interconnect_address_type` changed from `wildcard` to `unicast`.
- The `wal_keep_segments` server configuration parameter has been replaced by the `wal_keep_size` parameter.
- The `autovacuum` server configuration parameter is now enabled for all databases by default, rather than just for the `template0` and `template1` databases.
- The `gp_autostats_mode` server configuration parameter default value has been changed to `none`.
