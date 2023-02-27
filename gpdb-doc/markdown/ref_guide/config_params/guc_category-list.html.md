# Parameter Categories 

Configuration parameters affect categories of server behaviors, such as resource consumption, query tuning, and authentication. The following topics describe Greenplum configuration parameter categories.

-   [Connection and Authentication Parameters](#topic12)
-   [System Resource Consumption Parameters](#topic15)
-   [GPORCA Parameters](#topic57)
-   [Query Tuning Parameters](#topic21)
-   [Error Reporting and Logging Parameters](#topic29)
-   [Runtime Statistics Collection Parameters](#topic37)
-   [Automatic Vacuum Parameters](#automatic_vacuum)
-   [Automatic Statistics Collection Parameters](#topic38)
-   [Client Connection Default Parameters](#topic39)
-   [Lock Management Parameters](#topic43)
-   [Resource Management Parameters \(Resource Queues\)](#topic44)
-   [Resource Management Parameters \(Resource Groups\)](#topic444)
-   [External Table Parameters](#topic45)
-   [Database Table Parameters](#topic46)
-   [Past Version Compatibility Parameters](#topic48)
-   [Greenplum Database Array Configuration Parameters](#topic49)
-   [Greenplum Mirroring Parameters for Coordinator and Segments](#topic55)
-   [Greenplum PL/Java Parameters](#topic56)

## <a id="topic12"></a>Connection and Authentication Parameters 

These parameters control how clients connect and authenticate to Greenplum Database.

### <a id="topic13"></a>Connection Parameters 

- [client_connection_check_interval](guc-list.html#client_connection_check_interval)
- [gp_connection_send_timeout](guc-list.html#gp_connection_send_timeout)
- [gp_dispatch_keepalives_count](guc-list.html#gp_dispatch_keepalives_count)
- [gp_dispatch_keepalives_idle](guc-list.html#gp_dispatch_keepalives_idle)
- [gp_dispatch_keepalives_interval](guc-list.html#gp_dispatch_keepalives_interval)
- [gp_vmem_idle_resource_timeout](guc-list.html#gp_vmem_idle_resource_timeout)
- [listen_addresses](guc-list.html#listen_addresses)
- [max_connections](guc-list.html#max_connections)
- [max_prepared_transactions](guc-list.html#max_prepared_transactions)
- [superuser_reserved_connections](guc-list.html#superuser_reserved_connections)
- [tcp_keepalives_count](guc-list.html#tcp_keepalives_count)
- [tcp_keepalives_idle](guc-list.html#tcp_keepalives_idle)
- [tcp_keepalives_interval](guc-list.html#tcp_keepalives_interval)
- [unix_socket_directories](guc-list.html#unix_socket_directories)
- [unix_socket_group](guc-list.html#unix_socket_group)
- [unix_socket_permissions](guc-list.html#unix_socket_permissions)

### <a id="topic14"></a>Security and Authentication Parameters 

- [authentication_timeout](guc-list.html#authentication_timeout)
- [db_user_namespace](guc-list.html#db_user_namespace)
- [krb_caseins_users](guc-list.html#krb_caseins_users)
- [krb_server_keyfile](guc-list.html#krb_server_keyfile)
- [password_encryption](guc-list.html#password_encryption)
- [row_security](guc-list.html#row_security)
- [ssl](guc-list.html#ssl)
- [ssl_ciphers](guc-list.html#ssl_ciphers)

## <a id="topic15"></a>System Resource Consumption Parameters 

These parameters set the limits for system resources consumed by Greenplum Database.

### <a id="topic16"></a>Memory Consumption Parameters 

These parameters control system memory usage.

- [gp_vmem_idle_resource_timeout](guc-list.html#gp_vmem_idle_resource_timeout)
- [gp_resource_group_memory_limit](guc-list.html#gp_resource_group_memory_limit) \(resource group-based resource management\)
- [gp_vmem_protect_limit](guc-list.html#gp_vmem_protect_limit) \(resource queue-based resource management\)
- [gp_vmem_protect_segworker_cache_limit](guc-list.html#gp_vmem_protect_segworker_cache_limit)
- [gp_workfile_limit_files_per_query](guc-list.html#gp_workfile_limit_files_per_query)
- [gp_workfile_limit_per_query](guc-list.html#gp_workfile_limit_per_query)
- [gp_workfile_limit_per_segment](guc-list.html#gp_workfile_limit_per_segment)
- [maintenance_work_mem](guc-list.html#maintenance_work_mem)
- [max_stack_depth](guc-list.html#max_stack_depth)
- [shared_buffers](guc-list.html#shared_buffers)
- [temp_buffers](guc-list.html#temp_buffers)

### <a id="topic18"></a>OS Resource Parameters 

- [max_files_per_process](guc-list.html#max_files_per_process)
- [shared_preload_libraries](guc-list.html#shared_preload_libraries)

### <a id="topic19"></a>Cost-Based Vacuum Delay Parameters 

> **Caution** Do not use cost-based vacuum delay because it runs asynchronously among the segment instances. The vacuum cost limit and delay is invoked at the segment level without taking into account the state of the entire Greenplum Database array

You can configure the execution cost of `VACUUM` and `ANALYZE` commands to reduce the I/O impact on concurrent database activity. When the accumulated cost of I/O operations reaches the limit, the process performing the operation sleeps for a while, Then resets the counter and continues execution

- [vacuum_cost_delay](guc-list.html#vacuum_cost_delay)
- [vacuum_cost_limit](guc-list.html#vacuum_cost_limit)
- [vacuum_cost_page_dirty](guc-list.html#vacuum_cost_page_dirty)
- [vacuum_cost_page_hit](guc-list.html#vacuum_cost_page_hit)
- [vacuum_cost_page_miss](guc-list.html#vacuum_cost_page_miss)

### <a id="topic20"></a>Transaction ID Management Parameters 

- [xid_stop_limit](guc-list.html#xid_stop_limit)
- [xid_warn_limit](guc-list.html#xid_warn_limit)

### <a id="topic20other"></a>Other Parameters 

- [gp\_max\_parallel\_cursors](guc-list.html#gp_max_parallel_cursors)

## <a id="topic57"></a>GPORCA Parameters 

These parameters control the usage of GPORCA by Greenplum Database. For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html) in the *Greenplum Database Administrator Guide*.

- [gp_enable_relsize_collection](guc-list.html#gp_enable_relsize_collection)
- [optimizer](guc-list.html#optimizer)
- [optimizer_analyze_root_partition](guc-list.html#optimizer_analyze_root_partition)
- [optimizer_array_expansion_threshold](guc-list.html#optimizer_array_expansion_threshold)
- [optimizer_control](guc-list.html#optimizer_control)
- [optimizer_cost_model](guc-list.html#optimizer_cost_model)
- [optimizer_cte_inlining_bound](guc-list.html#optimizer_cte_inlining_bound)
- [optimizer_dpe_stats](guc-list.html#optimizer_dpe_stats)
- [optimizer_discard_redistribute_hashjoin](guc-list.html#optimizer_discard_redistribute_hashjoin)
- [optimizer_enable_associativity](guc-list.html#optimizer_enable_associativity)
- [optimizer_enable_dml](guc-list.html#optimizer_enable_dml)
- [optimizer_enable_indexonlyscan](guc-list.html#optimizer_enable_indexonlyscan)
- [optimizer_enable_master_only_queries](guc-list.html#optimizer_enable_master_only_queries)
- [optimizer_enable_multiple_distinct_aggs](guc-list.html#optimizer_enable_multiple_distinct_aggs)
- [optimizer_enable_replicated_table](guc-list.html#optimizer_enable_replicated_table)
- [optimizer_force_agg_skew_avoidance](guc-list.html#optimizer_force_agg_skew_avoidance)
- [optimizer_force_comprehensive_join_implementation](guc-list.html#optimizer_force_comprehensive_join_implementation)
- [optimizer_force_multistage_agg](guc-list.html#optimizer_force_multistage_agg)
- [optimizer_force_three_stage_scalar_dqa](guc-list.html#optimizer_force_three_stage_scalar_dqa)
- [optimizer_join_arity_for_associativity_commutativity](guc-list.html#optimizer_join_arity_for_associativity_commutativity)
- [optimizer_join_order](guc-list.html#optimizer_join_order)
- [optimizer_join_order_threshold](guc-list.html#optimizer_join_order_threshold)
- [optimizer_mdcache_size](guc-list.html#optimizer_mdcache_size)
- [optimizer_metadata_caching](guc-list.html#optimizer_metadata_caching)
- [optimizer_parallel_union](guc-list.html#optimizer_parallel_union)
- [optimizer_penalize_skew](guc-list.html#optimizer_penalize_skew)
- [optimizer_print_missing_stats](guc-list.html#optimizer_print_missing_stats)
- [optimizer_print_optimization_stats](guc-list.html#optimizer_print_optimization_stats)
- [optimizer_skew_factor](guc-list.html#optimizer_skew_factor)
- [optimizer_sort_factor](guc-list.html#optimizer_sort_factor)
- [optimizer_use_gpdb_allocators](guc-list.html#optimizer_use_gpdb_allocators)
- [optimizer_xform_bind_threshold](guc-list.html#optimizer_xform_bind_threshold)

## <a id="topic21"></a>Query Tuning Parameters 

These parameters control aspects of SQL query processing such as query operators and operator settings and statistics sampling.

### <a id="topic22"></a>Postgres Planner Control Parameters 

The following parameters control the types of plan operations the Postgres Planner can use. Enable or deactivate plan operations to force the Postgres Planner to choose a different plan. This is useful for testing and comparing query performance using different plan types.

- [enable_bitmapscan](guc-list.html#enable_bitmapscan)
- [enable_groupagg](guc-list.html#enable_groupagg)
- [enable_hashagg](guc-list.html#enable_hashagg)
- [enable_hashjoin](guc-list.html#enable_hashjoin)
- [enable_indexscan](guc-list.html#enable_indexscan)
- [enable_mergejoin](guc-list.html#enable_mergejoin)
- [enable_nestloop](guc-list.html#enable_nestloop)
- [enable_partition_pruning](guc-list.html#enable_partition_pruning)
- [enable_seqscan](guc-list.html#enable_seqscan)
- [enable_sort](guc-list.html#enable_sort)
- [enable_tidscan](guc-list.html#enable_tidscan)
- [gp_eager_two_phase_agg](guc-list.html#gp_eager_two_phase_agg)
- [gp_enable_agg_distinct](guc-list.html#gp_enable_agg_distinct)
- [gp_enable_agg_distinct_pruning](guc-list.html#gp_enable_agg_distinct_pruning)
- [gp_enable_direct_dispatch](guc-list.html#gp_enable_direct_dispatch)
- [gp_enable_fast_sri](guc-list.html#gp_enable_fast_sri)
- [gp_enable_groupext_distinct_gather](guc-list.html#gp_enable_groupext_distinct_gather)
- [gp_enable_groupext_distinct_pruning](guc-list.html#gp_enable_groupext_distinct_pruning)
- [gp_enable_multiphase_agg](guc-list.html#gp_enable_multiphase_agg)
- [gp_enable_predicate_propagation](guc-list.html#gp_enable_predicate_propagation)
- [gp_enable_preunique](guc-list.html#gp_enable_preunique)
- [gp_enable_relsize_collection](guc-list.html#gp_enable_relsize_collection)
- [gp_enable_sort_limit](guc-list.html#gp_enable_sort_limit)

### <a id="topic23"></a>Postgres Planner Costing Parameters 

> **Caution** Do not adjust these query costing parameters. They are tuned to reflect Greenplum Database hardware configurations and typical workloads. All of these parameters are related. Changing one without changing the others can have adverse affects on performance.

- [cpu_index_tuple_cost](guc-list.html#cpu_index_tuple_cost)
- [cpu_operator_cost](guc-list.html#cpu_operator_cost)
- [cpu_tuple_cost](guc-list.html#cpu_tuple_cost)
- [cursor_tuple_fraction](guc-list.html#cursor_tuple_fraction)
- [effective_cache_size](guc-list.html#effective_cache_size)
- [gp_motion_cost_per_row](guc-list.html#gp_motion_cost_per_row)
- [gp_segments_for_planner](guc-list.html#gp_segments_for_planner)
- [random_page_cost](guc-list.html#random_page_cost)
- [seq_page_cost](guc-list.html#seq_page_cost)

### <a id="topic24"></a>Database Statistics Sampling Parameters 

These parameters adjust the amount of data sampled by an `ANALYZE` operation. Adjusting these parameters affects statistics collection system-wide. You can configure statistics collection on particular tables and columns by using the `ALTER TABLE SET STATISTICS` clause.

- [default_statistics_target](guc-list.html#default_statistics_target)

### <a id="topic25"></a>Sort Operator Configuration Parameters 

- [gp_enable_sort_limit](guc-list.html#gp_enable_sort_limit)

### <a id="topic26"></a>Aggregate Operator Configuration Parameters 

- [gp_enable_agg_distinct](guc-list.html#gp_enable_agg_distinct)
- [gp_enable_agg_distinct_pruning](guc-list.html#gp_enable_agg_distinct_pruning)
- [gp_enable_multiphase_agg](guc-list.html#gp_enable_multiphase_agg)
- [gp_enable_preunique](guc-list.html#gp_enable_preunique)
- [gp_enable_groupext_distinct_gather](guc-list.html#gp_enable_groupext_distinct_gather)
- [gp_enable_groupext_distinct_pruning](guc-list.html#gp_enable_groupext_distinct_pruning)
- [gp_workfile_compression](guc-list.html#gp_workfile_compression)

### <a id="topic27"></a>Join Operator Configuration Parameters 

- [join_collapse_limit](guc-list.html#join_collapse_limit)
- [gp_adjust_selectivity_for_outerjoins](guc-list.html#gp_adjust_selectivity_for_outerjoins)
- [gp_hashjoin_tuples_per_bucket](guc-list.html#gp_hashjoin_tuples_per_bucket)
- [gp_workfile_compression](guc-list.html#gp_workfile_compression)

### <a id="topic28"></a>Other Postgres Planner Configuration Parameters 

- [from_collapse_limit](guc-list.html#from_collapse_limit)
- [gp_enable_predicate_propagation](guc-list.html#gp_enable_predicate_propagation)
- [gp_max_plan_size](guc-list.html#gp_max_plan_size)
- [gp_statistics_pullup_from_child_partition](guc-list.html#gp_statistics_pullup_from_child_partition)
- [gp_statistics_use_fkeys](guc-list.html#gp_statistics_use_fkeys)

### <a id="topic_zd5_p32_mdb"></a>Query Plan Execution 

Control the query plan execution.

- [gp_max_slices](guc-list.html#gp_max_slices)
- [plan_cache_mode](guc-list.html#plan_cache_mode)

### <a id="topic_jit"></a>JIT Configuration Parameters

- [gp_explain_jit](guc-list.html#gp_explain_jit)
- [jit](guc-list.html#jit)
- [jit_above_cost](guc-list.html#jit_above_cost)
- [jit_debugging_support](guc-list.html#jit_debugging_support)
- [jit_dump_bitcode](guc-list.html#jit_dump_bitcode)
- [jit_expressions](guc-list.html#jit_expressions)
- [jit_inline_above_cost](guc-list.html#jit_inline_above_cost)
- [jit_optimize_above_cost](guc-list.html#jit_optimize_above_cost)
- [jit_profiling_support](guc-list.html#jit_profiling_support)
- [jit_provider](guc-list.html#jit_provider)
- [jit_tuple_deforming](guc-list.html#jit_tuple_deforming)

## <a id="topic29"></a>Error Reporting and Logging Parameters 

These configuration parameters control Greenplum Database logging.

### <a id="topic30"></a>Log Rotation 

- [log_rotation_age](guc-list.html#log_rotation_age)
- [log_rotation_size](guc-list.html#log_rotation_size)
- [log_truncate_on_rotation](guc-list.html#log_truncate_on_rotation)

### <a id="topic31"></a>When to Log 

- [client_min_messages](guc-list.html#client_min_messages)
- [gp_interconnect_debug_retry_interval](guc-list.html#gp_interconnect_debug_retry_interval)
- [log_error_verbosity](guc-list.html#log_error_verbosity)
- [log_file_mode](guc-list.html#log_file_mode)
- [log_min_duration_statement](guc-list.html#log_min_duration_statement)
- [log_min_error_statement](guc-list.html#log_min_error_statement)
- [log_min_messages](guc-list.html#log_min_messages)
- [optimizer_minidump](guc-list.html#optimizer_minidump)

### <a id="topic32"></a>What to Log 

- [debug_pretty_print](guc-list.html#debug_pretty_print)
- [debug_print_parse](guc-list.html#debug_print_parse)
- [debug_print_plan](guc-list.html#debug_print_plan)
- [debug_print_prelim_plan](guc-list.html#debug_print_prelim_plan)
- [debug_print_rewritten](guc-list.html#debug_print_rewritten)
- [debug_print_slice_table](guc-list.html#debug_print_slice_table)
- [log_autostats](guc-list.html#log_autostats)
- [log_connections](guc-list.html#log_connections)
- [log_disconnections](guc-list.html#log_disconnections)
- [log_dispatch_stats](guc-list.html#log_dispatch_stats)
- [log_duration](guc-list.html#log_duration)
- [log_executor_stats](guc-list.html#log_executor_stats)
- [log_hostname](guc-list.html#log_hostname)
- [gp_log_endpoints](guc-list.html#gp_log_endpoints)
- [gp_log_interconnect](guc-list.html#gp_log_interconnect)
- [gp_print_create_gang_time](guc-list.html#gp_print_create_gang_time)
- [log_parser_stats](guc-list.html#log_parser_stats)
- [log_planner_stats](guc-list.html#log_planner_stats)
- [log_statement](guc-list.html#log_statement)
- [log_statement_stats](guc-list.html#log_statement_stats)
- [log_timezone](guc-list.html#log_timezone)
- [gp_debug_linger](guc-list.html#gp_debug_linger)
- [gp_log_format](guc-list.html#gp_log_format)
- [gp_reraise_signal](guc-list.html#gp_reraise_signal)


## <a id="automatic_vacuum"></a>Automatic Vacuum Parameters 

These parameters pertain to auto-vacuuming databases.

- [autovacuum](guc-list.html#autovacuum)

## <a id="query-metrics"></a>Query Metrics Collection Parameters 

These parameters enable and configure query metrics collection. When enabled, Greenplum Database saves metrics to shared memory during query execution. These metrics are used by VMware Greenplum Command Center, which is included with VMware's commercial version of Greenplum Database.

- [gp_enable_query_metrics](guc-list.html#gp_enable_query_metrics)
- [gp_instrument_shmem_size](guc-list.html#gp_instrument_shmem_size)

## <a id="topic37"></a>Runtime Statistics Collection Parameters 

These parameters control the server statistics collection feature. When statistics collection is enabled, you can access the statistics data using the *pg\_stat* family of system catalog views.

- [stats_queue_level](guc-list.html#stats_queue_level)
- [track_activities](guc-list.html#track_activities)
- [track_counts](guc-list.html#track_counts)
- [update_process_title](guc-list.html#update_process_title)

## <a id="topic38"></a>Automatic Statistics Collection Parameters 

When automatic statistics collection is enabled, you can run `ANALYZE` automatically in the same transaction as an `INSERT`, `UPDATE`, `DELETE`, `COPY` or `CREATE TABLE...AS SELECT` statement when a certain threshold of rows is affected \(`on_change`\), or when a newly generated table has no statistics \(`on_no_stats`\). To enable this feature, set the following server configuration parameters in your Greenplum Database coordinator `postgresql.conf` file and restart Greenplum Database:

- [gp_autostats_allow_nonowner](guc-list.html#gp_autostats_allow_nonowner)
- [gp_autostats_mode](guc-list.html#gp_autostats_mode)
- [gp_autostats_mode_in_functions](guc-list.html#gp_autostats_mode_in_functions)
- [gp_autostats_on_change_threshold](guc-list.html#gp_autostats_on_change_threshold)
- [log_autostats](guc-list.html#log_autostats)

> **Caution** Depending on the specific nature of your database operations, automatic statistics collection can have a negative performance impact. Carefully evaluate whether the default setting of `on_no_stats` is appropriate for your system.

## <a id="topic39"></a>Client Connection Default Parameters 

These configuration parameters set defaults that are used for client connections.

### <a id="topic40"></a>Statement Behavior Parameters 

- [check_function_bodies](guc-list.html#check_function_bodies)
- [default_tablespace](guc-list.html#default_tablespace)
- [default_transaction_deferrable](guc-list.html#default_transaction_deferrable)
- [default_transaction_isolation](guc-list.html#default_transaction_isolation)
- [default_transaction_read_only](guc-list.html)[search_path](guc-list.html#default_transaction_read_only](guc-list.html)[search_path)
- [gin_pending_list_limit](guc-list.html#gin_pending_list_limit)
- [statement_timeout](guc-list.html#statement_timeout)
- [temp_tablespaces](guc-list.html#temp_tablespaces)
- [vacuum_cleanup_index_scale_factor](guc-list.html#vacuum_cleanup_index_scale_factor)
- [vacuum_freeze_min_age](guc-list.html#vacuum_freeze_min_age)

### <a id="topic41"></a>Locale and Formatting Parameters 

- [client_encoding](guc-list.html#client_encoding)
- [DateStyle](guc-list.html#DateStyle)
- [extra_float_digits](guc-list.html#extra_float_digits)
- [IntervalStyle](guc-list.html#IntervalStyle)
- [lc_collate](guc-list.html#lc_collate)
- [lc_ctype](guc-list.html#lc_ctype)
- [lc_messages](guc-list.html#lc_messages)
- [lc_monetary](guc-list.html#lc_monetary)
- [lc_numeric](guc-list.html#lc_numeric)
- [lc_time](guc-list.html#lc_time)
- [TimeZone](guc-list.html#TimeZone)

### <a id="topic42"></a>Other Client Default Parameters 

- [dynamic_library_path](guc-list.html#dynamic_library_path)
- [explain_pretty_print](guc-list.html#explain_pretty_print)
- [local_preload_libraries](guc-list.html#local_preload_libraries)

## <a id="topic43"></a>Lock Management Parameters 

These configuration parameters set limits for locks and deadlocks.

- [deadlock_timeout](guc-list.html#deadlock_timeout)
- [gp_enable global_deadlock_detector](guc-list.html)[gp_global_deadlock_detector_period](guc-list.html#gp_enable global_deadlock_detector](guc-list.html)[gp_global_deadlock_detector_period)
- [lock_timeout](guc-list.html#lock_timeout)
- [max_locks_per_transaction](guc-list.html#max_locks_per_transaction)

## <a id="topic44"></a>Resource Management Parameters \(Resource Queues\) 

The following configuration parameters configure the Greenplum Database resource management feature \(resource queues\), query prioritization, memory utilization and concurrency control.

- [gp_resqueue_memory_policy](guc-list.html#gp_resqueue_memory_policy)
- [gp_resqueue_priority](guc-list.html#gp_resqueue_priority)
- [gp_resqueue_priority_cpucores_per_segment](guc-list.html#gp_resqueue_priority_cpucores_per_segment)
- [gp_resqueue_priority_sweeper_interval](guc-list.html#gp_resqueue_priority_sweeper_interval)
- [gp_vmem_idle_resource_timeout](guc-list.html#gp_vmem_idle_resource_timeout)
- [gp_vmem_protect_limit](guc-list.html#gp_vmem_protect_limit)
- [gp_vmem_protect_segworker_cache_limit](guc-list.html#gp_vmem_protect_segworker_cache_limit)
- [max_resource_queues](guc-list.html#max_resource_queues)
- [max_resource_portals_per_transaction](guc-list.html#max_resource_portals_per_transaction)
- [max_statement_mem](guc-list.html#max_statement_mem)
- [resource_cleanup_gangs_on_wait](guc-list.html#resource_cleanup_gangs_on_wait)
- [resource_select_only](guc-list.html#resource_select_only)
- [runaway_detector_activation_percent](guc-list.html#runaway_detector_activation_percent)
- [statement_mem](guc-list.html#statement_mem)
- [stats_queue_level](guc-list.html#stats_queue_level)
- [vmem_process_interrupt](guc-list.html#vmem_process_interrupt)

## <a id="topic444"></a>Resource Management Parameters \(Resource Groups\) 

The following parameters configure the Greenplum Database resource group workload management feature.

- [gp_resgroup_memory_policy](guc-list.html#gp_resgroup_memory_policy)
- [gp_resource_group_bypass](guc-list.html) [gp_resource_group_cpu_ceiling_enforcement](guc-list.html#gp_resource_group_bypass](guc-list.html) [gp_resource_group_cpu_ceiling_enforcement)
- [gp_resource_group_cpu_limit](guc-list.html#gp_resource_group_cpu_limit)
- [gp_resource_group_cpu_priority](guc-list.html#gp_resource_group_cpu_priority)
- [gp_resource_group_enable_recalculate_query_mem](guc-list.html#gp_resource_group_enable_recalculate_query_mem)
- [gp_resource_group_memory_limit](guc-list.html#gp_resource_group_memory_limit)
- [gp_resource_group_queuing_timeout](guc-list.html#gp_resource_group_queuing_timeout)
- [gp_resource_manager](guc-list.html#gp_resource_manager)
- [gp_vmem_idle_resource_timeout](guc-list.html#gp_vmem_idle_resource_timeout)
- [gp_vmem_protect_segworker_cache_limit](guc-list.html#gp_vmem_protect_segworker_cache_limit)
- [max_statement_mem](guc-list.html#max_statement_mem)
- [memory_spill_ratio](guc-list.html#memory_spill_ratio)
- [runaway_detector_activation_percent](guc-list.html#runaway_detector_activation_percent)
- [statement_mem](guc-list.html#statement_mem)
- [vmem_process_interrupt](guc-list.html#vmem_process_interrupt)

## <a id="topic45"></a>External Table Parameters 

The following parameters configure the external tables feature of Greenplum Database.

- [gp_external_enable_exec](guc-list.html#gp_external_enable_exec)
- [gp_external_enable_filter_pushdown](guc-list.html#gp_external_enable_filter_pushdown)
- [gp_external_max_segs](guc-list.html#gp_external_max_segs)
- [gp_initial_bad_row_limit](guc-list.html#gp_initial_bad_row_limit)
- [gp_reject_percent_threshold](guc-list.html#gp_reject_percent_threshold)
- [gpfdist_retry_timeout](guc-list.html#gpfdist_retry_timeout)
- [readable_external_table_timeout](guc-list.html#readable_external_table_timeout)
- [writable_external_table_bufsize](guc-list.html#writable_external_table_bufsize)
- [verify_gpfdists_cert](guc-list.html#verify_gpfdists_cert)

## <a id="topic46"></a>Database Table Parameters 

The following parameter configures default option settings for Greenplum Database tables.

- [default_table_access_method](guc-list.html#default_table_access_method)
- [gp_create_table_random_default_distribution](guc-list.html#gp_create_table_random_default_distribution)
- [gp_default_storage_options](guc-list.html#gp_default_storage_options)
- [gp_enable_segment_copy_checking](guc-list.html#gp_enable_segment_copy_checking)
- [gp_use_legacy_hashops](guc-list.html#gp_use_legacy_hashops)

### <a id="topic_hfd_1tl_zp"></a>Append-Optimized Table Parameters 

The following parameters configure the append-optimized tables feature of Greenplum Database.

- [gp_appendonly_compaction](guc-list.html#gp_appendonly_compaction)
- [gp_appendonly_compaction_threshold](guc-list.html#gp_appendonly_compaction_threshold)
- [validate_previous_free_tid](guc-list.html#validate_previous_free_tid)

## <a id="topic48"></a>Past Version Compatibility Parameters 

The following parameters provide compatibility with older PostgreSQL and Greenplum Database versions. You do not need to change these parameters in Greenplum Database.

### <a id="topic_ax3_r1v_bdb"></a>PostgreSQL 

- [array_nulls](guc-list.html#array_nulls)
- [backslash_quote](guc-list.html#backslash_quote)
- [escape_string_warning](guc-list.html#escape_string_warning)
- [quote_all_identifiers](guc-list.html#quote_all_identifiers)
- [regex_flavor](guc-list.html#regex_flavor)
- [standard_conforming_strings](guc-list.html#standard_conforming_strings)
- [transform_null_equals](guc-list.html#transform_null_equals)

### <a id="topic_jq1_n1v_bdb"></a>Greenplum Database 

- [gp_ignore_error_table](guc-list.html#gp_ignore_error_table)

## <a id="topic49"></a>Greenplum Database Array Configuration Parameters 

The parameters in this topic control the configuration of the Greenplum Database array and its components: segments, coordinator, distributed transaction manager, coordinator mirror, and interconnect.

### <a id="topic50"></a>Interconnect Configuration Parameters 

- [gp_interconnect_address_type](guc-list.html#gp_interconnect_address_type)
- [gp_interconnect_fc_method](guc-list.html#gp_interconnect_fc_method)
- [gp_interconnect_proxy_addresses](guc-list.html#gp_interconnect_proxy_addresses)
- [gp_interconnect_queue_depth](guc-list.html#gp_interconnect_queue_depth)
- [gp_interconnect_setup_timeout](guc-list.html#gp_interconnect_setup_timeout)
- [gp_interconnect_snd_queue_depth](guc-list.html#gp_interconnect_snd_queue_depth)
- [gp_interconnect_transmit_timeout](guc-list.html#gp_interconnect_transmit_timeout)
- [gp_interconnect_type](guc-list.html#gp_interconnect_type)
- [gp_max_packet_size](guc-list.html#gp_max_packet_size)

> **Note** Greenplum Database supports only the UDPIFC \(default\) and TCP interconnect types.

### <a id="topic51"></a>Dispatch Configuration Parameters 

- [gp_cached_segworkers_threshold](guc-list.html#gp_cached_segworkers_threshold)
- [gp_enable_direct_dispatch](guc-list.html#gp_enable_direct_dispatch)
- [gp_segment_connect_timeout](guc-list.html#gp_segment_connect_timeout)
- [gp_set_proc_affinity](guc-list.html#gp_set_proc_affinity)

### <a id="topic52"></a>Fault Operation Parameters 

- [gp_set_read_only](guc-list.html#gp_set_read_only)
- [gp_fts_probe_interval](guc-list.html#gp_fts_probe_interval)
- [gp_fts_probe_retries](guc-list.html#gp_fts_probe_retries)
- [gp_fts_probe_timeout](guc-list.html#gp_fts_probe_timeout)
- [gp_fts_replication_attempt_count](guc-list.html#gp_fts_replication_attempt_count)
- [gp_log_fts](guc-list.html#gp_log_fts)

### <a id="topic53"></a>Distributed Transaction Management Parameters 

- [gp_max_local_distributed_cache](guc-list.html#gp_max_local_distributed_cache)

### <a id="topic54"></a>Read-Only Parameters 

- [gp_command_count](guc-list.html#gp_command_count)
- [gp_content](guc-list.html#gp_content)
- [gp_dbid](guc-list.html#gp_dbid)
- [gp_retrieve_conn](guc-list.html#gp_retrieve_conn)
- [gp_role](guc-list.html#gp_role)
- [gp_session_id](guc-list.html#gp_session_id)
- [gp_server_version](guc-list.html#gp_server_version)
- [gp_server_version_num](guc-list.html#gp_server_version_num)

## <a id="topic55"></a>Greenplum Mirroring Parameters for Coordinator and Segments 

These parameters control the configuration of the replication between Greenplum Database primary coordinator and standby coordinator.

- [repl_catchup_within_range](guc-list.html#repl_catchup_within_range)
- [replication_timeout](guc-list.html#replication_timeout)
- [wait_for_replication_threshold](guc-list.html#wait_for_replication_threshold)
- [wal_keep_segments](guc-list.html#wal_keep_segments)
- [wal_receiver_status_interval](guc-list.html#wal_receiver_status_interval)

## <a id="topic56"></a>Greenplum PL/Java Parameters 

The parameters in this topic control the configuration of the Greenplum Database PL/Java language.

- [pljava_classpath](guc-list.html#pljava_classpath)
- [pljava_classpath_insecure](guc-list.html#pljava_classpath_insecure)
- [pljava_statement_cache_size](guc-list.html#pljava_statement_cache_size)
- [pljava_release_lingering_savepoints](guc-list.html#pljava_release_lingering_savepoints)
- [pljava_vmoptions](guc-list.html#pljava_vmoptions)

## <a id="topic_t3n_qml_rz"></a>XML Data Parameters 

The parameters in this topic control the configuration of the Greenplum Database XML data type.

- [xmlbinary](guc-list.html#xmlbinary)
- [xmloption](guc-list.html#xmloption)

