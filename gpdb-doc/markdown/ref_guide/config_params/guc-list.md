# Configuration Parameters 

Descriptions of the Greenplum Database server configuration parameters listed alphabetically.

-   [add\_missing\_from](#add_missing_from)
-   [application\_name](#application_name)
-   [array\_nulls](#array_nulls)
-   [authentication\_timeout](#authentication_timeout)
-   [backslash\_quote](#backslash_quote)
-   [block\_size](#block_size)
-   [bonjour\_name](#bonjour_name)
-   [check\_function\_bodies](#check_function_bodies)
-   [client\_encoding](#client_encoding)
-   [client\_min\_messages](#client_min_messages)
-   [cpu\_index\_tuple\_cost](#cpu_index_tuple_cost)
-   [cpu\_operator\_cost](#cpu_operator_cost)
-   [cpu\_tuple\_cost](#cpu_tuple_cost)
-   [cursor\_tuple\_fraction](#cursor_tuple_fraction)
-   [custom\_variable\_classes](#custom_variable_classes)
-   [data\_checksums](#data_checksums)
-   [DateStyle](#DateStyle)
-   [db\_user\_namespace](#db_user_namespace)
-   [deadlock\_timeout](#deadlock_timeout)
-   [debug\_assertions](#debug_assertions)
-   [debug\_pretty\_print](#debug_pretty_print)
-   [debug\_print\_parse](#debug_print_parse)
-   [debug\_print\_plan](#debug_print_plan)
-   [debug\_print\_prelim\_plan](#debug_print_prelim_plan)
-   [debug\_print\_rewritten](#debug_print_rewritten)
-   [debug\_print\_slice\_table](#debug_print_slice_table)
-   [default\_statistics\_target](#default_statistics_target)
-   [default\_tablespace](#default_tablespace)
-   [default\_transaction\_isolation](#default_transaction_isolation)
-   [default\_transaction\_read\_only](#default_transaction_read_only)
-   [dtx\_phase2\_retry\_count](#dtx_phase2_retry_count)
-   [dynamic\_library\_path](#dynamic_library_path)
-   [effective\_cache\_size](#effective_cache_size)
-   [enable\_bitmapscan](#enable_bitmapscan)
-   [enable\_groupagg](#enable_groupagg)
-   [enable\_hashagg](#enable_hashagg)
-   [enable\_hashjoin](#enable_hashjoin)
-   [enable\_indexscan](#enable_indexscan)
-   [enable\_mergejoin](#enable_mergejoin)
-   [enable\_nestloop](#enable_nestloop)
-   [enable\_seqscan](#enable_seqscan)
-   [enable\_sort](#enable_sort)
-   [enable\_tidscan](#enable_tidscan)
-   [escape\_string\_warning](#escape_string_warning)
-   [explain\_pretty\_print](#explain_pretty_print)
-   [extra\_float\_digits](#extra_float_digits)
-   [filerep\_mirrorvalidation\_during\_resync](#filerep_mirrorvalidation_during_resync)
-   [from\_collapse\_limit](#from_collapse_limit)
-   [gp\_adjust\_selectivity\_for\_outerjoins](#gp_adjust_selectivity_for_outerjoins)
-   [gp\_analyze\_relative\_error](#gp_analyze_relative_error)
-   [gp\_appendonly\_compaction](#gp_appendonly_compaction)
-   [gp\_appendonly\_compaction\_threshold](#gp_appendonly_compaction_threshold)
-   [gp\_autostats\_mode](#gp_autostats_mode)
-   [gp\_autostats\_mode\_in\_functions](#gp_autostats_mode_in_functions)
-   [gp\_autostats\_on\_change\_threshold](#gp_autostats_on_change_threshold)
-   [gp\_backup\_directIO](#gp_backup_directIO)
-   [gp\_backup\_directIO\_read\_chunk\_mb](#gp_backup_directIO_read_chunk_mb)
-   [gp\_cached\_segworkers\_threshold](#gp_cached_segworkers_threshold)
-   [gp\_command\_count](#gp_command_count)
-   [gp\_connection\_send\_timeout](#gp_connection_send_timeout)
-   [gp\_connections\_per\_thread](#gp_connections_per_thread)
-   [gp\_content](#gp_content)
-   [gp\_create\_table\_random\_default\_distribution](#gp_create_table_random_default_distribution)
-   [gp\_dbid](#gp_dbid)
-   [gp\_debug\_linger](#gp_debug_linger)
-   [gp\_default\_storage\_options](#gp_default_storage_options)
-   [gp\_dynamic\_partition\_pruning](#gp_dynamic_partition_pruning)
-   [gp\_email\_from](#gp_email_from)
-   [gp\_email\_smtp\_password](#gp_email_smtp_password)
-   [gp\_email\_smtp\_server](#gp_email_smtp_server)
-   [gp\_email\_smtp\_userid](#gp_email_smtp_userid)
-   [gp\_email\_to](#gp_email_to)
-   [gp\_enable\_agg\_distinct](#gp_enable_agg_distinct)
-   [gp\_enable\_agg\_distinct\_pruning](#gp_enable_agg_distinct_pruning)
-   [gp\_enable\_direct\_dispatch](#gp_enable_direct_dispatch)
-   [gp\_enable\_exchange\_default\_partition](#gp_enable_exchange_default_partition)
-   [gp\_enable\_fallback\_plan](#gp_enable_fallback_plan)
-   [gp\_enable\_fast\_sri](#gp_enable_fast_sri)
-   [gp\_enable\_gpperfmon](#gp_enable_gpperfmon)
-   [gp\_enable\_groupext\_distinct\_gather](#gp_enable_groupext_distinct_gather)
-   [gp\_enable\_groupext\_distinct\_pruning](#gp_enable_groupext_distinct_pruning)
-   [gp\_enable\_multiphase\_agg](#gp_enable_multiphase_agg)
-   [gp\_enable\_predicate\_propagation](#gp_enable_predicate_propagation)
-   [gp\_enable\_preunique](#gp_enable_preunique)
-   [gp\_enable\_relsize\_collection](#gp_enable_relsize_collection)
-   [gp\_enable\_segment\_copy\_checking](#gp_enable_segment_copy_checking)
-   [gp\_enable\_sequential\_window\_plans \(Beta\)](#gp_enable_sequential_window_plans)
-   [gp\_enable\_sort\_distinct](#gp_enable_sort_distinct)
-   [gp\_enable\_sort\_limit](#gp_enable_sort_limit)
-   [gp\_external\_enable\_exec](#gp_external_enable_exec)
-   [gp\_external\_max\_segs](#gp_external_max_segs)
-   [gp\_external\_enable\_filter\_pushdown](#gp_external_enable_filter_pushdown)
-   [gp\_filerep\_tcp\_keepalives\_count](#gp_filerep_tcp_keepalives_count)
-   [gp\_filerep\_tcp\_keepalives\_idle](#gp_filerep_tcp_keepalives_idle)
-   [gp\_filerep\_tcp\_keepalives\_interval](#gp_filerep_tcp_keepalives_interval)
-   [gp\_fts\_probe\_interval](#gp_fts_probe_interval)
-   [gp\_fts\_probe\_retries](#gp_fts_probe_retries)
-   [gp\_fts\_probe\_threadcount](#gp_fts_probe_threadcount)
-   [gp\_fts\_probe\_timeout](#gp_fts_probe_timeout)
-   [gp\_gpperfmon\_send\_interval](#gp_gpperfmon_send_interval)
-   [gpperfmon\_log\_alert\_level](#gpperfmon_log_alert_level)
-   [gp\_hadoop\_home](#gp_hadoop_home)
-   [gp\_hadoop\_target\_version](#gp_hadoop_target_version)
-   [gp\_hashjoin\_tuples\_per\_bucket](#gp_hashjoin_tuples_per_bucket)
-   [gp\_idf\_deduplicate](#gp_idf_deduplicate)
-   [gp\_ignore\_error\_table](#gp_ignore_error_table)
-   [gp\_initial\_bad\_row\_limit](#topic_lvm_ttc_3p)
-   [gp\_interconnect\_debug\_retry\_interval](#gp_interconnect_debug_retry_interval)
-   [gp\_interconnect\_fc\_method](#gp_interconnect_fc_method)
-   [gp\_interconnect\_hash\_multiplier](#gp_interconnect_hash_multiplier)
-   [gp\_interconnect\_queue\_depth](#gp_interconnect_queue_depth)
-   [gp\_interconnect\_setup\_timeout](#gp_interconnect_setup_timeout)
-   [gp\_interconnect\_snd\_queue\_depth](#gp_interconnect_snd_queue_depth)
-   [gp\_interconnect\_type](#gp_interconnect_type)
-   [gp\_log\_interconnect](#gp_log_interconnect)
-   [gp\_log\_format](#gp_log_format)
-   [gp\_log\_fts](#gp_log_fts)
-   [gp\_log\_interconnect](#gp_log_interconnect)
-   [gp\_log\_gang](#gp_log_gang)
-   [gp\_max\_csv\_line\_length](#gp_max_csv_line_length)
-   [gp\_max\_databases](#gp_max_databases)
-   [gp\_max\_filespaces](#gp_max_filespaces)
-   [gp\_max\_local\_distributed\_cache](#gp_max_local_distributed_cache)
-   [gp\_max\_packet\_size](#gp_max_packet_size)
-   [gp\_max\_plan\_size](#gp_max_plan_size)
-   [gp\_max\_slices](#gp_max_slices)
-   [gp\_max\_tablespaces](#gp_max_tablespaces)
-   [gp\_motion\_cost\_per\_row](#gp_motion_cost_per_row)
-   [gp\_num\_contents\_in\_cluster](#gp_num_contents_in_cluster)
-   [gp\_recursive\_cte\_prototype](#gp_recursive_cte_prototype)
-   [gp\_reject\_percent\_threshold](#gp_reject_percent_threshold)
-   [gp\_reraise\_signal](#gp_reraise_signal)
-   [gp\_resgroup\_memory\_policy](#gp_resgroup_memory_policy)
-   [gp\_resource\_group\_bypass](#gp_resource_group_bypass)
-   [gp\_resource\_group\_cpu\_limit](#gp_resource_group_cpu_limit)
-   [gp\_resource\_group\_memory\_limit](#gp_resource_group_memory_limit)
-   [gp\_resource\_manager](#gp_resource_manager)
-   [gp\_resqueue\_memory\_policy](#gp_resqueue_memory_policy)
-   [gp\_resqueue\_priority](#gp_resqueue_priority)
-   [gp\_resqueue\_priority\_cpucores\_per\_segment](#gp_resqueue_priority_cpucores_per_segment)
-   [gp\_resqueue\_priority\_sweeper\_interval](#gp_resqueue_priority_sweeper_interval)
-   [gp\_role](#gp_role)
-   [gp\_safefswritesize](#gp_safefswritesize)
-   [gp\_segment\_connect\_timeout](#gp_segment_connect_timeout)
-   [gp\_segments\_for\_planner](#gp_segments_for_planner)
-   [gp\_server\_version](#gp_server_version)
-   [gp\_server\_version\_num](#gp_server_version_num)
-   [gp\_session\_id](#gp_session_id)
-   [gp\_set\_proc\_affinity](#gp_set_proc_affinity)
-   [gp\_set\_read\_only](#gp_set_read_only)
-   [gp\_snmp\_community](#gp_snmp_community)
-   [gp\_snmp\_monitor\_address](#gp_snmp_monitor_address)
-   [gp\_snmp\_use\_inform\_or\_trap](#gp_snmp_use_inform_or_trap)
-   [gp\_statistics\_pullup\_from\_child\_partition](#gp_statistics_pullup_from_child_partition)
-   [gp\_statistics\_use\_fkeys](#gp_statistics_use_fkeys)
-   [gp\_vmem\_idle\_resource\_timeout](#gp_vmem_idle_resource_timeout)
-   [gp\_vmem\_protect\_limit](#gp_vmem_protect_limit)
-   [gp\_vmem\_protect\_segworker\_cache\_limit](#gp_vmem_protect_segworker_cache_limit)
-   [gp\_workfile\_checksumming](#gp_workfile_checksumming)
-   [gp\_workfile\_compress\_algorithm](#gp_workfile_compress_algorithm)
-   [gp\_workfile\_limit\_files\_per\_query](#gp_workfile_limit_files_per_query)
-   [gp\_workfile\_limit\_per\_query](#gp_workfile_limit_per_query)
-   [gp\_workfile\_limit\_per\_segment](#gp_workfile_limit_per_segment)
-   [gpperfmon\_port](#gpperfmon_port)
-   [ignore\_checksum\_failure](#ignore_checksum_failure)
-   [integer\_datetimes](#integer_datetimes)
-   [IntervalStyle](#IntervalStyle)
-   [join\_collapse\_limit](#join_collapse_limit)
-   [keep\_wal\_segments](#keep_wal_segments)
-   [krb\_caseins\_users](#krb_caseins_users)
-   [krb\_server\_keyfile](#krb_server_keyfile)
-   [krb\_srvname](#krb_srvname)
-   [lc\_collate](#lc_collate)
-   [lc\_ctype](#lc_ctype)
-   [lc\_messages](#lc_messages)
-   [lc\_monetary](#lc_monetary)
-   [lc\_numeric](#lc_numeric)
-   [lc\_time](#lc_time)
-   [listen\_addresses](#listen_addresses)
-   [local\_preload\_libraries](#local_preload_libraries)
-   [log\_autostats](#log_autostats)
-   [log\_connections](#log_connections)
-   [log\_disconnections](#log_disconnections)
-   [log\_dispatch\_stats](#log_dispatch_stats)
-   [log\_duration](#log_duration)
-   [log\_error\_verbosity](#log_error_verbosity)
-   [log\_executor\_stats](#log_executor_stats)
-   [log\_hostname](#log_hostname)
-   [log\_min\_duration\_statement](#log_min_duration_statement)
-   [log\_min\_error\_statement](#log_min_error_statement)
-   [log\_min\_messages](#log_min_messages)
-   [log\_parser\_stats](#log_parser_stats)
-   [log\_planner\_stats](#log_planner_stats)
-   [log\_rotation\_age](#log_rotation_age)
-   [log\_rotation\_size](#log_rotation_size)
-   [log\_statement](#log_statement)
-   [log\_statement\_stats](#log_statement_stats)
-   [log\_temp\_files](#log_temp_files)
-   [log\_timezone](#log_timezone)
-   [log\_truncate\_on\_rotation](#log_truncate_on_rotation)
-   [max\_appendonly\_tables](#max_appendonly_tables)
-   [max\_connections](#max_connections)
-   [max\_files\_per\_process](#max_files_per_process)
-   [max\_fsm\_pages](#max_fsm_pages)
-   [max\_fsm\_relations](#max_fsm_relations)
-   [max\_function\_args](#max_function_args)
-   [max\_identifier\_length](#max_identifier_length)
-   [max\_index\_keys](#max_index_keys)
-   [max\_locks\_per\_transaction](#max_locks_per_transaction)
-   [max\_prepared\_transactions](#max_prepared_transactions)
-   [max\_resource\_portals\_per\_transaction](#max_resource_portals_per_transaction)
-   [max\_resource\_queues](#max_resource_queues)
-   [max\_stack\_depth](#max_stack_depth)
-   [max\_statement\_mem](#max_statement_mem)
-   [memory\_spill\_ratio](#memory_spill_ratio)
-   [optimizer](#optimizer)
-   [optimizer\_array\_expansion\_threshold](#optimizer_array_expansion_threshold)
-   [optimizer\_analyze\_root\_partition](#optimizer_analyze_root_partition)
-   [optimizer\_control](#optimizer_control)
-   [optimizer\_cost\_model](#optimizer_cost_model)
-   [optimizer\_cte\_inlining\_bound](#optimizer_cte_inlining_bound)
-   [optimizer\_dpe\_stats](#optimizer_dpe_stats)
-   [optimizer\_enable\_associativity](#optimizer_enable_associativity)
-   [optimizer\_enable\_dml](#optimizer_enable_dml)
-   [optimizer\_enable\_master\_only\_queries](#optimizer_enable_master_only_queries)
-   [optimizer\_force\_agg\_skew\_avoidance](#optimizer_force_agg_skew_avoidance)
-   [optimizer\_force\_multistage\_agg](#optimizer_force_multistage_agg)
-   [optimizer\_force\_three\_stage\_scalar\_dqa](#optimizer_force_three_stage_scalar_dqa)
-   [optimizer\_join\_arity\_for\_associativity\_commutativity](#optimizer_join_arity_for_associativity_commutativity)
-   [optimizer\_join\_order](#optimizer_join_order)
-   [optimizer\_join\_order\_threshold](#optimizer_join_order_threshold)
-   [optimizer\_mdcache\_size](#optimizer_mdcache_size)
-   [optimizer\_metadata\_caching](#optimizer_metadata_caching)
-   [optimizer\_minidump](#optimizer_minidump)
-   [optimizer\_nestloop\_factor](#optimizer_nestloop_factor)
-   [optimizer\_parallel\_union](#optimizer_parallel_union)
-   [optimizer\_penalize\_skew](#optimizer_penalize_skew)
-   [optimizer\_print\_missing\_stats](#optimizer_print_missing_stats)
-   [optimizer\_print\_optimization\_stats](#optimizer_print_optimization_stats)
-   [optimizer\_sort\_factor](#optimizer_sort_factor)
-   [optimizer\_use\_gpdb\_allocators](#optimizer_use_gpdb_allocators)
-   [optimizer\_xform\_bind\_threshold](#optimizer_xform_bind_threshold)
-   [password\_hash\_algorithm](#password_hash_algorithm)
-   [pgcrypto.fips](#pgcrypto.fips)
-   [pgstat\_track\_activity\_query\_size](#pgstat_track_activity_query_size)
-   [pljava\_classpath](#pljava_classpath)
-   [pljava\_classpath\_insecure](#pljava_classpath_insecure)
-   [pljava\_statement\_cache\_size](#pljava_statement_cache_size)
-   [pljava\_release\_lingering\_savepoints](#pljava_release_lingering_savepoints)
-   [pljava\_vmoptions](#pljava_vmoptions)
-   [port](#port)
-   [random\_page\_cost](#random_page_cost)
-   [readable\_external\_table\_timeout](#readable_external_table_timeout)
-   [repl\_catchup\_within\_range](#repl_catchup_within_range)
-   [replication\_timeout](#replication_timeout)
-   [regex\_flavor](#regex_flavor)
-   [resource\_cleanup\_gangs\_on\_wait](#resource_cleanup_gangs_on_wait)
-   [resource\_select\_only](#resource_select_only)
-   [runaway\_detector\_activation\_percent](#runaway_detector_activation_percent)
-   [search\_path](#search_path)
-   [seq\_page\_cost](#seq_page_cost)
-   [server\_encoding](#server_encoding)
-   [server\_version](#server_version)
-   [server\_version\_num](#server_version_num)
-   [shared\_buffers](#shared_buffers)
-   [shared\_preload\_libraries](#shared_preload_libraries)
-   [ssl](#ssl)
-   [ssl\_ciphers](#ssl_ciphers)
-   [standard\_conforming\_strings](#standard_conforming_strings)
-   [statement\_mem](#statement_mem)
-   [statement\_timeout](#statement_timeout)
-   [stats\_queue\_level](#stats_queue_level)
-   [superuser\_reserved\_connections](#superuser_reserved_connections)
-   [tcp\_keepalives\_count](#tcp_keepalives_count)
-   [tcp\_keepalives\_idle](#tcp_keepalives_idle)
-   [tcp\_keepalives\_interval](#tcp_keepalives_interval)
-   [temp\_buffers](#temp_buffers)
-   [TimeZone](#TimeZone)
-   [timezone\_abbreviations](#timezone_abbreviations)
-   [track\_activities](#track_activities)
-   [track\_activity\_query\_size](#track_activity_query_size)
-   [track\_counts](#track_counts)
-   [transaction\_isolation](#transaction_isolation)
-   [transaction\_read\_only](#transaction_read_only)
-   [transform\_null\_equals](#transform_null_equals)
-   [unix\_socket\_directory](#unix_socket_directory)
-   [unix\_socket\_group](#unix_socket_group)
-   [unix\_socket\_permissions](#unix_socket_permissions)
-   [update\_process\_title](#update_process_title)
-   [vacuum\_cost\_delay](#vacuum_cost_delay)
-   [vacuum\_cost\_limit](#vacuum_cost_limit)
-   [vacuum\_cost\_page\_dirty](#vacuum_cost_page_dirty)
-   [vacuum\_cost\_page\_hit](#vacuum_cost_page_hit)
-   [vacuum\_cost\_page\_miss](#vacuum_cost_page_miss)
-   [vacuum\_freeze\_min\_age](#vacuum_freeze_min_age)
-   [validate\_previous\_free\_tid](#validate_previous_free_tid)
-   [verify\_gpfdists\_cert](#verify_gpfdists_cert)
-   [vmem\_process\_interrupt](#vmem_process_interrupt)
-   [wal\_receiver\_status\_interval](#wal_receiver_status_interval)
-   [writable\_external\_table\_bufsize](#writable_external_table_bufsize)
-   [xid\_stop\_limit](#xid_stop_limit)
-   [xid\_warn\_limit](#xid_warn_limit)
-   [xmlbinary](#xmlbinary)
-   [xmloption](#xmloption)

## add\_missing\_from 

Automatically adds missing table references to FROM clauses. Present for compatibility with releases of PostgreSQL prior to 8.1, where this behavior was allowed by default.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## application\_name 0

Sets the application name for a client session. For example, if connecting via `psql`, this will be set to `psql`. Setting an application name allows it to be reported in log messages and statistics views.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>session<br/>reload|

## array\_nulls 

This controls whether the array input parser recognizes unquoted NULL as specifying a null array element. By default, this is on, allowing array values containing null values to be entered. Greenplum Database versions before 3.0 did not support null values in arrays, and therefore would treat NULL as specifying a normal array element with the string value 'NULL'.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## authentication\_timeout 

Maximum time to complete client authentication. This prevents hung clients from occupying a connection indefinitely.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression \(number and unit\)|1 min|local<br/>system<br/>restart<br/>|

## backslash\_quote 

This controls whether a quote mark can be represented by \\' in a string literal. The preferred, SQL-standard way to represent a quote mark is by doubling it \(''\) but PostgreSQL has historically also accepted \\'. However, use of \\' creates security risks because in some client character set encodings, there are multibyte characters in which the last byte is numerically equivalent to ASCII \\.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|on \(allow \\' always\)<br/>off \(reject always\)<br/>safe\_encoding \(allow only if client encoding does not allow ASCII \\ within a multibyte character\)<br/>|safe\_encoding|master<br/>session<br/>reload|

## block\_size 

Reports the size of a disk block.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of bytes|32768|read only|

## bonjour\_name 

Specifies the Bonjour broadcast name. By default, the computer name is used, specified as an empty string. This option is ignored if the server was not compiled with Bonjour support.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string|unset|master<br/>system<br/>restart<br/>|

## check\_function\_bodies 

When set to off, disables validation of the function body string during `CREATE FUNCTION`. Disabling validation is occasionally useful to avoid problems such as forward references when restoring function definitions from a dump.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## client\_encoding 

Sets the client-side encoding \(character set\). The default is to use the same as the database encoding. See [Supported Character Sets](https://www.postgresql.org/docs/8.3/static/multibyte.html#MULTIBYTE-CHARSET-SUPPORTED) in the PostgreSQL documentation.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|character set|UTF8|master<br/>session<br/>reload| 

## client\_min\_messages 

Controls which message levels are sent to the client. Each level includes all the levels that follow it. The later the level, the fewer messages are sent.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|DEBUG5<br/>DEBUG4<br/>DEBUG3<br/>DEBUG2<br/>DEBUG1<br/>LOG<br/>NOTICE<br/>WARNING<br/>ERROR<br/>FATAL<br/>PANIC|NOTICE|master<br/>session<br/>reload|

## cpu\_index\_tuple\_cost 

For the legacy query optimizer \(planner\), sets the estimate of the cost of processing each index row during an index scan. This is measured as a fraction of the cost of a sequential page fetch.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|0.005|master<br/>session<br/>reload|

## cpu\_operator\_cost 

For the legacy query optimizer \(planner\), sets the estimate of the cost of processing each operator in a WHERE clause. This is measured as a fraction of the cost of a sequential page fetch.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|0.0025|master<br/>session<br/>reload|

## cpu\_tuple\_cost 

For the legacy query optimizer \(planner\), Sets the estimate of the cost of processing each row during a query. This is measured as a fraction of the cost of a sequential page fetch.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|0.01|master<br/>session<br/>reload|

## cursor\_tuple\_fraction 

Tells the legacy query optimizer \(planner\) how many rows are expected to be fetched in a cursor query, thereby allowing the legacy optimizer to use this information to optimize the query plan. The default of 1 means all rows will be fetched.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1|master<br/>session<br/>reload|

## custom\_variable\_classes 

Specifies one or several class names to be used for custom variables. A custom variable is a variable not normally known to the server but used by some add-on modules. Such variables must have names consisting of a class name, a dot, and a variable name.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|comma-separated list of class names|unset|local<br/>system<br/>restart<br/>|

## data\_checksums 

Reports whether checksums are enabled for heap data storage in the database system. Checksums for heap data are enabled or disabled when the database system is initialized and cannot be changed.

Heap data pages store heap tables, catalog tables, indexes, and database metadata. Append-optimized storage has built-in checksum support that is unrelated to this parameter.

Greenplum Database uses checksums to prevent loading data corrupted in the file system into memory managed by database processes. When heap data checksums are enabled, Greenplum Database computes and stores checksums on heap data pages when they are written to disk. When a page is retrieved from disk, the checksum is verified. If the verification fails, an error is generated and the page is not permitted to load into managed memory.

If the `ignore_checksum_failure` configuration parameter has been set to on, a failed checksum verification generates a warning, but the page is allowed to be loaded into managed memory. If the page is then updated, it is flushed to disk and replicated to the mirror. This can cause data corruption to propagate to the mirror and prevent a complete recovery. Because of the potential for data loss, the `ignore_checksum_failure` parameter should only be enabled when needed to recover data. See [ignore\_checksum\_failure](#ignore_checksum_failure) for more information.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|read only|

## DateStyle 

Sets the display format for date and time values, as well as the rules for interpreting ambiguous date input values. This variable contains two independent components: the output format specification and the input/output specification for year/month/day ordering.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<format\>, <date style\><br/>where:<br/><format\> is ISO, Postgres, SQL, or German<br/><date style\> is DMY, MDY, or YMD<br/>|ISO, MDY<br>|master<br/>session<br/>reload|

## db\_user\_namespace 

This enables per-database user names. If on, you should create users as *username@dbname*. To create ordinary global users, simply append @ when specifying the user name in the client.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart<br/>

## deadlock\_timeout 

The time to wait on a lock before checking to see if there is a deadlock condition. On a heavily loaded server you might want to raise this value. Ideally the setting should exceed your typical transaction time, so as to improve the odds that a lock will be released before the waiter decides to check for deadlock.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|1 s|local<br/>system<br/>restart<br/>|

## debug\_assertions 

Turns on various assertion checks.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart<br/>|

## debug\_pretty\_print 

Indents debug output to produce a more readable but much longer output format. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## debug\_print\_parse 

For each executed query, prints the resulting parse tree. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## debug\_print\_plan 

For each executed query, prints the Greenplum parallel query execution plan. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## debug\_print\_prelim\_plan 

For each executed query, prints the preliminary query plan. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## debug\_print\_rewritten 

For each executed query, prints the query rewriter output. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## debug\_print\_slice\_table 

For each executed query, prints the Greenplum query slice plan. *client\_min\_messages* or *log\_min\_messages* must be DEBUG1 or lower.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## default\_statistics\_target 

Sets the default statistics sampling target \(the number of values that are stored in the list of common values\) for table columns that have not had a column-specific target set via `ALTER TABLE SET STATISTICS`. Larger values may improve the quality of the legacy query optimizer \(planner\) estimates.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 \> Integer \> 10000|100|master<br/>session<br/>reload|

## default\_tablespace 

The default tablespace in which to create objects \(tables and indexes\) when a `CREATE` command does not explicitly specify a tablespace.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|name of a tablespace|unset|master<br/>session<br/>reload|

## default\_transaction\_isolation 

Controls the default isolation level of each new transaction.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|read committed<br/>read uncommitted<br/>serializable<br/>|read committed|master<br/>session<br/>reload|

## default\_transaction\_read\_only 

Controls the default read-only status of each new transaction. A read-only SQL transaction cannot alter non-temporary tables.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## dtx\_phase2\_retry\_count 

The maximum number of retries attempted by Greenplum Database during the second phase of a two phase commit. When one or more segments cannot successfully complete the commit phase, the master retries the commit a maximum of `dtx_phase2_retry_count` times. If the commit continues to fail on the last retry attempt, the master generates a PANIC.

When the network is unstable, the master may be unable to connect to one or more segments; increasing the number of two phase commit retries may improve high availability of Greenplum when the master encounters transient network issues.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - `INT_MAX`|10|master<br/>system<br/>restart|

## dynamic\_library\_path 

If a dynamically loadable module needs to be opened and the file name specified in the `CREATE FUNCTION` or `LOAD` command does not have a directory component \(i.e. the name does not contain a slash\), the system will search this path for the required file. The compiled-in PostgreSQL package library directory is substituted for $libdir. This is where the modules provided by the standard PostgreSQL distribution are installed.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|a list of absolute directory paths separated by colons|$libdir|local<br/>system<br/>restart|

## effective\_cache\_size 

Sets the assumption about the effective size of the disk cache that is available to a single query for the legacy query optimizer \(planner\). This is factored into estimates of the cost of using an index; a higher value makes it more likely index scans will be used, a lower value makes it more likely sequential scans will be used. This parameter has no effect on the size of shared memory allocated by a Greenplum server instance, nor does it reserve kernel disk cache; it is used only for estimation purposes.

Set this parameter to a number of [block\_size](#backslash_quote) blocks \(default 32K\) with no units; for example, `8192` for 256MB. You can also directly specify the size of the effective cache; for example, `'1GB'` specifies a size of 32768 blocks. The `gpconfig` utility and `SHOW` command display the effective cache size value in units such as 'GB', 'MB', or 'kB'.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1 - INT\_MAX *or* number and unit|16384 \(512MB\)|master<br/>session<br/>reload|

## enable\_bitmapscan 

Enables or disables the use of bitmap-scan plan types by the legacy query optimizer \(planner\). Note that this is different than a Bitmap Index Scan. A Bitmap Scan means that indexes will be dynamically converted to bitmaps in memory when appropriate, giving faster index performance on complex queries against very large tables. It is used when there are multiple predicates on different indexed columns. Each bitmap per column can be compared to create a final list of selected tuples.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_groupagg 

Enables or disables the use of group aggregation plan types by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_hashagg 

Enables or disables the use of hash aggregation plan types by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_hashjoin 

Enables or disables the use of hash-join plan types by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_indexscan 

Enables or disables the use of index-scan plan types by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_mergejoin 

Enables or disables the use of merge-join plan types by the legacy query optimizer \(planner\). Merge join is based on the idea of sorting the left- and right-hand tables into order and then scanning them in parallel. So, both data types must be capable of being fully ordered, and the join operator must be one that can only succeed for pairs of values that fall at the 'same place' in the sort order. In practice this means that the join operator must behave like equality.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## enable\_nestloop 

Enables or disables the use of nested-loop join plans by the legacy query optimizer \(planner\). It's not possible to suppress nested-loop joins entirely, but turning this variable off discourages the legacy optimizer from using one if there are other methods available.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## enable\_seqscan 

Enables or disables the use of sequential scan plan types by the legacy query optimizer \(planner\). It's not possible to suppress sequential scans entirely, but turning this variable off discourages the legacy optimizer from using one if there are other methods available.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_sort 

Enables or disables the use of explicit sort steps by the legacy query optimizer \(planner\). It's not possible to suppress explicit sorts entirely, but turning this variable off discourages the legacy optimizer from using one if there are other methods available.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## enable\_tidscan 

Enables or disables the use of tuple identifier \(TID\) scan plan types by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## escape\_string\_warning 

When on, a warning is issued if a backslash \(\\\) appears in an ordinary string literal \('...' syntax\). Escape string syntax \(E'...'\) should be used for escapes, because in future versions, ordinary strings will have the SQL standard-conforming behavior of treating backslashes literally.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## explain\_pretty\_print 

Determines whether EXPLAIN VERBOSE uses the indented or non-indented format for displaying detailed query-tree dumps.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## extra\_float\_digits 

Adjusts the number of digits displayed for floating-point values, including float4, float8, and geometric data types. The parameter value is added to the standard number of digits. The value can be set as high as 3, to include partially-significant digits; this is especially useful for dumping float data that needs to be restored exactly. Or it can be set negative to suppress unwanted digits.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|0|master<br/>session<br/>reload|

## filerep\_mirrorvalidation\_during\_resync 

The default setting `false` improves Greenplum Database performance during incremental resynchronization of segment mirrors. Setting the value to `true` enables checking for the existence of files for all relations on the segment mirror during incremental resynchronization. Checking for files degrades performance of the incremental resynchronization process but provides a minimal check of database objects.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>session<br/>reload|

## from\_collapse\_limit 

The legacy query optimizer \(planner\) will merge sub-queries into upper queries if the resulting FROM list would have no more than this many items. Smaller values reduce planning time but may yield inferior query plans.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-*n*|20|master<br/>session<br/>reload|

## gp\_adjust\_selectivity\_for\_outerjoins 

Enables the selectivity of NULL tests over outer joins.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_analyze\_relative\_error 

Sets the estimated acceptable error in the cardinality of the table " a value of 0.5 is supposed to be equivalent to an acceptable error of 50% \(this is the default value used in PostgreSQL\). If the statistics collected during `ANALYZE` are not producing good estimates of cardinality for a particular table attribute, decreasing the relative error fraction \(accepting less error\) tells the system to sample more rows.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point < 1.0|0.25|master<br/>session<br/>reload|

## gp\_appendonly\_compaction 

Enables compacting segment files during `VACUUM` commands. When disabled, `VACUUM` only truncates the segment files to the EOF value, as is the current behavior. The administrator may want to disable compaction in high I/O load situations or low space situations.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_appendonly\_compaction\_threshold 

Specifies the threshold ratio \(as a percentage\) of hidden rows to total rows that triggers compaction of the segment file when VACUUM is run without the FULL option \(a lazy vacuum\). If the ratio of hidden rows in a segment file on a segment is less than this threshold, the segment file is not compacted, and a log message is issued.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \(%\)|10|master<br/>session<br/>reload|

## gp\_autostats\_mode 

Specifies the mode for triggering automatic statistics collection with `ANALYZE`. The `on_no_stats` option triggers statistics collection for `CREATE TABLE AS SELECT`, `INSERT`, or `COPY` operations on any table that has no existing statistics.

The `on_change` option triggers statistics collection only when the number of rows affected exceeds the threshold defined by `gp_autostats_on_change_threshold`. Operations that can trigger automatic statistics collection with `on_change` are:

`CREATE TABLE AS SELECT`

`UPDATE`

`DELETE`

`INSERT`

`COPY`

Default is `on_no_stats`.

**Note:** For partitioned tables, automatic statistics collection is not triggered if data is inserted from the top-level parent table of a partitioned table.

Automatic statistics collection is triggered if data is inserted directly in a leaf table \(where the data is stored\) of the partitioned table. Statistics are collected only on the leaf table.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|none<br/>on\_change<br/>on\_no\_stats<br/>|on\_no\_ stats|master<br/>session<br/>reload|

## gp\_autostats\_mode\_in\_functions 

Specifies the mode for triggering automatic statistics collection with `ANALYZE` for statements in procedural language functions. The `none` option disables statistics collection. The `on_no_stats` option triggers statistics collection for `CREATE TABLE AS SELECT`, `INSERT`, or `COPY` operations that are executed in functions on any table that has no existing statistics.

The `on_change` option triggers statistics collection only when the number of rows affected exceeds the threshold defined by `gp_autostats_on_change_threshold`. Operations in functions that can trigger automatic statistics collection with `on_change` are:

`CREATE TABLE AS SELECT`

`UPDATE`

`DELETE`

`INSERT`

`COPY`

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|none<br/>on\_change<br/>none<br/>|none|master<br/>session<br/>reload|

## gp\_autostats\_on\_change\_threshold 

Specifies the threshold for automatic statistics collection when `gp_autostats_mode` is set to `on_change`. When a triggering table operation affects a number of rows exceeding this threshold, `ANALYZE`is added and statistics are collected for the table.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|2147483647|master<br/>session<br/>reload|

## gp\_backup\_directIO 

Direct I/O allows Greenplum Database to bypass the buffering of memory within the file system cache for backup. When Direct I/O is used for a file, data is transferred directly from the disk to the application buffer, without the use of the file buffer cache.

Direct I/O is supported only on Red Hat Enterprise Linux, CentOS, and SUSE.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|on, off|off|local<br/>session<br/>reload|

## gp\_backup\_directIO\_read\_chunk\_mb 

Sets the chunk size in MB when Direct I/O is enabled with [gp\_backup\_directIO](#gp_backup_directIO). The default chunk size is 20MB.

The default value is the optimal setting. Decreasing it will increase the backup time and increasing it will result in little change to backup time.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-200|20 MB|local<br/>session<br/>reload|

## gp\_cached\_segworkers\_threshold 

When a user starts a session with Greenplum Database and issues a query, the system creates groups or 'gangs' of worker processes on each segment to do the work. After the work is done, the segment worker processes are destroyed except for a cached number which is set by this parameter. A lower setting conserves system resources on the segment hosts, but a higher setting may improve performance for power-users that want to issue many complex queries in a row.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|5|master<br/>session<br/>reload|

## gp\_command\_count 

Shows how many commands the master has received from the client. Note that a single SQLcommand might actually involve more than one command internally, so the counter may increment by more than one for a single query. This counter also is shared by all of the segment processes working on the command.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|1|read only|

## gp\_connection\_send\_timeout 

Timeout for sending data to unresponsive Greenplum Database user clients during query processing. A value of 0 disables the timeout, Greenplum Database waits indefinitely for a client. When the timeout is reached, the query is cancelled with this message:

```
Could not send data to client: Connection timed out.
```

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of seconds|3600 \(1 hour\)|master<br/>session<br/>reload|

## gp\_connections\_per\_thread 

Controls the number of asynchronous threads \(worker threads\) that a Greenplum Database query dispatcher \(QD\) generates when dispatching work to query executor processes on segment instances when processing SQL queries. The value sets the number of primary segment instances that a worker thread connects to when processing a query. For example, when the value is 2 and there are 64 segment instances, a QD generates 32 worker threads to dispatch a query plan work. Each thread is assigned to two segments.

For the default value, 0, a query dispatcher generates two types of threads: a main thread that manages the dispatch of query plan work, and an interconnect thread. The main thread also acts as a worker thread.

For a value greater than 0, a QD generates three types of threads: a main thread, one or more worker threads, and an interconnect thread. When the value is equal to or greater than the number of segment instances, a QD generates three threads: a main thread, a single worker thread, and an interconnect thread.

The value does not need to be changed from the default unless there are known throughput performance issues.

This parameter is master only and changing it requires a server restart.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \>= 0<br/>|0<br/>|master<br/>restart|

## gp\_content 

The local content id if a segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer| |read only|

## gp\_create\_table\_random\_default\_distribution 

Controls table creation when a Greenplum Database table is created with a CREATE TABLE or CREATE TABLE AS command that does not contain a DISTRIBUTED BY clause.

For CREATE TABLE, if the value of the parameter is `off` \(the default\), and the table creation command does not contain a DISTRIBUTED BY clause, Greenplum Database chooses the table distribution key based on the command. If the LIKE or INHERITS clause is specified in table creation command, the created table uses the same distribution key as the source or parent table.

If the value of the parameter is set to `on`, Greenplum Database follows these rules to create a table when the DISTRIBUTED BY clause is not specified:

-   If PRIMARY KEY or UNIQUE columns are not specified, the distribution of the table is random \(DISTRIBUTED RANDOMLY\). Table distribution is random even if the table creation command contains the LIKE or INHERITS clause.
-   If PRIMARY KEY or UNIQUE columns are specified, a DISTRIBUTED BY clause must also be specified. If a DISTRIBUTED BY clause is not specified as part of the table creation command, the command fails.

For a CREATE TABLE AS command that does not contain a distribution clause:

-   If the legacy query optimizer creates the table, and the value of the parameter is `off`, the table distribution policy is determined based on the command.
-   If the legacy query optimizer creates the table, and the value of the parameter is `on`, the table distribution policy is random.
-   If GPORCA creates the table, the table distribution policy is random. The parameter value has no affect.

For information about the legacy query optimizer and GPORCA, see "Querying Data" in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|boolean|off|master<br/>system<br/>reload|

## gp\_dbid 

The local content dbid if a segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer| |read only|

## gp\_debug\_linger 

Number of seconds for a Greenplum process to linger after a fatal internal error.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|0|master<br/>session<br/>reload|

## gp\_default\_storage\_options 

Set the default values for the following table storage options when a table is created with the CREATE TABLE command.

-   APPENDONLY
-   BLOCKSIZE
-   CHECKSUM
-   COMPRESSTYPE
-   COMPRESSLEVEL
-   ORIENTATION

Specify multiple storage option values as a comma separated list.

You can set the storage options with this parameter instead of specifying the table storage options in the WITH of the CREATE TABLE command. The table storage options that are specified with the CREATE TABLE command override the values specified by this parameter.

Not all combinations of storage option values are valid. If the specified storage options are not valid, an error is returned. See the CREATE TABLE command for information about table storage options.

The defaults can be set for a database and user. If the server configuration parameter is set at different levels, this the order of precedence, from highest to lowest, of the table storage values when a user logs into a database and creates a table:

1.  The values specified in a CREATE TABLE command with the WITH clause or ENCODING clause
2.  The value of `gp_default_storage_options` that set for the user with the ALTER ROLE...SET command
3.  The value of `gp_default_storage_options` that is set for the database with the ALTER DATABASE...SET command
4.  The value of `gp_default_storage_options` that is set for the Greenplum Database system with the gpconfig utility

The parameter value is not cumulative. For example, if the parameter specifies the APPENDONLY and COMPRESSTYPE options for a database and a user logs in and sets the parameter to specify the value for the ORIENTATION option, the APPENDONLY, and COMPRESSTYPE values set at the database level are ignored.

This example ALTER DATABASE command sets the default ORIENTATION and COMPRESSTYPE table storage options for the database `mystest`.

```
ALTER DATABASE mytest SET gp_default_storage_options = 'orientation=column, compresstype=rle_type'
```

To create an append-optimized table in the `mytest` database with column-oriented table and RLE compression. The user needs to specify only `APPENDONLY=TRUE` in the WITH clause.

This example gpconfig utility command sets the default storage option for a Greenplum Database system. If you set the defaults for multiple table storage options, the value must be enclosed in single quotes.

```
gpconfig -c 'gp_default_storage_options' -v 'appendonly=true, orientation=column'
```

This example gpconfig utility command shows the value of the parameter. The parameter value must be consistent across the Greenplum Database master and all segments.

```
gpconfig -s 'gp_default_storage_options'
```

|Value Range|Default|Set Classifications<sup>1</sup>|
|-----------|-------|---------------------|
|APPENDONLY=<br/>`TRUE` \| `FALSE`<br/>BLOCKSIZE= integer between 8192 and 2097152<br/>CHECKSUM= `TRUE` \| `FALSE`<br/>COMPRESSTYPE= `ZLIB` \| `QUICKLZ`<sup>2</sup> \| `RLE`\_`TYPE` \| `NONE`<br/>COMPRESSLEVEL= integer between 0 and 19<br/>ORIENTATION= `ROW` \| `COLUMN`<br/>|APPENDONLY=`FALSE`<br/>BLOCKSIZE=`32768`<br/>CHECKSUM=`TRUE`<br/>COMPRESSTYPE=`none`<br/>COMPRESSLEVEL=`0`<br/>ORIENTATION=`ROW`|master<br/>session<br/>reload|


**Note:** <sup>1</sup>The set classification when the parameter is set at the system level with the gpconfig utility.

**Note:** <sup>2</sup>QuickLZ compression is available only in the commercial release of Tanzu Greenplum.

## gp\_dynamic\_partition\_pruning 

Enables plans that can dynamically eliminate the scanning of partitions.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|on/off|on|master<br/>session<br/>reload|

## gp\_email\_from 

The email address used to send email alerts, in the format of:

`'username@example.com'`

or

`'Name <username@example.com>'`

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## gp\_email\_smtp\_password 

The password/passphrase used to authenticate with the SMTP server.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## gp\_email\_smtp\_server 

The fully qualified domain name or IP address and port of the SMTP server to use to send the email alerts. Must be in the format of:

smtp\_servername.domain.com:port

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## gp\_email\_smtp\_userid 

The user id used to authenticate with the SMTP server.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## gp\_email\_to 

A semi-colon \(`;`\) separated list of email addresses to receive email alert messages to in the format of: `'username@example.com'`

or

`'Name <username@example.com>'`

If this parameter is not set, then email alerts are disabled.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## gp\_enable\_agg\_distinct 

Enables or disables two-phase aggregation to compute a single distinct-qualified aggregate. This applies only to subqueries that include a single distinct-qualified aggregate function.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_agg\_distinct\_pruning 

Enables or disables three-phase aggregation and join to compute distinct-qualified aggregates. This applies only to subqueries that include one or more distinct-qualified aggregate functions.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_direct\_dispatch 

Enables or disables the dispatching of targeted query plans for queries that access data on a single segment. When on, queries that target rows on a single segment will only have their query plan dispatched to that segment \(rather than to all segments\). This significantly reduces the response time of qualifying queries as there is no interconnect setup involved. Direct dispatch does require more CPU utilization on the master.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>system<br/>restart|

## gp\_enable\_exchange\_default\_partition 

Controls availability of the `EXCHANGE DEFAULT PARTITION` clause for `ALTER TABLE`. The default value for the parameter is `off`. The clause is not available and Greenplum Database returns an error if the clause is specified in an `ALTER TABLE` command.

If the value is `on`, Greenplum Database returns a warning stating that exchanging the default partition might result in incorrect results due to invalid data in the default partition.

**Warning:** Before you exchange the default partition, you must ensure the data in the table to be exchanged, the new default partition, is valid for the default partition. For example, the data in the new default partition must not contain data that would be valid in other leaf child partitions of the partitioned table. Otherwise, queries against the partitioned table with the exchanged default partition that are executed by GPORCA might return incorrect results.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## gp\_enable\_fallback\_plan 

Allows use of disabled plan types when a query would not be feasible without them.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_fast\_sri 

When set to `on`, the legacy query optimizer \(planner\) plans single row inserts so that they are sent directly to the correct segment instance \(no motion operation required\). This significantly improves performance of single-row-insert statements.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_gpperfmon 

Enables or disables the data collection agents that populate the `gpperfmon` database for Greenplum Command Center.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## gp\_enable\_groupext\_distinct\_gather 

Enables or disables gathering data to a single node to compute distinct-qualified aggregates on grouping extension queries. When this parameter and `gp_enable_groupext_distinct_pruning` are both enabled, the legacy query optimizer \(planner\) uses the cheaper plan.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_groupext\_distinct\_pruning 

Enables or disables three-phase aggregation and join to compute distinct-qualified aggregates on grouping extension queries. Usually, enabling this parameter generates a cheaper query plan that the legacy query optimizer \(planner\) will use in preference to existing plan.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_multiphase\_agg 

Enables or disables the use of two or three-stage parallel aggregation plans legacy query optimizer \(planner\). This approach applies to any subquery with aggregation. If `gp_enable_multiphase_agg` is off, then`gp_enable_agg_distinct` and `gp_enable_agg_distinct_pruning` are disabled.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_predicate\_propagation 

When enabled, the legacy query optimizer \(planner\) applies query predicates to both table expressions in cases where the tables are joined on their distribution key column\(s\). Filtering both tables prior to doing the join \(when possible\) is more efficient.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_preunique 

Enables two-phase duplicate removal for `SELECT DISTINCT` queries \(not `SELECT COUNT(DISTINCT)`\). When enabled, it adds an extra `SORT DISTINCT` set of plan nodes before motioning. In cases where the distinct operation greatly reduces the number of rows, this extra `SORT DISTINCT` is much cheaper than the cost of sending the rows across the Interconnect.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_relsize\_collection 

Enables GPORCA and the legacy query optimizer \(planner\) to use the estimated size of a table \(`pg_relation_size` function\) if there are no statistics for the table. By default, GPORCA and the planner use a default value to estimate the number of rows if statistics are not available. The default behavior improves query optimization time and reduces resource queue usage in heavy workloads, but can lead to suboptimal plans.

This parameter is ignored for a root partition of a partitioned table. When GPORCA is enabled and the root partition does not have statistics, GPORCA always uses the default value. You can use `ANALZYE ROOTPARTITION` to collect statistics on the root partition. See [ANALYZE](../sql_commands/ANALYZE.html).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## gp\_enable\_sequential\_window\_plans \(Beta\) 

If on, enables non-parallel \(sequential\) query plans for queries containing window function calls. If off, evaluates compatible window functions in parallel and rejoins the results. This is a Beta feature.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_segment\_copy\_checking 

Controls whether the distribution policy for a table \(from the table `DISTRIBUTED` clause\) is checked when data is copied into the table with the `COPY FROM...ON SEGMENT` command. The default is `true`, check the policy when copying data into the table. An error is returned in these situations.

-   For a table that is not a partitioned table, an error is returned if the row of data violates the distribution policy for a segment instance.
-   For a partitioned table, an error is returned if either of the following is true.
    -   If the distribution policy of the child leaf partitioned table is the same as the root table and the row of data violates the distribution policy for a segment instance.
    -   If the distribution policy of the child leaf partitioned table is not the same as the root table.

If the value is `false`, the distribution policy is not checked. The data added to the table might violate the table distribution policy for the segment instance. Manual redistribution of table data might be required. See the `ALTER TABLE` clause `WITH REORGANIZE`.

The parameter can be set for a database system or a session. The parameter cannot be set for a specific database.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## gp\_enable\_sort\_distinct 

Enable duplicates to be removed while sorting.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_enable\_sort\_limit 

Enable `LIMIT` operation to be performed while sorting. Sorts more efficiently when the plan requires the first *limit\_number* of rows at most.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_external\_enable\_exec 

Enables or disables the use of external tables that execute OS commands or scripts on the segment hosts \(`CREATE EXTERNAL TABLE EXECUTE` syntax\). Must be enabled if using the Command Center or MapReduce features.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>system<br/>restart|

## gp\_external\_max\_segs 

Sets the number of segments that will scan external table data during an external table operation, the purpose being not to overload the system with scanning data and take away resources from other concurrent operations. This only applies to external tables that use the `gpfdist://` protocol to access external table data.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|64|master<br/>session<br/>reload|

## gp\_external\_enable\_filter\_pushdown 

Enable filter pushdown when reading data from external tables. If pushdown fails, a query is executed without pushing filters to the external data source \(instead, Greenplum Database applies the same constraints to the result\). See [Defining External Tables](../../admin_guide/external/g-external-tables.html) for more information.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_filerep\_tcp\_keepalives\_count 

How many keepalives may be lost before the connection is considered dead. A value of 0 uses the system default. If TCP\_KEEPCNT is not supported, this parameter must be 0.

Use this parameter for all connections that are between a primary and mirror segment. Use `tcp_keepalives_count` for settings that are not between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of lost keepalives|2|local<br/>system<br/>restart|

## gp\_filerep\_tcp\_keepalives\_idle 

Number of seconds between sending keepalives on an otherwise idle connection. A value of 0 uses the system default. If TCP\_KEEPIDLE is not supported, this parameter must be 0.

Use this parameter for all connections that are between a primary and mirror segment. Use `tcp_keepalives_idle` for settings that are not between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of seconds|1 min|local<br/>system<br/>restart|

## gp\_filerep\_tcp\_keepalives\_interval 

How many seconds to wait for a response to a keepalive before retransmitting. A value of 0 uses the system default. If TCP\_KEEPINTVL is not supported, this parameter must be 0.

Use this parameter for all connections that are between a primary and mirror segment. Use tcp\_keepalives\_interval for settings that are not between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of seconds|30 sec|local<br/>system<br/>restart|

## gp\_fts\_probe\_interval 

Specifies the polling interval for the fault detection process \(`ftsprobe`\). The `ftsprobe` process will take approximately this amount of time to detect a segment failure.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|10 - 3600 seconds|1 min|master<br/>system<br/>restart|

## gp\_fts\_probe\_retries 

Specifies the number of times the fault detection process \(`ftsprobe`\) attempts to connect to a segment before reporting segment failure.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|5|master<br/>system<br/>restart|

## gp\_fts\_probe\_threadcount 

Specifies the number of `ftsprobe` threads to create. This parameter should be set to a value equal to or greater than the number of segments per host.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1 - 128|16|master<br/>system<br/>restart|

## gp\_fts\_probe\_timeout 

Specifies the allowed timeout for the fault detection process \(`ftsprobe`\) to establish a connection to a segment before declaring it down.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|10 - 3600 seconds|20 secs|master<br/>system<br/>restart|

## gp\_log\_fts 

Controls the amount of detail the fault detection process \(`ftsprobe`\) writes to the log file.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|OFF<br/>TERSE<br/>VERBOSE<br/>DEBUG<br/>|TERSE|master<br/>system<br/>restart|

## gp\_log\_interconnect 

Controls the amount of information that is written to the log file about communication between Greenplum Database segment instance worker processes. The default value is `terse`. The log information is written to both the master and segment instance logs.

Increasing the amount of logging could affect performance and increase disk space usage.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|OFF<br/>TERSE<br/>VERBOSE<br/>DEBUG<br/>|TERSE|master<br/>system<br/>reload|

## gp\_log\_gang 

Controls the amount of information that is written to the log file about query worker process creation and query management. The default value is `OFF`, do not log information.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|OFF<br/>TERSE<br/>VERBOSE<br/>DEBUG<br/>|OFF|master<br/>system<br/>restart|

## gp\_gpperfmon\_send\_interval 

Sets the frequency that the Greenplum Database server processes send query execution updates to the data collection agent processes used to populate the `gpperfmon` database for Command Center. Query operations executed during this interval are sent through UDP to the segment monitor agents. If you find that an excessive number of UDP packets are dropped during long-running, complex queries, you may consider increasing this value.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|1 sec|master<br/>system<br/>restart<br/>superuser|

## gpperfmon\_log\_alert\_level 

Controls which message levels are written to the gpperfmon log. Each level includes all the levels that follow it. The later the level, the fewer messages are sent to the log.

**Note:** If the `gpperfmon` database is installed and is monitoring the database, the default value is warning.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|none<br/>warning<br/>error<br/>fatal<br/>panic|none|local<br/>system<br/>restart|

## gp\_hadoop\_home 

Specifies the installation directory for the Greenplum Database `gphdfs` protocol \(deprecated\) Hadoop target.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Valid directory name|Value of `HADOOP_HOME`|local<br/>session<br/>reload|

## gp\_hadoop\_target\_version 

The installed version of the Greenplum Database `gphdfs` protocol \(deprecated\) Hadoop target.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|cdh<br/>hadoop<br/>hdp<br/>mpr|hadoop|local<br/>session<br/>reload|

## gp\_hashjoin\_tuples\_per\_bucket 

Sets the target density of the hash table used by HashJoin operations. A smaller value will tend to produce larger hash tables, which can increase join performance.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|5|master<br/>session<br/>reload|

## gp\_idf\_deduplicate 

Changes the strategy to compute and process MEDIAN, and PERCENTILE\_DISC.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|auto<br/>none<br/>force|auto|master<br/>session<br/>reload|

## gp\_ignore\_error\_table 

Controls Greenplum Database behavior when the `INTO ERROR TABLE` clause is specified in a `CREATE EXTERNAL TABLE` or `COPY` command. The `INTO ERROR TABLE` clause was removed in Greenplum Database 5.

The default value is `false`, Greenplum Database returns an error if the `INTO ERROR TABLE` clause is specified in a command.

If the value is `true`, Greenplum Database ignores the clause, issues a warning, and executes the command without the `INTO ERROR TABLE` clause. In Greenplum Database 5.x and later, you access the error log information with built-in SQL functions. See the [CREATE EXTERNAL TABLE](../sql_commands/CREATE_EXTERNAL_TABLE.html) or [COPY](../sql_commands/COPY.html) command.

You can set this value to `true` to avoid the Greenplum Database error when you run applications that execute `CREATE EXTERNAL TABLE` or `COPY` commands that include the Greenplum Database 4.3.x `INTO ERROR TABLE` clause.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>session<br/>reload|

## gp\_initial\_bad\_row\_limit 

For the parameter value *n*, Greenplum Database stops processing input rows when you import data with the `COPY` command or from an external table if the first *n* rows processed contain formatting errors. If a valid row is processed within the first *n* rows, Greenplum Database continues processing input rows.

Setting the value to 0 disables this limit.

The `SEGMENT REJECT LIMIT` clause can also be specified for the `COPY` command or the external table definition to limit the number of rejected rows.

`INT_MAX` is the largest value that can be stored as an integer on your system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 0 - `INT_MAX`|1000|master<br/>session<br/>reload|

## gp\_interconnect\_debug\_retry\_interval 

Specifies the interval, in seconds, to log Greenplum Database interconnect debugging messages when the server configuration parameter [gp\_log\_interconnect](#gp_log_interconnect) is set to `DEBUG`. The default is 10 seconds.

The log messages contain information about the interconnect communication between Greenplum Database segment instance worker processes. The information can be helpful when debugging network issues between segment instances.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1 =< Integer < 4096|10|local<br/>session<br/>reload|

## gp\_interconnect\_fc\_method 

Specifies the flow control method used for the default Greenplum Database UDPIFC interconnect.

For capacity based flow control, senders do not send packets when receivers do not have the capacity.

Loss based flow control is based on capacity based flow control, and also tunes the sending speed according to packet losses.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|CAPACITY<br/>LOSS|LOSS|master<br/>session<br/>reload|

## gp\_interconnect\_hash\_multiplier 

Sets the size of the hash table used by the Greenplum Database to track interconnect connections with the default UDPIFC interconnect. This number is multiplied by the number of segments to determine the number of buckets in the hash table. Increasing the value may increase interconnect performance for complex multi-slice queries \(while consuming slightly more memory on the segment hosts\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|2-25|2|master<br/>session<br/>reload|

## gp\_interconnect\_queue\_depth 

Sets the amount of data per-peer to be queued by the Greenplum Database interconnect on receivers \(when data is received but no space is available to receive it the data will be dropped, and the transmitter will need to resend it\) for the default UDPIFC interconnect. Increasing the depth from its default value will cause the system to use more memory, but may increase performance. It is reasonable to set this value between 1 and 10. Queries with data skew potentially perform better with an increased queue depth. Increasing this may radically increase the amount of memory used by the system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-2048|4|master<br/>session<br/>reload|

## gp\_interconnect\_setup\_timeout 

Specifies the amount of time, in seconds, that Greenplum Database waits for the interconnect to complete setup before it times out.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - 7200 seconds|7200 seconds \(2 hours\)|master<br/>session<br/>reload|

## gp\_interconnect\_snd\_queue\_depth 

Sets the amount of data per-peer to be queued by the default UDPIFC interconnect on senders. Increasing the depth from its default value will cause the system to use more memory, but may increase performance. Reasonable values for this parameter are between 1 and 4. Increasing the value might radically increase the amount of memory used by the system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1 - 4096|2|master<br/>session<br/>reload|

## gp\_interconnect\_transmit\_timeout 

Specifies the amount of time, in seconds, that Greenplum Database waits for network transmission of interconnect traffic to complete before it times out.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1 - 7200 seconds|3600 seconds \(1 hour\)|master<br/>session<br/>reload|

## gp\_interconnect\_type 

Sets the networking protocol used for Greenplum Database interconnect traffic. UDPIFC specifies using UDP with flow control for interconnect traffic, and is the only value supported.

UDPIFC \(the default\) specifies using UDP with flow control for interconnect traffic. Specify the interconnect flow control method with [gp\_interconnect\_fc\_method](#gp_interconnect_fc_method).

With TCP as the interconnect protocol, Greenplum Database has an upper limit of 1000 segment instances - less than that if the query workload involves complex, multi-slice queries.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|UDPIFC<br/>TCP|UDPIFC|local<br/>session<br/>reload|

## gp\_log\_format 

Specifies the format of the server log files. If using *gp\_toolkit* administrative schema, the log files must be in CSV format.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|csv<br/>text|csv|local<br/>system<br/>restart|

## gp\_max\_csv\_line\_length 

The maximum length of a line in a CSV formatted file that will be imported into the system. The default is 1MB \(1048576 bytes\). Maximum allowed is 1GB \(1073741824 bytes\). The default may need to be increased if using the *gp\_toolkit* administrative schema to read Greenplum Database log files.

**Note:** This parameter is deprecated and will be removed in a future release.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of bytes|1048576|local<br/>system<br/>restart|

## gp\_max\_databases 

The maximum number of databases allowed in a Greenplum Database system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|16|master<br/>system<br/>restart|

## gp\_max\_filespaces 

The maximum number of filespaces allowed in a Greenplum Database system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|8|master<br/>system<br/>restart|

## gp\_max\_local\_distributed\_cache 

Sets the maximum number of distributed transaction log entries to cache in the backend process memory of a segment instance.

The log entries contain information about the state of rows that are being accessed by an SQL statement. The information is used to determine which rows are visible to an SQL transaction when executing multiple simultaneous SQL statements in an MVCC environment. Caching distributed transaction log entries locally improves transaction processing speed by improving performance of the row visibility determination process.

The default value is optimal for a wide variety of SQL processing environments.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1024|local<br/>system<br/>restart|

## gp\_max\_packet\_size 

Sets the tuple-serialization chunk size for the Greenplum Database interconnect.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|512-65536|8192|master<br/>system<br/>restart|

## gp\_max\_plan\_size 

Specifies the total maximum uncompressed size of a query execution plan multiplied by the number of Motion operators \(slices\) in the plan. If the size of the query plan exceeds the value, the query is cancelled and an error is returned. A value of 0 means that the size of the plan is not monitored.

You can specify a value in `kB`, `MB`, or `GB`. The default unit is `kB`. For example, a value of `200` is 200kB. A value of `1GB` is the same as `1024MB` or `1048576kB`.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|0|master<br/>superuser<br/>session|

## gp\_max\_slices 

Specifies the maximum number of slices \(portions of a query plan that are executed on segment instances\) that can be generated by a query. If the query generates more than the specified number of slices, Greenplum Database returns an error and does not execute the query. The default value is `0`, no maximum value.

Executing a query that generates a large number of slices might affect Greenplum Database performance. For example, a query that contains `UNION` or `UNION ALL` operators over several complex views can generate a large number of slices. You can run `EXPLAIN ANALYZE` on the query to view slice statistics for the query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - INT\_MAX|0|master<br/>session<br/>reload|

## gp\_max\_tablespaces 

The maximum number of tablespaces allowed in a Greenplum Database system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|16|master<br/>system<br/>restart|

## gp\_motion\_cost\_per\_row 

Sets the legacy query optimizer \(planner\) cost estimate for a Motion operator to transfer a row from one segment to another, measured as a fraction of the cost of a sequential page fetch. If 0, then the value used is two times the value of *cpu\_tuple\_cost*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|0|master<br/>session<br/>reload|

## gp\_num\_contents\_in\_cluster 

The number of primary segments in the Greenplum Database system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|-|-|read only|

## gp\_recursive\_cte\_prototype 

Controls the availability of the `RECURSIVE` keyword \(Beta\) in the `WITH` clause of a `SELECT[ INTO]` command. The keyword allows a subquery in the `WITH` clause of a `SELECT[ INTO]` command to reference itself. The default value is `false`, the keyword is not allowed in the `WITH` clause a `SELECT[ INTO]` command.

**Note:** The `RECURSIVE` keyword is a Beta feature.

For information about the `RECURSIVE` keyword \(Beta\), see the `SELECT` command.

The parameter can be set for a database system, an individual database, or a session or query.

**Note:** This parameter will be removed if the `RECURSIVE` keyword is promoted from Beta or if the keyword is removed from Greenplum Database.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>session<br/>restart|

## gp\_reject\_percent\_threshold 

For single row error handling on COPY and external table SELECTs, sets the number of rows processed before SEGMENT REJECT LIMIT *n* PERCENT starts calculating.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-*n*|300|master<br/>session<br/>reload|

## gp\_reraise\_signal 

If enabled, will attempt to dump core if a fatal server error occurs.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_resgroup\_memory\_policy 

**Note:** The `gp_resgroup_memory_policy` server configuration parameter is enforced only when resource group-based resource management is active.

Used by a resource group to manage memory allocation to query operators.

When set to `auto`, Greenplum Database uses resource group memory limits to distribute memory across query operators, allocating a fixed size of memory to non-memory-intensive operators and the rest to memory-intensive operators.

When you specify `eager_free`, Greenplum Database distributes memory among operators more optimally by re-allocating memory released by operators that have completed their processing to operators in a later query stage.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|auto, eager\_free|eager\_free|local<br/>system<br/>superuser<br/>restart/reload<br/>|

## gp\_resource\_group\_bypass 

**Note:** The `gp_resource_group_bypass` server configuration parameter is enforced only when resource group-based resource management is active.

Enables or disables the enforcement of resource group concurrent transaction limits on Greenplum Database resources. The default value is `false`, which enforces resource group transaction limits. Resource groups manage resources such as CPU, memory, and the number of concurrent transactions that are used by queries and external components such as PL/Container.

You can set this parameter to `true` to bypass resource group concurrent transaction limitations so that a query can run immediately. For example, you can set the parameter to `true` for a session to run a system catalog query or a similar query that requires a minimal amount of resources.

When you set this parameter to `true` and a run a query, the query runs in this environment:

-   The query runs inside a resource group. The resource group assignment for the query does not change.
-   The query memory quota is approximately 10 MB per query. The memory is allocated from resource group shared memory or global shared memory. The query fails if there is not enough shared memory available to fulfill the memory allocation request.

This parameter can be set for a session. The parameter cannot be set within a transaction or a function.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|session|

## gp\_resource\_group\_cpu\_limit 

**Note:** The `gp_resource_group_cpu_limit` server configuration parameter is enforced only when resource group-based resource management is active.

Identifies the maximum percentage of system CPU resources to allocate to resource groups on each Greenplum Database segment node.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0.1 - 1.0|0.9|local<br/>system<br/>restart|

## gp\_resource\_group\_memory\_limit 

**Note:** The `gp_resource_group_memory_limit` server configuration parameter is enforced only when resource group-based resource management is active.

Identifies the maximum percentage of system memory resources to allocate to resource groups on each Greenplum Database segment node.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0.1 - 1.0|0.7|local<br/>system<br/>restart|

**Note:** When resource group-based resource management is active, the memory allotted to a segment host is equally shared by active primary segments. Greenplum Database assigns memory to primary segments when the segment takes the primary role. The initial memory allotment to a primary segment does not change, even in a failover situation. This may result in a segment host utilizing more memory than the `gp_resource_group_memory_limit` setting permits.

For example, suppose your Greenplum Database cluster is utilizing the default `gp_resource_group_memory_limit` of `0.7` and a segment host named `seghost1` has 4 primary segments and 4 mirror segments. Greenplum Database assigns each primary segment on `seghost1` `(0.7 / 4 = 0.175%)` of overall system memory. If failover occurs and two mirrors on `seghost1` fail over to become primary segments, each of the original 4 primaries retain their memory allotment of `0.175`, and the two new primary segments are each allotted `(0.7 / 6 = 0.116%)` of system memory. `seghost1`'s overall memory allocation in this scenario is

```

0.7 + (0.116 * 2) = 0.932%
```

which is above the percentage configured in the `gp_resource_group_memory_limit` setting.

## gp\_resource\_manager 

Identifies the resource management scheme currently enabled in the Greenplum Database cluster. The default scheme is to use resource queues. For information about Greenplum Database resource management, see [Managing Resources](../../admin_guide/wlmgmt.html).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|group<br/>queue|queue|local<br/>system<br/>restart|

## gp\_resqueue\_memory\_policy 

**Note:** The `gp_resqueue_memory_policy` server configuration parameter is enforced only when resource queue-based resource management is active.

Enables Greenplum memory management features. The distribution algorithm `eager_free` takes advantage of the fact that not all operators execute at the same time\(in Greenplum Database 4.2 and later\). The query plan is divided into stages and Greenplum Database eagerly frees memory allocated to a previous stage at the end of that stage's execution, then allocates the eagerly freed memory to the new stage.

When set to `none`, memory management is the same as in Greenplum Database releases prior to 4.1.

When set to `auto`, query memory usage is controlled by [statement\_mem](#statement_mem) and resource queue memory limits.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|none, auto, eager\_free|eager\_free|local<br/>system<br/>restart/reload|

## gp\_resqueue\_priority 

**Note:** The `gp_resqueue_priority` server configuration parameter is enforced only when resource queue-based resource management is active.

Enables or disables query prioritization. When this parameter is disabled, existing priority settings are not evaluated at query run time.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|local<br/>system<br/>restart|

## gp\_resqueue\_priority\_cpucores\_per\_segment 

**Note:** The `gp_resqueue_priority_cpucores_per_segment` server configuration parameter is enforced only when resource queue-based resource management is active.

Specifies the number of CPU units allocated per segment instance. For example, if a Greenplum Database cluster has 10-core segment hosts that are configured with four segments, set the value for the segment instances to 2.5. For the master instance, the value would be 10. A master host typically has only the master instance running on it, so the value for the master should reflect the usage of all available CPU cores.

Incorrect settings can result in CPU under-utilization or query prioritization not working as designed.

The default values for the Greenplum Data Computing Appliance are 4 for segments and 4 for the master.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0.1 - 512.0|4|local<br/>system<br/>restart|

## gp\_resqueue\_priority\_sweeper\_interval 

**Note:** The `gp_resqueue_priority_sweeper_interval` server configuration parameter is enforced only when resource queue-based resource management is active.

Specifies the interval at which the sweeper process evaluates current CPU usage. When a new statement becomes active, its priority is evaluated and its CPU share determined when the next interval is reached.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|500 - 15000 ms|1000|local<br/>system<br/>restart|

## gp\_role 

The role of this server process " set to *dispatch* for the master and *execute* for a segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|dispatch<br/>execute<br/>utility| |read only|

## gp\_safefswritesize 

Specifies a minimum size for safe write operations to append-optimized tables in a non-mature file system. When a number of bytes greater than zero is specified, the append-optimized writer adds padding data up to that number in order to prevent data corruption due to file system errors. Each non-mature file system has a known safe write size that must be specified here when using Greenplum Database with that type of file system. This is commonly set to a multiple of the extent size of the file system; for example, Linux ext3 is 4096 bytes, so a value of 32768 is commonly used.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|0|local<br/>system<br/>restart|

## gp\_segment\_connect\_timeout 

Time that the Greenplum interconnect will try to connect to a segment instance over the network before timing out. Controls the network connection timeout between master and primary segments, and primary to mirror segment replication processes.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|10 min|local<br/>system<br/>reload|

## gp\_segments\_for\_planner 

Sets the number of primary segment instances for the legacy query optimizer \(planner\) to assume in its cost and size estimates. If 0, then the value used is the actual number of primary segments. This variable affects the legacy optimizer's estimates of the number of rows handled by each sending and receiving process in Motion operators.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0-*n*|0|master<br/>session<br/>reload|

## gp\_server\_version 

Reports the version number of the server as a string. A version modifier argument might be appended to the numeric portion of the version string, example: *5.0.0 beta*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|String. Examples: *5.0.0*|n/a|read only|

## gp\_server\_version\_num 

Reports the version number of the server as an integer. The number is guaranteed to always be increasing for each version and can be used for numeric comparisons. The major version is represented as is, the minor and patch versions are zero-padded to always be double digit wide.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|*Mmmpp* where *M* is the major version, *mm* is the minor version zero-padded and *pp* is the patch version zero-padded. Example: 50000|n/a|read only|

## gp\_session\_id 

A system assigned ID number for a client session. Starts counting from 1 when the master instance is first started.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-*n*|14|read only|

## gp\_set\_proc\_affinity 

If enabled, when a Greenplum server process \(postmaster\) is started it will bind to a CPU.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|


## gp\_set\_read\_only 

Set to on to disable writes to the database. Any in progress transactions must finish before read-only mode takes affect.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|

## gp\_snmp\_community 

Set to the community name you specified for your environment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|SNMP community name|public|master<br/>system<br/>reload|

## gp\_snmp\_monitor\_address 

The hostname:port of your network monitor application. Typically, the port number is 162. If there are multiple monitor addresses, separate them with a comma.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|hostname:port| |master<br/>system<br/>reload|

## gp\_snmp\_use\_inform\_or\_trap 

Trap notifications are SNMP messages sent from one application to another \(for example, between Greenplum Database and a network monitoring application\). These messages are unacknowledged by the monitoring application, but generate less network overhead.

Inform notifications are the same as trap messages, except that the application sends an acknowledgement to the application that generated the alert.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|inform<br/>trap<br/>|trap|master<br/>system<br/>reload|

## gp\_statistics\_pullup\_from\_child\_partition 

Enables the use of statistics from child tables when planning queries on the parent table by the legacy query optimizer \(planner\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>system<br/>reload|

## gp\_statistics\_use\_fkeys 

When enabled, allows the legacy query optimizer \(planner\) to use foreign key information stored in the system catalog to optimize joins between foreign keys and primary keys.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## gp\_vmem\_idle\_resource\_timeout 

If a database session is idle for longer than the time specified, the session will free system resources \(such as shared memory\), but remain connected to the database. This allows more concurrent connections to the database at one time.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|18 s|master<br/>system<br/>reload|

## gp\_vmem\_protect\_limit 

**Note:** The `gp_vmem_protect_limit` server configuration parameter is enforced only when resource queue-based resource management is active.

Sets the amount of memory \(in number of MBs\) that all postgres processes of an active segment instance can consume. If a query causes this limit to be exceeded, memory will not be allocated and the query will fail. Note that this is a local parameter and must be set for every segment in the system \(primary and mirrors\). When setting the parameter value, specify only the numeric value. For example, to specify 4096MB, use the value `4096`. Do not add the units `MB` to the value.

To prevent over-allocation of memory, these calculations can estimate a safe `gp_vmem_protect_limit` value.

First calculate the value gp\_vmem. This is the Greenplum Database memory available on a host.

-   If the total system memory is less than 256 GB, use this formula:

    ```
    <gp_vmem> = ((<SWAP> + <RAM>) – (7.5GB + 0.05 * <RAM>)) / 1.7
    ```

-   If the total system memory is equal to or greater than 256 GB, use this formula:

    ```
    <gp_vmem> = ((<SWAP> + <RAM>) – (7.5GB + 0.05 * <RAM>)) / 1.17
    ```


where SWAP is the host swap space and RAM is the RAM on the host in GB.

Next, calculate the max\_acting\_primary\_segments. This is the maximum number of primary segments that can be running on a host when mirror segments are activated due to a failure. With mirrors arranged in a 4-host block with 8 primary segments per host, for example, a single segment host failure would activate two or three mirror segments on each remaining host in the failed host's block. The max\_acting\_primary\_segments value for this configuration is 11 \(8 primary segments plus 3 mirrors activated on failure\).

This is the calculation for `gp_vmem_protect_limit`. The value should be converted to MB.

```
`gp_vmem_protect_limit` = <gp_vmem> / <acting_primary_segments>
```

For scenarios where a large number of workfiles are generated, this is the calculation for gp\_vmem that accounts for the workfiles.

-   If the total system memory is less than 256 GB:

    ```
    gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM - (300KB * <total_#_workfiles>))) / 1.7
    ```

-   If the total system memory is equal to or greater than 256 GB:

    ```
    gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM - (300KB * <total_#_workfiles>))) / 1.17
    ```


For information about monitoring and managing workfile usage, see the *Greenplum Database Administrator Guide*.

Based on the gp\_vmem value you can calculate the value for the `vm.overcommit_ratio` operating system kernel parameter. This parameter is set when you configure each Greenplum Database host.

```
vm.overcommit_ratio = (<RAM> - (0.026 * <gp_vmem>)) / <RAM>
```

**Note:** The default value for the kernel parameter `vm.overcommit_ratio` in Red Hat Enterprise Linux is 50.

For information about the kernel parameter, see the *Greenplum Database Installation Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|8192|local<br/>system<br/>restart|

## gp\_vmem\_protect\_segworker\_cache\_limit 

If a query executor process consumes more than this configured amount, then the process will not be cached for use in subsequent queries after the process completes. Systems with lots of connections or idle processes may want to reduce this number to free more memory on the segments. Note that this is a local parameter and must be set for every segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of megabytes|500|local<br/>system<br/>restart|

## gp\_workfile\_checksumming 

Adds a checksum value to each block of a work file \(or spill file\) used by `HashAgg` and `HashJoin` query operators. This adds an additional safeguard from faulty OS disk drivers writing corrupted blocks to disk. When a checksum operation fails, the query will cancel and rollback rather than potentially writing bad data to disk.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## gp\_workfile\_compress\_algorithm 

When a hash aggregation or hash join operation spills to disk during query processing, specifies the compression algorithm to use on the spill files. If using zlib, the zlib library must be installed on all hosts in the Greenplum Database cluster.

If your Greenplum Database installation uses serial ATA \(SATA\) disk drives, setting the value of this parameter to `zlib` might help to avoid overloading the disk subsystem with IO operations.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|none<br/>zlib<br/>|none|master<br/>session<br/>reload|

## gp\_workfile\_limit\_files\_per\_query 

Sets the maximum number of temporary spill files \(also known as workfiles\) allowed per query per segment. Spill files are created when executing a query that requires more memory than it is allocated. The current query is terminated when the limit is exceeded.

Set the value to 0 \(zero\) to allow an unlimited number of spill files. master session reload

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|100000|master<br/>session<br/>reload|

## gp\_workfile\_limit\_per\_query 

Sets the maximum disk size an individual query is allowed to use for creating temporary spill files at each segment. The default value is 0, which means a limit is not enforced.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|kilobytes|0|master<br/>session<br/>reload|

## gp\_workfile\_limit\_per\_segment 

Sets the maximum total disk size that all running queries are allowed to use for creating temporary spill files at each segment. The default value is 0, which means a limit is not enforced.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|kilobytes|0|local<br/>system<br/>restart|

## gpperfmon\_port 

Sets the port on which all data collection agents \(for Command Center\) communicate with the master.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|8888|master<br/>system<br/>restart|

## ignore\_checksum\_failure 

Only has effect if [data\_checksums](#data_checksums) is enabled.

Greenplum Database uses checksums to prevent loading data that has been corrupted in the file system into memory managed by database processes.

By default, when a checksum verify error occurs when reading a heap data page, Greenplum Database generates an error and prevents the page from being loaded into managed memory. When `ignore_checksum_failure` is set to on and a checksum verify failure occurs, Greenplum Database generates a warning, and allows the page to be read into managed memory. If the page is then updated it is saved to disk and replicated to the mirror. If the page header is corrupt an error is reported even if this option is enabled.

**Warning:** Setting `ignore_checksum_failure` to on may propagate or hide data corruption or lead to other serious problems. However, if a checksum failure has already been detected and the page header is uncorrupted, setting `ignore_checksum_failure` to on may allow you to bypass the error and recover undamaged tuples that may still be present in the table.

The default setting is off, and it can only be changed by a superuser.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## integer\_datetimes 

Reports whether PostgreSQL was built with support for 64-bit-integer dates and times.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|read only|

## IntervalStyle 

Sets the display format for interval values. The value *sql\_standard* produces output matching SQL standard interval literals. The value *postgres* produces output matching PostgreSQL releases prior to 8.4 when the [DateStyle](#DateStyle) parameter was set to ISO.

The value *postgres\_verbose* produces output matching Greenplum releases prior to 3.3 when the [DateStyle](#DateStyle) parameter was set to non-ISO output.

The value *iso\_8601* will produce output matching the time interval *format with designators* defined in section 4.4.3.2 of ISO 8601. See the [PostgreSQL 8.4 documentation](https://www.postgresql.org/docs/8.4/static/datatype-datetime.html) for more information.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|postgres<br/>postgres\_verbose<br/>sql\_standard<br/>iso\_8601|postgres|master<br/>session<br/>reload|

## join\_collapse\_limit 

The legacy query optimizer \(planner\) will rewrite explicit inner `JOIN` constructs into lists of `FROM` items whenever a list of no more than this many items in total would result. By default, this variable is set the same as *from\_collapse\_limit*, which is appropriate for most uses. Setting it to 1 prevents any reordering of inner JOINs. Setting this variable to a value between 1 and *from\_collapse\_limit* might be useful to trade off planning time against the quality of the chosen plan \(higher values produce better plans\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|1-*n*|20|master<br/>session<br/>reload|

## keep\_wal\_segments 

For Greenplum Database master mirroring, sets the maximum number of processed WAL segment files that are saved by the by the active Greenplum Database master if a checkpoint operation occurs.

The segment files are used to sycnronize the active master on the standby master.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|5|master<br/>session<br/>reload<br/>superuser|

## krb\_caseins\_users 

Sets whether Kerberos user names should be treated case-insensitively. The default is case sensitive \(off\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|

## krb\_server\_keyfile 

Sets the location of the Kerberos server key file.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|path and file name|unset|master<br/>system<br/>restart|

## krb\_srvname 

Sets the Kerberos service name.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|service name|postgres|master<br/>system<br/>restart|

## lc\_collate 

Reports the locale in which sorting of textual data is done. The value is determined when the Greenplum Database array is initialized.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |read only|

## lc\_ctype 

Reports the locale that determines character classifications. The value is determined when the Greenplum Database array is initialized.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |read only|

## lc\_messages 

Sets the language in which messages are displayed. The locales available depends on what was installed with your operating system - use *locale -a* to list available locales. The default value is inherited from the execution environment of the server. On some systems, this locale category does not exist. Setting this variable will still work, but there will be no effect. Also, there is a chance that no translated messages for the desired language exist. In that case you will continue to see the English messages.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |local<br/>system<br/>restart|

## lc\_monetary 

Sets the locale to use for formatting monetary amounts, for example with the *to\_char* family of functions. The locales available depends on what was installed with your operating system - use *locale -a* to list available locales. The default value is inherited from the execution environment of the server.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |local<br/>system<br/>restart|

## lc\_numeric 

Sets the locale to use for formatting numbers, for example with the *to\_char* family of functions. The locales available depends on what was installed with your operating system - use *locale -a* to list available locales. The default value is inherited from the execution environment of the server.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |local<br/>system<br/>restart|

## lc\_time 

This parameter currently does nothing, but may in the future.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>| |local<br/>system<br/>restart|

## listen\_addresses 

Specifies the TCP/IP address\(es\) on which the server is to listen for connections from client applications - a comma-separated list of host names and/or numeric IP addresses. The special entry \* corresponds to all available IP interfaces. If the list is empty, only UNIX-domain sockets can connect.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|localhost,<br/>host names,<br/>IP addresses,<br/>\* \(all available IP interfaces\)|\*|master<br/>system<br/>restart|

## local\_preload\_libraries 

Comma separated list of shared library files to preload at the start of a client session.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
| | |local<br/>system<br/>restart|

## log\_autostats 

Logs information about automatic `ANALYZE` operations related to [gp\_autostats\_mode](#gp_autostats_mode)and [gp\_autostats\_on\_change\_threshold](#gp_autostats_on_change_threshold).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload<br/>superuser|

## log\_connections 

This outputs a line to the server log detailing each successful connection. Some client programs, like psql, attempt to connect twice while determining if a password is required, so duplicate "connection received" messages do not always indicate a problem.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## log\_disconnections 

This outputs a line in the server log at termination of a client session, and includes the duration of the session.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## log\_dispatch\_stats 

When set to "on," this parameter adds a log message with verbose information about the dispatch of the statement.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## log\_duration 

Causes the duration of every completed statement which satisfies *log\_statement* to be logged.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload<br/>superuser|

## log\_error\_verbosity 

Controls the amount of detail written in the server log for each message that is logged.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|TERSE<br/>DEFAULT<br/>VERBOSE|DEFAULT|master<br/>session<br/>reload<br/>superuser|

## log\_executor\_stats 

For each query, write performance statistics of the query executor to the server log. This is a crude profiling instrument. Cannot be enabled together with *log\_statement\_stats*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## log\_hostname 

By default, connection log messages only show the IP address of the connecting host. Turning on this option causes logging of the IP address and host name of the Greenplum Database master. Note that depending on your host name resolution setup this might impose a non-negligible performance penalty.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|

## log\_min\_duration\_statement 

Logs the statement and its duration on a single log line if its duration is greater than or equal to the specified number of milliseconds. Setting this to 0 will print all statements and their durations. -1 disables the feature. For example, if you set it to 250 then all SQL statements that run 250ms or longer will be logged. Enabling this option can be useful in tracking down unoptimized queries in your applications.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of milliseconds,<br/>0, -1|-1|master<br/>session<br/>reload<br/>superuser|

## log\_min\_error\_statement 

Controls whether or not the SQL statement that causes an error condition will also be recorded in the server log. All SQL statements that cause an error of the specified level or higher are logged. The default is ERROR. To effectively turn off logging of failing statements, set this parameter to PANIC.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|DEBUG5<br/>DEBUG4<br/>DEBUG3<br/>DEBUG2<br/>DEBUG1<br/>INFO<br/>NOTICE<br/>WARNING<br/>ERROR<br/>FATAL<br/>PANIC|ERROR|master<br/>session<br/>reload<br/>superuser|

## log\_min\_messages 

Controls which message levels are written to the server log. Each level includes all the levels that follow it. The later the level, the fewer messages are sent to the log.

If the Greenplum Database PL/Container extension is installed. This parameter also controls the PL/Container log level. For information about the extension, see [PL/Container Language](../extensions/pl_container.html).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|DEBUG5<br/>DEBUG4<br/>DEBUG3<br/>DEBUG2<br/>DEBUG1<br/>INFO<br/>NOTICE<br/>WARNING<br/>ERROR<br/>FATAL<br/>PANIC|WARNING|master<br/>session<br/>reload<br/>superuser|

## log\_parser\_stats 

For each query, write performance statistics of the query parser to the server log. This is a crude profiling instrument. Cannot be enabled together with *log\_statement\_stats*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload<br/>superuser|

## log\_planner\_stats 

For each query, write performance statistics of the legacy query optimizer \(planner\) to the server log. This is a crude profiling instrument. Cannot be enabled together with *log\_statement\_stats*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload<br/>superuser|

## log\_rotation\_age 

Determines the maximum lifetime of an individual log file. After this time has elapsed, a new log file will be created. Set to zero to disable time-based creation of new log files.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Any valid time expression<br/>\(number and unit\)|1 d|local<br/>system<br/>restart|

## log\_rotation\_size 

Determines the maximum size of an individual log file. After this many kilobytes have been emitted into a log file, a new log file will be created. Set to zero to disable size-based creation of new log files.

The maximum value is INT\_MAX/1024. If an invalid value is specified, the default value is used. INT\_MAX is the largest value that can be stored as an integer on your system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of kilobytes|0|local<br/>system<br/>restart|

## log\_statement 

Controls which SQL statements are logged. DDL logs all data definition commands like CREATE, ALTER, and DROP commands. MOD logs all DDL statements, plus INSERT, UPDATE, DELETE, TRUNCATE, and COPY FROM. PREPARE and EXPLAIN ANALYZE statements are also logged if their contained command is of an appropriate type.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|NONE<br/>DDL<br/>MOD<br/>ALL|ALL|master<br/>session<br/>reload<br/>superuser|

## log\_statement\_stats 

For each query, write total performance statistics of the query parser, planner, and executor to the server log. This is a crude profiling instrument.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload<br/>superuser|

## log\_temp\_files 

Controls logging of temporary file names and sizes. Temporary files can be created for sorts, hashes, temporary query results and spill files. A log entry is made in `pg_log` for each temporary file when it is deleted. Depending on the source of the temporary files, the log entry could be created on either the master and/or segments. A `log_temp_files` value of zero logs all temporary file information, while positive values log only files whose size is greater than or equal to the specified number of kilobytes. The default setting is `-1`, which disables logging. Only superusers can change this setting.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Integer|-1|local<br/>system<br/>restart|

## log\_timezone 

Sets the time zone used for timestamps written in the log. Unlike [TimeZone](#TimeZone), this value is system-wide, so that all sessions will report timestamps consistently. The default is `unknown`, which means to use whatever the system environment specifies as the time zone.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string|unknown|local<br/>system<br/>restart|

## log\_truncate\_on\_rotation 

Truncates \(overwrites\), rather than appends to, any existing log file of the same name. Truncation will occur only when a new file is being opened due to time-based rotation. For example, using this setting in combination with a log\_filename such as `gpseg#-%H.log` would result in generating twenty-four hourly log files and then cyclically overwriting them. When off, pre-existing files will be appended to in all cases.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|local<br/>system<br/>restart|

## max\_appendonly\_tables 

Sets the maximum number of concurrent transactions that can write to or update append-optimized tables. Transactions that exceed the maximum return an error.

Operations that are counted are `INSERT`, `UPDATE`, `COPY`, and `VACUUM` operations. The limit is only for in-progress transactions. Once a transaction ends \(either aborted or committed\), it is no longer counted against this limit.

For operations against a partitioned table, each subpartition \(child table\) that is an append-optimized table and is changed counts as a single table towards the maximum. For example, a partitioned table `p_tbl` is defined with three subpartitions that are append-optimized tables `p_tbl_ao1`, `p_tbl_ao2`, and `p_tbl_ao3`. An `INSERT` or `UPDATE` command against the partitioned table `p_tbl` that changes append-optimized tables `p_tbl_ao1` and `p_tbl_ao2` is counted as two transactions.

Increasing the limit allocates more shared memory on the master host at server start.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|10000|master<br/>system<br/>restart|

## max\_connections 

The maximum number of concurrent connections to the database server. In a Greenplum Database system, user client connections go through the Greenplum master instance only. Segment instances should allow 5-10 times the amount as the master. When you increase this parameter, [max\_prepared\_transactions](#max_prepared_transactions) must be increased as well. For more information about limiting concurrent connections, see "Configuring Client Authentication" in the *Greenplum Database Administrator Guide*.

Increasing this parameter might cause Greenplum Database to request more shared memory. See [shared\_buffers](#shared_buffers) for information about Greenplum server instance shared memory buffers. If this value is increased, [shared\_buffers](#shared_buffers) and the kernel parameter `SHMMAX` might also need to be increased. Also, parameters that depend on shared memory such as [max\_fsm\_pages](#max_fsm_pages) might need to be changed.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|10 - 65535|250 on master<br/>750 on segments|local<br/>system<br/>restart|

## max\_files\_per\_process 

Sets the maximum number of simultaneously open files allowed to each server subprocess. If the kernel is enforcing a safe per-process limit, you don't need to worry about this setting. Some platforms such as BSD, the kernel will allow individual processes to open many more files than the system can really support.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1000|local<br/>system<br/>restart|

## max\_fsm\_pages 

Sets the maximum number of disk pages for which free space will be tracked in the shared free-space map. Six bytes of shared memory are consumed for each page slot. If this value is increased, [shared\_buffers](#shared_buffers) and the kernel parameter `SHMMAX` might also need to be increased. Also, parameters that depend on shared memory such as [max\_connections](#max_connections) might need to be changed.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 16 \*<br/>*max\_fsm\_relations*|200000|local<br/>system<br/>restart|

## max\_fsm\_relations 

Sets the maximum number of relations for which free space will be tracked in the shared memory free-space map. Should be set to a value larger than the total number of:

```
tables + indexes + system tables
```

It costs about 60 bytes of memory for each relation per segment instance. It is better to allow some room for overhead and set too high rather than too low.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1000|local<br/>system<br/>restart|

## max\_function\_args 

Reports the maximum number of function arguments.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|100|read only|

## max\_identifier\_length 

Reports the maximum identifier length.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|63|read only|

## max\_index\_keys 

Reports the maximum number of index keys.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|32|read only|

## max\_locks\_per\_transaction 

The shared lock table is created with room to describe locks on *max\_locks\_per\_transaction* \* \(*max\_connections* + *max\_prepared\_transactions*\) objects, so no more than this many distinct objects can be locked at any one time. This is not a hard limit on the number of locks taken by any one transaction, but rather a maximum average value. You might need to raise this value if you have clients that touch many different tables in a single transaction.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|128|local<br/>system<br/>restart|

## max\_prepared\_transactions 

Sets the maximum number of transactions that can be in the prepared state simultaneously. Greenplum uses prepared transactions internally to ensure data integrity across the segments. This value must be at least as large as the value of [max\_connections](#max_connections) on the master. Segment instances should be set to the same value as the master.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer <= 1000|250 on master<br/>250 on segments|local<br/>system<br/>restart|

## max\_resource\_portals\_per\_transaction 

**Note:** The `max_resource_portals_per_transaction` server configuration parameter is enforced only when resource queue-based resource management is active.

Sets the maximum number of simultaneously open user-declared cursors allowed per transaction. Note that an open cursor will hold an active query slot in a resource queue. Used for resource management.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|64|master<br/>system<br/>restart|

## max\_resource\_queues 

**Note:** The `max_resource_queues` server configuration parameter is enforced only when resource queue-based resource management is active.

Sets the maximum number of resource queues that can be created in a Greenplum Database system. Note that resource queues are system-wide \(as are roles\) so they apply to all databases in the system.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|9|master<br/>system<br/>restart|

## max\_stack\_depth 

Specifies the maximum safe depth of the server's execution stack. The ideal setting for this parameter is the actual stack size limit enforced by the kernel \(as set by *ulimit -s* or local equivalent\), less a safety margin of a megabyte or so. Setting the parameter higher than the actual kernel limit will mean that a runaway recursive function can crash an individual backend process.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of kilobytes|2 MB|local<br/>system<br/>restart|

## max\_statement\_mem 

Sets the maximum memory limit for a query. Helps avoid out-of-memory errors on a segment host during query processing as a result of setting [statement\_mem](#statement_mem) too high.

Taking into account the configuration of a single segment host, calculate `max_statement_mem` as follows:

`(seghost_physical_memory) / (average_number_concurrent_queries)`

When changing both `max_statement_mem` and `statement_mem`, `max_statement_mem` must be changed first, or listed first in the postgresql.conf file.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of kilobytes|2000 MB|master<br/>session<br/>reload<br/>superuser|

## memory\_spill\_ratio 

**Note:** The `memory_spill_ratio` server configuration parameter is enforced only when resource group-based resource management is active.

Sets the memory usage threshold percentage for memory-intensive operators in a transaction. When a transaction reaches this threshold, it spills to disk.

The default `memory_spill_ratio` percentage is the value defined for the resource group assigned to the currently active role. You can set `memory_spill_ratio` at the session level to selectively set this limit on a per-query basis. For example, if you have a specific query that spills to disk and requires more memory, you may choose to set a larger `memory_spill_ratio` to increase the initial memory allocation.

When you set `memory_spill_ratio` at the session level, Greenplum Database does not perform semantic validation on the new value until you next perform a query.

You can specify an integer percentage value from 0 to 100 inclusive. If you specify a value of 0, Greenplum Database uses the [`statement_mem`](#statement_mem) server configuration parameter value to control the initial query operator memory amount.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - 100|20|master<br/>session<br/>reload|

## optimizer 

Enables or disables GPORCA when running SQL queries. The default is `on`. If you disable GPORCA, Greenplum Database uses only the legacy query optimizer.

GPORCA co-exists with the legacy query optimizer. With GPORCA enabled, Greenplum Database uses GPORCA to generate an execution plan for a query when possible. If GPORCA cannot be used, then the legacy query optimizer is used.

The optimizer parameter can be set for a database system, an individual database, or a session or query.

For information about the legacy query optimizer and GPORCA, see [Querying Data](../../admin_guide/query/topics/query.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## optimizer\_analyze\_root\_partition 

For a partitioned table, controls whether the `ROOTPARTITION` keyword is required to collect root partition statistics when the ANALYZE command is run on the table. GPORCA uses the root partition statistics when generating a query plan. The legacy query optimizer does not use these statistics.

The default setting for the parameter is `on`, the `ANALYZE` command can collect root partition statistics without the `ROOTPARTITION` keyword. Root partition statistics are collected when you run `ANALYZE` on the root partition, or when you run `ANALYZE` on a child leaf partition of the partitioned table and the other child leaf partitions have statistics. When the value is `off`, you must run `ANALZYE ROOTPARTITION` to collect root partition statistics.

When the value of the server configuration parameter [optimizer](#optimizer) is `on` \(the default\), the value of this parameter should also be `on`. For information about collecting table statistics on partitioned tables, see [ANALYZE](../sql_commands/ANALYZE.html).

For information about the legacy query optimizer and GPORCA, see [Querying Data](../../admin_guide/query/topics/query.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## optimizer\_array\_expansion\_threshold 

When GPORCA is enabled \(the default\) and is processing a query that contains a predicate with a constant array, the `optimizer_array_expansion_threshold` parameter limits the optimization process based on the number of constants in the array. If the array in the query predicate contains more than the number elements specified by parameter, GPORCA disables the transformation of the predicate into its disjunctive normal form during query optimization.

The default value is 100.

For example, when GPORCA is executing a query that contains an `IN` clause with more than 100 elements, GPORCA does not transform the predicate into its disjunctive normal form during query optimization to reduce optimization time consume less memory. The difference in query processing can be seen in the filter condition for the `IN` clause of the query `EXPLAIN` plan.

Changing the value of this parameter changes the trade-off between a shorter optimization time and lower memory consumption, and the potential benefits from constraint derivation during query optimization, for example conflict detection and partition elimination.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Integer \> 0|100|master<br/>session<br/>reload|

## optimizer\_control 

Controls whether the server configuration parameter optimizer can be changed with SET, the RESET command, or the Greenplum Database utility gpconfig. If the optimizer\_control parameter value is `on`, users can set the optimizer parameter. If the optimizer\_control parameter value is `off`, the optimizer parameter cannot be changed.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>system<br/>restart<br/>superuser|
## optimizer\_cost\_model 

When GPORCA is enabled \(the default\), this parameter controls the cost model that GPORCA chooses for bitmap scans used with bitmap indexes or with btree indexes on AO tables.

The `experimental` cost model is more likely to choose a faster bitmap index with nested loop joins instead of hash joins.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|legacy<br/>calibrated<br/>experimental|calibrated|master<br/>session<br/>reload|

## optimizer\_cte\_inlining\_bound 

When GPORCA is enabled \(the default\), this parameter controls the amount of inlining performed for common table expression \(CTE\) queries \(queries that contain a `WHERE` clause\). The default value, 0, disables inlining.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Decimal \>= 0|0|master<br/>session<br/>reload|

## optimizer\_dpe\_stats 

When GPORCA is enabled \(the default\) and this parameter is `true` \(the default\), GPORCA derives statistics that allow it to more accurately estimate the number of rows to be scanned during dynamic partition elimination.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_enable\_associativity 

When GPORCA is enabled \(the default\), this parameter controls whether the join associativity transform is enabled during query optimization. The transform analyzes join orders. For the default value `off`, only the GPORCA dynamic programming algorithm for analyzing join orders is enabled. The join associativity transform largely duplicates the functionality of the newer dynamic programming algorithm.

If the value is `on`, GPORCA can use the associativity transform during query optimization.

The parameter can be set for a database system, an individual database, or a session or query.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## optimizer\_enable\_dml 

When GPORCA is enabled \(the default\) and this parameter is `true` \(the default\), GPORCA attempts to execute DML commands such as `INSERT`, `UPDATE`, and `DELETE`. If GPORCA cannot execute the command, Greenplum Database falls back to the Postgres Planner.

When set to `false`, Greenplum Database always falls back to the Postgres Planner when performing DML commands.

The parameter can be set for a database system, an individual database, or a session or query.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_enable\_master\_only\_queries 

When GPORCA is enabled \(the default\), this parameter allows GPORCA to execute catalog queries that run only on the Greenplum Database master. For the default value `off`, only the Postgres Planner can execute catalog queries that run only on the Greenplum Database master.

The parameter can be set for a database system, an individual database, or a session or query.

**Note:** Enabling this parameter decreases performance of short running catalog queries. To avoid this issue, set this parameter only for a session or a query.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## optimizer\_force\_agg\_skew\_avoidance 

When GPORCA is enabled \(the default\), this parameter affects the query plan alternatives that GPORCA considers when 3 stage aggregate plans are generated. When the value is `true`, the default, GPORCA considers only 3 stage aggregate plans where the intermediate aggregation uses the `GROUP BY` and `DISTINCT` columns for distribution to reduce the effects of processing skew.

If the value is `false`, GPORCA can also consider a plan that uses `GROUP BY` columns for distribution. These plans might perform poorly when processing skew is present.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_force\_multistage\_agg 

For the default settings, GPORCA is enabled and this parameter is `true`, GPORCA chooses a 3 stage aggregate plan for scalar distinct qualified aggregate when such a plan alternative is generated. When the value is `false`, GPORCA makes a cost based choice rather than a heuristic choice.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_force\_three\_stage\_scalar\_dqa 

For the default settings, GPORCA is enabled and this parameter is `true`, GPORCA chooses a plan with multistage aggregates when such a plan alternative is generated. When the value is `false`, GPORCA makes a cost based choice rather than a heuristic choice.

The parameter can be set for a database system, an individual database, or a session, or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_join\_arity\_for\_associativity\_commutativity 

The value is an optimization hint to limit the number of join associativity and join commutativity transformations explored during query optimization. The limit controls the alternative plans that GPORCA considers during query optimization. For example, the default value of 18 is an optimization hint for GPORCA to stop exploring join associativity and join commutativity transformations when an n-ary join operator has more than 18 children during optimization.

For a query with a large number of joins, specifying a lower value improves query performance by limiting the number of alternate query plans that GPORCA evaluates. However, setting the value too low might cause GPORCA to generate a query plan that performs sub-optimally.

This parameter has no effect when the `optimizer_join_order` parameter is set to `query` or `greedy`.

This parameter can be set for a database system or a session.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|18|local<br/>system<br/>restart|

## optimizer\_join\_order 

When GPORCA is enabled, this parameter sets the optimization level for join ordering during query optimization by specifying which types of join ordering alternatives to evaluate.

-   `query` - Uses the join order specified in the query.
-   `greedy` - Evaluates the join order specified in the query and alternatives based on minimum cardinalities of the relations in the joins.
-   `exhaustive` - Applies transformation rules to find and evaluate all join ordering alternatives.

The default value is `exhaustive`. Setting this parameter to `query` or `greedy` can generate a suboptimal query plan. However, if the administrator is confident that a satisfactory plan is generated with the `query` or `greedy` setting, query optimization time can be improved by setting the parameter to the lower optimization level.

Setting this parameter to `query` or `greedy` overrides the `optimizer_join_order_threshold` and `optimizer_join_arity_for_associativity_commutativity` parameters.

This parameter can be set for an individual database, a session, or a query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|query<br/>greedy<br/>exhaustive|exhaustive|master<br/>session<br/>reload|

## optimizer\_join\_order\_threshold 

When GPORCA is enabled \(the default\), this parameter sets the maximum number of join children for which GPORCA will use the dynamic programming-based join ordering algorithm. You can set this value for a single query or for an entire session.

This parameter has no effect when the `optimizer_join_query` parameter is set to `query` or `greedy`.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - 12|10|master<br/>session<br/>reload|

## optimizer\_mdcache\_size 

Sets the maximum amount of memory on the Greenplum Database master that GPORCA uses to cache query metadata \(optimization data\) during query optimization. The memory limit session based. GPORCA caches query metadata during query optimization with the default settings: GPORCA is enabled and [optimizer\_metadata\_caching](#optimizer_metadata_caching) is `on`.

The default value is 16384 \(16MB\). This is an optimal value that has been determined through performance analysis.

You can specify a value in KB, MB, or GB. The default unit is KB. For example, a value of 16384 is 16384KB. A value of 1GB is the same as 1024MB or 1048576KB. If the value is 0, the size of the cache is not limited.

This parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Integer \>= 0|16384|master<br/>session<br/>reload|

## optimizer\_metadata\_caching 

When GPORCA is enabled \(the default\), this parameter specifies whether GPORCA caches query metadata \(optimization data\) in memory on the Greenplum Database master during query optimization. The default for this parameter is `on`, enable caching. The cache is session based. When a session ends, the cache is released. If the amount of query metadata exceeds the cache size, then old, unused metadata is evicted from the cache.

If the value is `off`, GPORCA does not cache metadata during query optimization.

This parameter can be set for a database system, an individual database, or a session or query.

The server configuration parameter [optimizer\_mdcache\_size](#optimizer_mdcache_size) controls the size of the query metadata cache.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## optimizer\_minidump 

GPORCA generates minidump files to describe the optimization context for a given query. The information in the file is not in a format that can be easily used for debugging or troubleshooting. The minidump file is located under the master data directory and uses the following naming format:

`Minidump_date_time.mdp`

The minidump file contains this query related information:

-   Catalog objects including data types, tables, operators, and statistics required by GPORCA
-   An internal representation \(DXL\) of the query
-   An internal representation \(DXL\) of the plan produced by GPORCA
-   System configuration information passed to GPORCA such as server configuration parameters, cost and statistics configuration, and number of segments
-   A stack trace of errors generated while optimizing the query

Setting this parameter to `ALWAYS` generates a minidump for all queries. Set this parameter to `ONERROR` to minimize total optimization time.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|ONERROR<br/>ALWAYS|ONERROR|master<br/>session<br/>reload|

## optimizer\_nestloop\_factor 

This parameter adds a costing factor to GPORCA to prioritize hash joins instead of nested loop joins during query optimization. The default value of 1024 was chosen after evaluating numerous workloads with uniformly distributed data. 1024 should be treated as the practical upper bound setting for this parameter. If you find the GPORCA selects hash joins more often than it should, reduce the value to shift the costing factor in favor of nested loop joins.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|INT\_MAX \> 1|1024|master<br/>session<br/>reload|

## optimizer\_parallel\_union 

When GPORCA is enabled \(the default\), `optimizer_parallel_union` controls the amount of parallelization that occurs for queries that contain a `UNION` or `UNION ALL` clause.

When the value is `off`, the default, GPORCA generates a query plan where each child of an APPEND\(UNION\) operator is in the same slice as the APPEND operator. During query execution, the children are executed in a sequential manner.

When the value is `on`, GPORCA generates a query plan where a redistribution motion node is under an APPEND\(UNION\) operator. During query execution, the children and the parent APPEND operator are on different slices, allowing the children of the APPEND\(UNION\) operator to execute in parallel on segment instances.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## optimizer\_penalize\_skew 

When GPORCA is enabled \(the default\), this parameter allows GPORCA to penalize the local cost of a HashJoin with a skewed Redistribute Motion as child to favor a Broadcast Motion during query optimization. The default value is `true`.

GPORCA determines there is skew for a Redistribute Motion when the NDV \(number of distinct values\) is less than the number of segments.

The parameter can be set for a database system, an individual database, or a session or query.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>session<br/>reload|

## optimizer\_print\_missing\_stats 

When GPORCA is enabled \(the default\), this parameter controls the display of table column information about columns with missing statistics for a query. The default value is `true`, display the column information to the client. When the value is `false`, the information is not sent to the client.

The information is displayed during query execution, or with the `EXPLAIN` or `EXPLAIN ANALYZE` commands.

The parameter can be set for a database system, an individual database, or a session.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## optimizer\_print\_optimization\_stats 

When GPORCA is enabled \(the default\), this parameter enables logging of GPORCA query optimization statistics for various optimization stages for a query. The default value is off, do not log optimization statistics. To log the optimization statistics, this parameter must be set to on and the parameter client\_min\_messages must be set to log.

-   `set optimizer_print_optimization_stats = on;`
-   `set client_min_messages = 'log';`

The information is logged during query execution, or with the `EXPLAIN` or `EXPLAIN ANALYZE` commands.

This parameter can be set for a database system, an individual database,or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## optimizer\_sort\_factor 

When GPORCA is enabled \(the default\), `optimizer_sort_factor` controls the cost factor to apply to sorting operations during query optimization. The default value `1` specifies the default sort cost factor. The value is a ratio of increase or decrease from the default factor. For example, a value of `2.0` sets the cost factor at twice the default, and a value of `0.5` sets the factor at half the default.

The parameter can be set for a database system, an individual database, or a session or query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Decimal \> 0|1|master<br/>session<br/>reload|

## optimizer\_use\_gpdb\_allocators 

When GPORCA is enabled \(the default\) and this parameter is `true`, GPORCA uses Greenplum Database memory management when executing queries. The default is `false`, GPORCA uses GPORCA-specific memory management. Greenplum Database memory management allows for faster optimization, reduced memory usage during optimization, and improves GPORCA support of vmem limits when compared to GPORCA-specific memory management.

For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>system<br/>restart|

## optimizer\_xform\_bind\_threshold 

When GPORCA is enabled \(the default\), this parameter controls the maximum number of bindings per transform that GPORCA produces per group expression. Setting this parameter limits the number of alternatives that GPORCA creates, in many cases reducing the optimization time and overall memory usage of queries that include deeply nested expressions.

The default value is `0`, GPORCA produces an unlimited set of bindings.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - INT\_MAX|0|master<br/>session<br/>reload|

## password\_encryption 

When a password is specified in CREATE USER or ALTER USER without writing either ENCRYPTED or UNENCRYPTED, this option determines whether the password is to be encrypted.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## password\_hash\_algorithm 

Specifies the cryptographic hash algorithm that is used when storing an encrypted Greenplum Database user password. The default algorithm is MD5.

For information about setting the password hash algorithm to protect user passwords, see "Protecting Passwords in Greenplum Database" in the *Greenplum Database Administrator Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|MD5<br/>SHA-256|MD5|master<br/>session<br/>reload<br/>superuser|

## pgcrypto.fips 

Enables Greenplum Database support for a limited set of FIPS encryption functionality \(*Federal Information Processing Standard* \(FIPS\) 140-2\). For information about FIPS, see [https://www.nist.gov/itl/popular-links/federal-information-processing-standards-fips](https://www.nist.gov/itl/popular-links/federal-information-processing-standards-fips). The default is `off`, encryption is not enabled.

Before enabling this parameter, ensure that FIPS is enabled on the Greenplum Database system hosts.

When this parameter is enabled, these changes occur:

-   FIPS mode is initialized in the OpenSSL library
-   The functions digest\(\) and hmac\(\) allow only the SHA encryption algorithm \(MD5 is not allowed\)
-   The functions for the crypt and gen\_salt algorithms are disabled
-   PGP encryption and decryption functions support only AES and 3DES encryption algorithms \(other algorithms such as blowfish are not allowed\)
-   RAW encryption and decryption functions support only AES and 3DES \(other algorithms such as blowfish are not allowed\)

**To enable `pgcrypto.fips`**

1.  Enable the `pgcrypto` functions as an extension if it is not enabled. See [pgcrypto Cryptographic Functions](/vmware/install_guide/install_pgcrypto.html).This example `psql` command creates the `pgcrypto` extension in the database `testdb`.

    ```
    psql -d testdb -c 'CREATE EXTENSION pgcrypto'
    ```

2.  Configure the Greenplum Database server configuration parameter `shared_preload_libraries` to load the `pgrcypto` library. This example uses the `gpconfig` utility to update the parameter in the Greenplum Database `postgresql.conf` files.

    ```
    gpconfig -c shared_preload_libraries -v '\$libdir/pgcrypto'
    ```

    This command displays the value of `shared_preload_libraries`.

    ```
    gpconfig -s shared_preload_libraries
    ```

3.  Restart the Greenplum Database system.

    ```
    gpstop -ra 
    ```

4.  Set the `pgcrypto.fips` server configuration parameter to `on` for each database that uses FIPS encryption. For example, this command sets the parameter to `on` for the database `testdb`.

    ```
    ALTER DATABASE testdb SET pgcrypto.fips TO on;
    ```

    **Important:** You must use the `ALTER DATABASE` command to set the parameter. You cannot use the `SET` command that updates the parameter for a session, or use the `gpconfig` utility that updates `postgresql.conf` files.

5.  After setting the parameter, reconnect to the database to enable encryption support for a session. This example uses the `psql` meta command `\c` to connect to the `testdb` database.

    ```
    \c testdb
    ```


**To disable `pgcrypto.fips`**

1.  If the database does not use `pgcrypto` functions, disable the `pgcrypto` extension. See [pgcrypto Cryptographic Functions](/vmware/install_guide/install_pgcrypto.html).This example `psql` command drops the `pgcrypto` extension in the database `testdb`.

    ```
    psql -d testdb -c 'DROP EXTENSION pgcrypto'
    ```

2.  Remove `\$libdir/pgcrypto` from the `shared_preload_libraries` parameter, and restart Greenplum Database. This `gpconfig` command displays the value of the parameter from the Greenplum Database `postgresql.conf` files.

    ```
    gpconfig -s shared_preload_libraries
    ```

    Use the `gpconfig` utility with the `-c` and `-v` options to change the value of the parameter. Use the `-r` option to remove the parameter.

3.  Restart the Greenplum Database system.

    ```
    gpstop -ra 
    ```


|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|see description|

## pgstat\_track\_activity\_query\_size 

Sets the maximum length limit for the query text stored in `current_query` column of the system catalog view pg\_stat\_activity. The minimum length is 1024 characters.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1024|local<br/>system<br/>restart|

## pljava\_classpath 

A colon \(`:`\) separated list of jar files or directories containing jar files needed for PL/Java functions. The full path to the jar file or directory must be specified, except the path can be omitted for jar files in the `$GPHOME/lib/postgresql/java` directory. The jar files must be installed in the same locations on all Greenplum hosts and readable by the `gpadmin` user.

The `pljava_classpath` parameter is used to assemble the PL/Java classpath at the beginning of each user session. Jar files added after a session has started are not available to that session.

If the full path to a jar file is specified in `pljava_classpath` it is added to the PL/Java classpath. When a directory is specified, any jar files the directory contains are added to the PL/Java classpath. The search does not descend into subdirectories of the specified directories. If the name of a jar file is included in `pljava_classpath` with no path, the jar file must be in the `$GPHOME/lib/postgresql/java` directory.

**Note:** Performance can be affected if there are many directories to search or a large number of jar files.

If [pljava\_classpath\_insecure](#pljava_classpath_insecure) is `false`, setting the `pljava_classpath` parameter requires superuser privilege. Setting the classpath in SQL code will fail when the code is executed by a user without superuser privilege. The `pljava_classpath` parameter must have been set previously by a superuser or in the `postgresql.conf` file. Changing the classpath in the `postgresql.conf` file requires a reload \(`gpstop -u`\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>session<br/>reload<br/>superuser|

## pljava\_classpath\_insecure 

Controls whether the server configuration parameter [pljava\_classpath](#pljava_classpath) can be set by a user without Greenplum Database superuser privileges. When `true`, `pljava_classpath` can be set by a regular user. Otherwise, [pljava\_classpath](#pljava_classpath) can be set only by a database superuser. The default is `false`.

**Warning:** Enabling this parameter exposes a security risk by giving non-administrator database users the ability to run unauthorized Java methods.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|false|master<br/>session<br/>restart<br/>superuser|

## pljava\_statement\_cache\_size 

Sets the size in KB of the JRE MRU \(Most Recently Used\) cache for prepared statements.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of kilobytes|10|master<br/>system<br/>restart<br/>superuser|

## pljava\_release\_lingering\_savepoints 

If true, lingering savepoints used in PL/Java functions will be released on function exit. If false, savepoints will be rolled back.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>system<br/>restart<br/>superuser|

## pljava\_vmoptions 

Defines the startup options for the Java VM. The default value is an empty string \(""\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string| |master<br/>system<br/>reload<br/>superuser|

## port 

The database listener port for a Greenplum instance. The master and each segment has its own port. Port numbers for the Greenplum system must also be changed in the `gp_segment_configuration` catalog. You must shut down your Greenplum Database system before changing port numbers.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|any valid port number|5432|local<br/>system<br/>restart|

## random\_page\_cost 

Sets the estimate of the cost of a nonsequentially fetched disk page for the legacy query optimizer \(planner\). This is measured as a multiple of the cost of a sequential page fetch. A higher value makes it more likely a sequential scan will be used, a lower value makes it more likely an index scan will be used.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|100|master<br/>session<br/>reload|

## readable\_external\_table\_timeout 

When an SQL query reads from an external table, the parameter value specifies the amount of time in seconds that Greenplum Database waits before cancelling the query when data stops being returned from the external table.

The default value of 0, specifies no time out. Greenplum Database does not cancel the query.

If queries that use gpfdist run a long time and then return the error "intermittent network connectivity issues", you can specify a value for readable\_external\_table\_timeout. If no data is returned by gpfdist for the specified length of time, Greenplum Database cancels the query.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \>= 0|0|master<br/>system<br/>reload|

## repl\_catchup\_within\_range 

For Greenplum Database master mirroring, controls updates to the active master. If the number of WAL segment files that have not been processed by the `walsender` exceeds this value, Greenplum Database updates the active master.

If the number of segment files does not exceed the value, Greenplum Database blocks updates to the to allow the `walsender` process the files. If all WAL segments have been processed, the active master is updated.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - 64|1|master<br/>system<br/>reload<br/>superuser|

## replication\_timeout 

For Greenplum Database master mirroring, sets the maximum time in milliseconds that the `walsender` process on the active master waits for a status message from the `walreceiver` process on the standby master. If a message is not received, the `walsender` logs an error message.

The [wal\_receiver\_status\_interval](#wal_receiver_status_interval) controls the interval between `walreceiver` status messages.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|0 - INT\_MAX|60000 ms \(60 seconds\)|master<br/>system<br/>reload<br/>superuser|

## regex\_flavor 

The 'extended' setting may be useful for exact backwards compatibility with pre-7.4 releases of PostgreSQL.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|advanced<br/>extended<br/>basic|advanced|master<br/>system<br/>reload|

## resource\_cleanup\_gangs\_on\_wait 

**Note:** The `resource_cleanup_gangs_on_wait` server configuration parameter is enforced only when resource queue-based resource management is active.

If a statement is submitted through a resource queue, clean up any idle query executor worker processes before taking a lock on the resource queue.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>system<br/>restart|

## resource\_select\_only 

**Note:** The `resource_select_only` server configuration parameter is enforced only when resource queue-based resource management is active.

Sets the types of queries managed by resource queues. If set to on, then `SELECT`, `SELECT INTO`, `CREATE TABLE AS SELECT`, and `DECLARE CURSOR` commands are evaluated. If set to off `INSERT`, `UPDATE`, and `DELETE` commands will be evaluated as well.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|

## runaway\_detector\_activation\_percent 

For queries that are managed by resource queues or resource groups, this parameter determines when Greenplum Database terminates running queries based on the amount of memory the queries are using. A value of 100 disables the automatic termination of queries based on the percentage of memory that is utilized.

Either the resource queue or the resource group management scheme can be active in Greenplum Database; both schemes cannot be active at the same time. The server configuration parameter [gp\_resource\_manager](#gp_resource_manager) controls which scheme is active.

**When resource queues are enabled -** This parameter sets the percent of utilized Greenplum Database vmem memory that triggers the termination of queries. If the percentage of vmem memory that is utilized for a Greenplum Database segment exceeds the specified value, Greenplum Database terminates queries managed by resource queues based on memory usage, starting with the query consuming the largest amount of memory. Queries are terminated until the percentage of utilized vmem is below the specified percentage.

Specify the maximum vmem value for active Greenplum Database segment instances with the server configuration parameter [gp\_vmem\_protect\_limit](#gp_vmem_protect_limit).

For example, if vmem memory is set to 10GB, and this parameter is 90 \(90%\), Greenplum Database starts terminating queries when the utilized vmem memory exceeds 9 GB.

For information about resource queues, see [Using Resource Queues](../../admin_guide/workload_mgmt.html).

**When resource groups are enabled -** This parameter sets the percent of utilized resource group global shared memory that triggers the termination of queries that are managed by resource groups that are configured to use the `vmtracker` memory auditor, such as `admin_group` and `default_group`. For information about memory auditors, see [Memory Auditor](../../admin_guide/workload_mgmt_resgroups.html).

Resource groups have a global shared memory pool when the sum of the `MEMORY_LIMIT` attribute values configured for all resource groups is less than 100. For example, if you have 3 resource groups configured with `memory_limit` values of 10 , 20, and 30, then global shared memory is 40% = 100% - \(10% + 20% + 30%\). See [Global Shared Memory](../../admin_guide/workload_mgmt_resgroups.html).

If the percentage of utilized global shared memory exceeds the specified value, Greenplum Database terminates queries based on memory usage, selecting from queries managed by the resource groups that are configured to use the `vmtracker` memory auditor. Greenplum Database starts with the query consuming the largest amount of memory. Queries are terminated until the percentage of utilized global shared memory is below the specified percentage.

For example, if global shared memory is 10GB, and this parameter is 90 \(90%\), Greenplum Database starts terminating queries when the utilized global shared memory exceeds 9 GB.

For information about resource groups, see [Using Resource Groups](../../admin_guide/workload_mgmt_resgroups.html).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|percentage<br/>\(integer\)|90|local<br/>system<br/>restart|

## search\_path 

Specifies the order in which schemas are searched when an object is referenced by a simple name with no schema component. When there are objects of identical names in different schemas, the one found first in the search path is used. The system catalog schema, *pg\_catalog*, is always searched, whether it is mentioned in the path or not. When objects are created without specifying a particular target schema, they will be placed in the first schema listed in the search path. The current effective value of the search path can be examined via the SQL function *current\_schemas\(\)*. *current\_schemas\(\)* shows how the requests appearing in *search\_path* were resolved.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|a comma-separated list of schema names|$user,public|master<br/>session<br/>reload|

## seq\_page\_cost 

For the legacy query optimizer \(planner\), sets the estimate of the cost of a disk page fetch that is part of a series of sequential fetches.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|floating point|1|master<br/>session<br/>reload|

## server\_encoding 

Reports the database encoding \(character set\). It is determined when the Greenplum Database array is initialized. Ordinarily, clients need only be concerned with the value of *client\_encoding*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|<system dependent\>|UTF8|read only|

## server\_version 

Reports the version of PostgreSQL that this release of Greenplum Database is based on.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string|8.3.23|read only|

## server\_version\_num 

Reports the version of PostgreSQL that this release of Greenplum Database is based on as an integer.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|80323|read only|

## shared\_buffers 

Sets the amount of memory a Greenplum Database segment instance uses for shared memory buffers. This setting must be at least 128KB and at least 16KB times [max\_connections](#max_connections).

Each Greenplum Database segment instance calculates and attempts to allocate certain amount of shared memory based on the segment configuration. The value of `shared_buffers` is significant portion of this shared memory calculation, but is not all it. When setting `shared_buffers`, the values for the operating system parameters `SHMMAX` or `SHMALL` might also need to be adjusted.

The operating system parameter `SHMMAX` specifies maximum size of a single shared memory allocation. The value of `SHMMAX` must be greater than this value:

```
 `shared_buffers` + <other_seg_shmem>
```

The value of other\_seg\_shmem is the portion the Greenplum Database shared memory calculation that is not accounted for by the `shared_buffers` value. The other\_seg\_shmem value will vary based on the segment configuration.

With the default Greenplum Database parameter values, the value for other\_seg\_shmem is approximately 111MB for Greenplum Database segments and approximately 79MB for the Greenplum Database master.

The operating system parameter `SHMALL` specifies the maximum amount of shared memory on the host. The value of `SHMALL` must be greater than this value:

```
 (<num_instances_per_host> * ( `shared_buffers` + <other_seg_shmem> )) + <other_app_shared_mem> 
```

The value of other\_app\_shared\_mem is the amount of shared memory that is used by other applications and processes on the host.

When shared memory allocation errors occur, possible ways to resolve shared memory allocation issues are to increase `SHMMAX` or `SHMALL`, or decrease `shared_buffers` or `max_connections`.

See the *Greenplum Database Installation Guide* for information about the Greenplum Database values for the parameters `SHMMAX` and `SHMALL`.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 16K \* *max\_connections*|125 MB|local<br/>system<br/>restart|

## shared\_preload\_libraries 

A comma-separated list of shared libraries that are to be preloaded at server start. PostgreSQL procedural language libraries can be preloaded in this way, typically by using the syntax '`$libdir/plXXX`' where XXX is pgsql, perl, tcl, or python. By preloading a shared library, the library startup time is avoided when the library is first used. If a specified library is not found, the server will fail to start.

**Note:** When you add a library to `shared_preload_libraries`, be sure to retain any previous setting of the parameter.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
| | |local<br/>system<br/>restart|

## ssl 

Enables SSL connections.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>system<br/>restart|

## ssl\_ciphers 

Specifies a list of SSL ciphers that are allowed to be used on secure connections. See the openssl manual page for a list of supported ciphers.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string|ALL|master<br/>system<br/>restart|

## standard\_conforming\_strings 

Determines whether ordinary string literals \('...'\) treat backslashes literally, as specified in the SQL standard. The default value is on. Turn this parameter off to treat backslashes in string literals as escape characters instead of literal backslashes. Applications may check this parameter to determine how string literals are processed. The presence of this parameter can also be taken as an indication that the escape string syntax \(E'...'\) is supported.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|master<br/>session<br/>reload|

## statement\_mem 

Allocates segment host memory per query. The amount of memory allocated with this parameter cannot exceed [max\_statement\_mem](#max_statement_mem) or the memory limit on the resource queue or resource group through which the query was submitted. If additional memory is required for a query, temporary spill files on disk are used.

*If you are using resource groups to control resource allocation in your Greenplum Database cluster*:

-   Greenplum Database uses `statement_mem` to control query memory usage when the resource group `MEMORY_SPILL_RATIO` is set to 0.
-   You can use the following calculation to estimate a reasonable `statement_mem` value:

    ```
    rg_perseg_mem = ((RAM * (vm.overcommit_ratio / 100) + SWAP) * gp_resource_group_memory_limit) / num_active_primary_segments_per_host
    statement_mem = rg_perseg_mem / max_expected_concurrent_queries
    ```


*If you are using resource queues to control resource allocation in your Greenplum Database cluster*:

-   When [gp\_resqueue\_memory\_policy](#gp_resqueue_memory_policy) =auto, `statement_mem` and resource queue memory limits control query memory usage.
-   You can use the following calculation to estimate a reasonable `statement_mem` value for a wide variety of situations:

    ```
    ( <gp_vmem_protect_limit>GB * .9 ) / <max_expected_concurrent_queries>
    ```

    For example, with a gp\_vmem\_protect\_limit set to 8192MB \(8GB\) and assuming a maximum of 40 concurrent queries with a 10% buffer, you would use the following calculation to determine the `statement_mem` value:

    ```
    (8GB * .9) / 40 = .18GB = 184MB
    ```


When changing both `max_statement_mem` and `statement_mem`, `max_statement_mem` must be changed first, or listed first in the postgresql.conf file.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of kilobytes|128 MB|master<br/>session<br/>reload|

## statement\_timeout 

Abort any statement that takes over the specified number of milliseconds. 0 turns off the limitation.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of milliseconds|0|master<br/>session<br/>reload|

## stats\_queue\_level 

**Note:** The `stats_queue_level` server configuration parameter is enforced only when resource queue-based resource management is active.

Collects resource queue statistics on database activity.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## superuser\_reserved\_connections 

Determines the number of connection slots that are reserved for Greenplum Database superusers.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer < *max\_connections*|3|local<br/>system<br/>restart|

## tcp\_keepalives\_count 

How many keepalives may be lost before the connection is considered dead. A value of 0 uses the system default. If TCP\_KEEPCNT is not supported, this parameter must be 0.

Use this parameter for all connections that are not between a primary and mirror segment. Use gp\_filerep\_tcp\_keepalives\_count for settings that are between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of lost keepalives|0|local<br/>system<br/>restart|

## tcp\_keepalives\_idle 

Number of seconds between sending keepalives on an otherwise idle connection. A value of 0 uses the system default. If TCP\_KEEPIDLE is not supported, this parameter must be 0.

Use this parameter for all connections that are not between a primary and mirror segment. Use gp\_filerep\_tcp\_keepalives\_idle for settings that are between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of seconds|0|local<br/>system<br/>restart|

## tcp\_keepalives\_interval 

How many seconds to wait for a response to a keepalive before retransmitting. A value of 0 uses the system default. If TCP\_KEEPINTVL is not supported, this parameter must be 0.

Use this parameter for all connections that are not between a primary and mirror segment. Use gp\_filerep\_tcp\_keepalives\_interval for settings that are between a primary and mirror segment.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|number of seconds|0|local<br/>system<br/>restart|

## temp\_buffers 

Sets the maximum number of temporary buffers used by each database session. These are session-local buffers used only for access to temporary tables. The setting can be changed within individual sessions, but only up until the first use of temporary tables within a session. The cost of setting a large value in sessions that do not actually need a lot of temporary buffers is only a buffer descriptor, or about 64 bytes, per increment. However if a buffer is actually used, an additional 8192 bytes will be consumed.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1024|master<br/>session<br/>reload|

## TimeZone 

Sets the time zone for displaying and interpreting time stamps. The default is to use whatever the system environment specifies as the time zone. See [Date/Time Keywords](https://www.postgresql.org/docs/8.3/static/datetime-keywords.html) in the PostgreSQL documentation.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|time zone abbreviation| |local<br/>restart|

## timezone\_abbreviations 

Sets the collection of time zone abbreviations that will be accepted by the server for date time input. The default is `Default`, which is a collection that works in most of the world. `Australia` and `India`, and other collections can be defined for a particular installation. Possible values are names of configuration files stored in `$GPHOME/share/postgresql/timezonesets/`.

To configure Greenplum Database to use a custom collection of timezones, copy the file that contains the timezone definitions to the directory `$GPHOME/share/postgresql/timezonesets/` on the Greenplum Database master and segment hosts. Then set value of the server configuration parameter `timezone_abbreviations` to the file. For example, to use a file `custom` that contains the default timezones and the WIB \(Waktu Indonesia Barat\) timezone.

1.  Copy the file `Default` from the directory `$GPHOME/share/postgresql/timezonesets/` the file `custom`. Add the WIB timezone information from the file `Asia.txt` to the `custom`.
2.  Copy the file `custom` to the directory `$GPHOME/share/postgresql/timezonesets/` on the Greenplum Database master and segment hosts.
3.  Set value of the server configuration parameter `timezone_abbreviations` to `custom`.
4.  Reload the server configuration file \(`gpstop -u`\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|string|Default|master<br/>session<br/>reload|

## track\_activities 

Enables the collection of information on the currently executing command of each session, along with the time when that command began execution. This parameter is `true` by default. Only superusers can change this setting. See the `pg_stat_activity` view.

**Note:** Even when enabled, this information is not visible to all users, only to superusers and the user owning the session being reported on, so it should not represent a security risk.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>system<br/>reload<br/>superuser|

## track\_activity\_query\_size 

Sets the maximum length limit for the query text stored in `current_query` column of the system catalog view *pg\_stat\_activity*. The minimum length is 1024 characters.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer|1024|local<br/>system<br/>restart|

## track\_counts 

Collects information about executing commands. Enables the collection of information on the currently executing command of each session, along with the time at which that command began execution.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload<br/>superuser|

## transaction\_isolation 

Sets the current transaction's isolation level.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|read committed<br/>serializable<br/>|read committed|master<br/>session<br/>reload|

## transaction\_read\_only 

Sets the current transaction's read-only status.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## transform\_null\_equals 

When on, expressions of the form expr = NULL \(or NULL = expr\) are treated as expr IS NULL, that is, they return true if expr evaluates to the null value, and false otherwise. The correct SQL-spec-compliant behavior of expr = NULL is to always return null \(unknown\).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## unix\_socket\_directory 

Specifies the directory of the UNIX-domain socket on which the server is to listen for connections from client applications. The default is an empty string which is the directory `/tmp/`.

**Important:** Do not change the value of this parameter. The default location is required for Greenplum Database utilities.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|directory path|unset|local<br/>system<br/>restart|

## unix\_socket\_group 

Sets the owning group of the UNIX-domain socket. By default this is an empty string, which uses the default group for the current user.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|UNIX group name|unset|local<br/>system<br/>restart|

## unix\_socket\_permissions 

Sets the access permissions of the UNIX-domain socket. UNIX-domain sockets use the usual UNIX file system permission set. Note that for a UNIX-domain socket, only write permission matters.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|numeric UNIX file<br/>permission mode<br/>\(as accepted by the<br/>*chmod* or *umask* commands\)|511|local<br/>system<br/>restart|

## update\_process\_title 

Enables updating of the process title every time a new SQL command is received by the server. The process title is typically viewed by the `ps` command.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|on|local<br/>system<br/>restart|

## vacuum\_cost\_delay 

The length of time that the process will sleep when the cost limit has been exceeded. 0 disables the cost-based vacuum delay feature.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|milliseconds < 0<br/>\(in multiples of 10\)|0|local<br/>system<br/>restart|

## vacuum\_cost\_limit 

The accumulated cost that will cause the vacuuming process to sleep.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|200|local<br/>system<br/>restart|

## vacuum\_cost\_page\_dirty 

The estimated cost charged when vacuum modifies a block that was previously clean. It represents the extra I/O required to flush the dirty block out to disk again.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|20|local<br/>system<br/>restart|

## vacuum\_cost\_page\_hit 

The estimated cost for vacuuming a buffer found in the shared buffer cache. It represents the cost to lock the buffer pool, lookup the shared hash table and scan the content of the page.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|1|local<br/>system<br/>restart|

## vacuum\_cost\_page\_miss 

The estimated cost for vacuuming a buffer that has to be read from disk. This represents the effort to lock the buffer pool, lookup the shared hash table, read the desired block in from the disk and scan its content.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer \> 0|10|local<br/>system<br/>restart|

## vacuum\_freeze\_min\_age 

Specifies the cutoff age \(in transactions\) that `VACUUM` should use to decide whether to replace transaction IDs with *FrozenXID* while scanning a table.

For information about `VACUUM` and transaction ID management, see "Managing Data" in the *Greenplum Database Administrator Guide* and the [PostgreSQL documentation](https://www.postgresql.org/docs/8.3/static/routine-vacuuming.html#VACUUM-FOR-WRAPAROUND).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 0-100000000000|100000000|local<br/>system<br/>restart|

## validate\_previous\_free\_tid 

Enables a test that validates the free tuple ID \(TID\) list. The list is maintained and used by Greenplum Database. Greenplum Database determines the validity of the free TID list by ensuring the previous free TID of the current free tuple is a valid free tuple. The default value is `true`, enable the test.

If Greenplum Database detects a corruption in the free TID list, the free TID list is rebuilt, a warning is logged, and a warning is returned by queries for which the check failed. Greenplum Database attempts to execute the queries.

**Note:** If a warning is returned, please contact VMware Support.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## verify\_gpfdists\_cert 

When a Greenplum Database external table is defined with the `gpfdists` protocol to use SSL security, this parameter controls whether SSL certificate authentication is enabled. The default is `true`, SSL authentication is enabled when Greenplum Database communicates with the `gpfdist` utility to either read data from or write data to an external data source.

The value `false` disables SSL certificate authentication. These SSL exceptions are ignored:

-   The self-signed SSL certificate that is used by `gpfdist` is not trusted by Greenplum Database.
-   The host name contained in the SSL certificate does not match the host name that is running `gpfdist`.

You can set the value to `false` to disable authentication when testing the communication between the Greenplum Database external table and the `gpfdist` utility that is serving the external data.

**Warning:** Disabling SSL certificate authentication exposes a security risk by not validating the `gpfdists` SSL certificate.

For information about the `gpfdists` protocol, see [gpfdists:// Protocol](../../admin_guide/external/g-gpfdists-protocol.html). For information about running the `gpfdist` utility, see [gpfdist](../../utility_guide/admin_utilities/gpfdist.html).

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|true|master<br/>session<br/>reload|

## vmem\_process\_interrupt 

Enables checking for interrupts before reserving vmem memory for a query during Greenplum Database query execution. Before reserving further vmem for a query, check if the current session for the query has a pending query cancellation or other pending interrupts. This ensures more responsive interrupt processing, including query cancellation requests. The default is `off`.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|Boolean|off|master<br/>session<br/>reload|

## wal\_receiver\_status\_interval 

For Greenplum Database master mirroring, sets the interval in seconds between `walreceiver` process status messages that are sent to the active master. Under heavy loads, the time might be longer.

The value of [replication\_timeout](#replication_timeout) controls the time that the `walsender` process waits for a `walreceiver` message.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 0- INT\_MAX/1000|10 sec|master<br/>system<br/>reload<br/>superuser|

## writable\_external\_table\_bufsize 

Size of the buffer \(in KB\) that Greenplum Database uses for network communication, such as the `gpfdist` utility and external web tables \(that use http\). Greenplum Database stores data in the buffer before writing the data out. For information about `gpfdist`, see the *Greenplum Database Utility Guide*.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 32 - 131072<br/>\(32KB - 128MB\)|64|local<br/>session<br/>reload|
## xid\_stop\_limit 

The number of transaction IDs prior to the ID where transaction ID wraparound occurs. When this limit is reached, Greenplum Database stops creating new transactions to avoid data loss due to transaction ID wraparound.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 10000000 -<br/>2000000000|1000000000|local<br/>system<br/>restart|

## xid\_warn\_limit 

The number of transaction IDs prior to the limit specified by [xid\_stop\_limit](#xid_stop_limit). When Greenplum Database reaches this limit, it issues a warning to perform a VACUUM operation to avoid data loss due to transaction ID wraparound.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|integer 10000000 - <br/>2000000000|500000000|local<br/>system<br/>restart|

## xmlbinary 

Specifies how binary values are encoded in XML data. For example, when `bytea` values are converted to XML. The binary data can be converted to either base64 encoding or hexadecimal encoding. The default is base64.

The parameter can be set for a database system, an individual database, or a session.

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|base64<br/>hex<br/>|base64|master<br/>session<br/>reload|


## xmloption 

Specifies whether XML data is to be considered as an XML document \(`document`\) or XML content fragment \(`content`\) for operations that perform implicit parsing and serialization. The default is `content`.

This parameter affects the validation performed by `xml_is_well_formed()`. If the value is `document`, the function checks for a well-formed XML document. If the value is `content`, the function checks for a well-formed XML content fragment.

**Note:** An XML document that contains a document type declaration \(DTD\) is not considered a valid XML content fragment. If `xmloption` set to `content`, XML that contains a DTD is not considered valid XML.

To cast a character string that contains a DTD to the `xml` data type, use the `xmlparse` function with the `document` keyword, or change the `xmloption` value to `document`.

The parameter can be set for a database system, an individual database, or a session. The SQL command to set this option for a session is also available in Greenplum Database.

```
SET XML OPTION { DOCUMENT | CONTENT }
```

|Value Range|Default|Set Classifications|
|-----------|-------|-------------------|
|document<br/>content|content|master<br/>session<br/>reload|
 