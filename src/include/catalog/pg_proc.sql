-- All GPDB-added functions are here, instead of pg_proc.h. pg_proc.h should
-- kept as close as possible to the upstream version, to make merging easier.
--
-- This file is translated into DATA rows by catullus.pl. See
-- README.add_catalog_function for instructions on how to run it.

 CREATE FUNCTION float4_decum(_float8, float4) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'float4_decum' WITH (OID=6024, DESCRIPTION="aggregate inverse transition function");
 CREATE FUNCTION float4_avg_accum(bytea, float4) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float4_avg_accum' WITH (OID=4106, DESCRIPTION="aggregate transition function");
 CREATE FUNCTION float4_avg_decum(bytea, float4) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float4_avg_decum' WITH (OID=4107, DESCRIPTION="aggregate inverse transition function");
 CREATE FUNCTION float8_decum(_float8, float8) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'float8_decum' WITH (OID=6025, DESCRIPTION="aggregate inverse transition function");
 CREATE FUNCTION float8_avg_accum(bytea, float8) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float8_avg_accum' WITH (OID=4108, DESCRIPTION="aggregate transition function");
 CREATE FUNCTION float8_avg_decum(bytea, float8) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float8_avg_decum' WITH (OID=4109, DESCRIPTION="aggregate inverse transition function");
 CREATE FUNCTION btgpxlogloccmp(gpxlogloc, gpxlogloc) RETURNS int4 LANGUAGE internal IMMUTABLE STRICT AS 'btgpxlogloccmp' WITH (OID=7081, DESCRIPTION="btree less-equal-greater");

-- MPP -- array_add -- special for prospective customer 
 CREATE FUNCTION array_add(_int4, _int4) RETURNS _int4 LANGUAGE internal IMMUTABLE STRICT AS 'array_int4_add' WITH (OID=6012, DESCRIPTION="itemwise add two integer arrays");

 CREATE FUNCTION string_agg_transfn(internal, text) RETURNS internal LANGUAGE internal IMMUTABLE AS 'string_agg_transfn' WITH (OID=3534, DESCRIPTION="string_agg(text) transition function");

 CREATE FUNCTION string_agg_delim_transfn(internal, text, text) RETURNS internal LANGUAGE internal IMMUTABLE AS 'string_agg_delim_transfn' WITH (OID=3535, DESCRIPTION="string_agg(text, text) transition function");

 CREATE FUNCTION string_agg_finalfn(internal) RETURNS text LANGUAGE internal IMMUTABLE AS 'string_agg_finalfn' WITH (OID=3536, DESCRIPTION="string_agg final function");

 CREATE FUNCTION string_agg(text) RETURNS text LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3537, DESCRIPTION="concatenate aggregate input into an string", proisagg="t");

 CREATE FUNCTION string_agg(text, text) RETURNS text LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3538, DESCRIPTION="concatenate aggregate input into an string with delimiter", proisagg="t");

 CREATE FUNCTION int8dec(int8) RETURNS int8 LANGUAGE internal IMMUTABLE STRICT AS 'int8dec' WITH (OID=3546);


 CREATE FUNCTION interval_interval_div("interval", "interval") RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'interval_interval_div' WITH (OID=6115, DESCRIPTION="divide");

 CREATE FUNCTION interval_interval_mod("interval", "interval") RETURNS "interval" LANGUAGE internal IMMUTABLE STRICT AS 'interval_interval_mod' WITH (OID=6116, DESCRIPTION="modulus");

-- System-view support functions 
 CREATE FUNCTION pg_get_partition_def(oid) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_def' WITH (OID=5024);

 CREATE FUNCTION pg_get_partition_def(oid, bool) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_def_ext' WITH (OID=5025, DESCRIPTION="partition configuration for a given relation");

 CREATE FUNCTION pg_get_partition_def(oid, bool, bool) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_def_ext2' WITH (OID=5034, DESCRIPTION="partition configuration for a given relation");

 CREATE FUNCTION pg_get_partition_rule_def(oid) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_rule_def' WITH (OID=5027);

 CREATE FUNCTION pg_get_partition_rule_def(oid, bool) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_rule_def_ext' WITH (OID=5028, DESCRIPTION="partition configuration for a given rule");

 CREATE FUNCTION pg_get_partition_template_def(oid, bool, bool) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_get_partition_template_def' WITH (OID=5037, DESCRIPTION="ALTER statement to recreate subpartition templates for a give relation");

 CREATE FUNCTION numeric_dec("numeric") RETURNS "numeric" LANGUAGE internal IMMUTABLE STRICT AS 'numeric_dec' WITH (OID=6997, DESCRIPTION="increment by one");


-- Sequences and time series
 CREATE FUNCTION interval_bound(numeric, numeric) RETURNS numeric LANGUAGE internal IMMUTABLE STRICT AS 'numeric_interval_bound' WITH (OID=7082, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(numeric, numeric, int4) RETURNS numeric LANGUAGE internal IMMUTABLE AS 'numeric_interval_bound_shift' WITH (OID=7083, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(numeric, numeric, int4, numeric) RETURNS numeric LANGUAGE internal IMMUTABLE AS 'numeric_interval_bound_shift_rbound' WITH (OID=7084, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamp, "interval") RETURNS timestamp LANGUAGE internal IMMUTABLE STRICT AS 'timestamp_interval_bound' WITH (OID=7085, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamp, "interval", int4) RETURNS timestamp LANGUAGE internal IMMUTABLE AS 'timestamp_interval_bound_shift' WITH (OID=7086, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamp, "interval", int4, timestamp) RETURNS timestamp LANGUAGE internal IMMUTABLE AS 'timestamp_interval_bound_shift_reg' WITH (OID=7087, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamptz, "interval") RETURNS timestamptz LANGUAGE internal IMMUTABLE STRICT AS 'timestamptz_interval_bound' WITH (OID=7088, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamptz, "interval", int4) RETURNS timestamptz LANGUAGE internal IMMUTABLE AS 'timestamptz_interval_bound_shift' WITH (OID=7089, DESCRIPTION="boundary of the interval containing the given value");

 CREATE FUNCTION interval_bound(timestamptz, "interval", int4, timestamptz) RETURNS timestamptz LANGUAGE internal IMMUTABLE AS 'timestamptz_interval_bound_shift_reg' WITH (OID=7090, DESCRIPTION="boundary of the interval containing the given value");


-- Aggregate-related functions 
 CREATE FUNCTION numeric_avg_accum(bytea, "numeric") RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'numeric_avg_accum' WITH (OID=4102, DESCRIPTION="aggregate transition function");

 CREATE FUNCTION numeric_decum(_numeric, "numeric") RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'numeric_decum' WITH (OID=7309, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION numeric_avg_decum(bytea, "numeric") RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'numeric_avg_decum' WITH (OID=4103, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION int2_decum(_numeric, int2) RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'int2_decum' WITH (OID=7306, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION int4_decum(_numeric, int4) RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'int4_decum' WITH (OID=7307, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION int8_decum(_numeric, int8) RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'int8_decum' WITH (OID=7308, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION int2_invsum(int8, int2) RETURNS int8 LANGUAGE internal IMMUTABLE AS 'int2_invsum' WITH (OID=7008, DESCRIPTION="SUM(int2) inverse transition function");

 CREATE FUNCTION int4_invsum(int8, int4) RETURNS int8 LANGUAGE internal IMMUTABLE AS 'int4_invsum' WITH (OID=7009, DESCRIPTION="SUM(int4) inverse transition function");

 CREATE FUNCTION int8_invsum("numeric", int8) RETURNS "numeric" LANGUAGE internal IMMUTABLE AS 'int8_invsum' WITH (OID=7010, DESCRIPTION="SUM(int8) inverse transition function");

 CREATE FUNCTION interval_decum(_interval, "interval") RETURNS _interval LANGUAGE internal IMMUTABLE STRICT AS 'interval_decum' WITH (OID=6038, DESCRIPTION="aggregate inverse transition function");

 CREATE FUNCTION int8_avg_accum(bytea, int8) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int8_avg_accum' WITH (OID=4100, DESCRIPTION="AVG(int8) transition function");

 CREATE FUNCTION int2_avg_decum(bytea, int2) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int2_avg_decum' WITH (OID=6019, DESCRIPTION="AVG(int2) transition function");

 CREATE FUNCTION int4_avg_decum(bytea, int4) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int4_avg_decum' WITH (OID=6020, DESCRIPTION="AVG(int4) transition function");

 CREATE FUNCTION int8_avg_decum(bytea, int8) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int8_avg_decum' WITH (OID=4101, DESCRIPTION="AVG(int8) transition function");

 CREATE FUNCTION pg_stat_get_backend_waiting_reason(int4) RETURNS text LANGUAGE internal STABLE STRICT AS 'pg_stat_get_backend_waiting_reason' WITH (OID=7298, DESCRIPTION="Statistics: Reason backend is waiting for");

 CREATE FUNCTION pg_stat_get_queue_num_exec(oid) RETURNS int8 LANGUAGE internal STABLE STRICT AS 'pg_stat_get_queue_num_exec' WITH (OID=6031, DESCRIPTION="Statistics: Number of queries that executed in queue");

 CREATE FUNCTION pg_stat_get_queue_num_wait(oid) RETURNS int8 LANGUAGE internal STABLE STRICT AS 'pg_stat_get_queue_num_wait' WITH (OID=6032, DESCRIPTION="Statistics: Number of queries that waited in queue");

 CREATE FUNCTION pg_stat_get_queue_elapsed_exec(oid) RETURNS int8 LANGUAGE internal STABLE STRICT AS 'pg_stat_get_queue_elapsed_exec' WITH (OID=6033, DESCRIPTION="Statistics:  Elapsed seconds for queries that executed in queue");

 CREATE FUNCTION pg_stat_get_queue_elapsed_wait(oid) RETURNS int8 LANGUAGE internal STABLE STRICT AS 'pg_stat_get_queue_elapsed_wait' WITH (OID=6034, DESCRIPTION="Statistics:  Elapsed seconds for queries that waited in queue");

 CREATE FUNCTION pg_stat_get_backend_session_id(int4) RETURNS int4 LANGUAGE internal STABLE STRICT AS 'pg_stat_get_backend_session_id' WITH (OID=6039, DESCRIPTION="Statistics: Greenplum session id of backend");

 CREATE FUNCTION pg_renice_session(int4, int4) RETURNS int4 LANGUAGE internal VOLATILE STRICT AS 'pg_renice_session' WITH (OID=6042, DESCRIPTION="change priority of all the backends for a given session id");

 CREATE FUNCTION pg_stat_get_wal_senders(OUT pid int4, OUT state text, OUT sent_location text, OUT write_location text, OUT flush_location text, OUT replay_location text, OUT sync_priority int4, OUT sync_state text) RETURNS SETOF pg_catalog.record LANGUAGE internal STABLE AS 'pg_stat_get_wal_senders' WITH (OID=3099, DESCRIPTION="statistics: information about currently active replication");

 CREATE FUNCTION pg_terminate_backend(int4, text) RETURNS bool LANGUAGE internal VOLATILE STRICT AS 'pg_terminate_backend_msg' WITH (OID=951, DESCRIPTION="terminate a server process");

 CREATE FUNCTION pg_resgroup_get_status_kv(IN prop_in text, OUT rsgid oid, OUT prop text, OUT value text) RETURNS SETOF pg_catalog.record LANGUAGE internal VOLATILE AS 'pg_resgroup_get_status_kv' WITH (OID=6065, DESCRIPTION="statistics: information about resource groups in key-value style");

 CREATE FUNCTION pg_resgroup_get_status(IN groupid oid, OUT groupid oid, OUT num_running int4, OUT num_queueing int4, OUT num_queued int4, OUT num_executed int4, OUT total_queue_duration interval, OUT cpu_usage json, OUT memory_usage json) RETURNS SETOF pg_catalog.record LANGUAGE internal VOLATILE AS 'pg_resgroup_get_status' WITH (OID=6066, DESCRIPTION="statistics: information about resource groups");

 CREATE FUNCTION pg_resqueue_status() RETURNS SETOF record LANGUAGE internal VOLATILE STRICT AS 'pg_resqueue_status' WITH (OID=6030, DESCRIPTION="Return resource queue information");

 CREATE FUNCTION pg_resqueue_status_kv() RETURNS SETOF record LANGUAGE internal VOLATILE STRICT AS 'pg_resqueue_status_kv' WITH (OID=6069, DESCRIPTION="Return resource queue information");

 CREATE FUNCTION pg_file_read(text, int8, int8) RETURNS text LANGUAGE internal VOLATILE STRICT AS 'pg_read_file' WITH (OID=6045, DESCRIPTION="Read text from a file");

 CREATE FUNCTION pg_logfile_rotate() RETURNS bool LANGUAGE internal VOLATILE STRICT AS 'pg_rotate_logfile' WITH (OID=6046, DESCRIPTION="Rotate log file");

 CREATE FUNCTION pg_file_write(text, text, bool) RETURNS int8 LANGUAGE internal VOLATILE STRICT AS 'pg_file_write' WITH (OID=6047, DESCRIPTION="Write text to a file");

 CREATE FUNCTION pg_file_rename(text, text, text) RETURNS bool LANGUAGE internal VOLATILE AS 'pg_file_rename' WITH (OID=6048, DESCRIPTION="Rename a file");

 CREATE FUNCTION pg_file_unlink(text) RETURNS bool LANGUAGE internal VOLATILE STRICT AS 'pg_file_unlink' WITH (OID=6049, DESCRIPTION="Delete (unlink) a file");

 CREATE FUNCTION pg_logdir_ls() RETURNS SETOF record LANGUAGE internal VOLATILE STRICT AS 'pg_logdir_ls' WITH (OID=6050, DESCRIPTION="ls the log dir");

 CREATE FUNCTION pg_file_length(text) RETURNS int8 LANGUAGE internal VOLATILE STRICT AS 'pg_file_length' WITH (OID=6051, DESCRIPTION="Get the length of a file (via stat)");

-- Aggregates (moved here from pg_aggregate for 7.3) 

 CREATE FUNCTION max(gpxlogloc) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3332, proisagg="t");

 CREATE FUNCTION min(gpxlogloc) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3333, proisagg="t");

-- MPP Aggregate -- array_sum -- special for prospective customer. 
 CREATE FUNCTION array_sum(_int4) RETURNS _int4 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=6013, proisagg="t");

-- Greenplum Analytic functions
 CREATE FUNCTION int2_matrix_accum(_int8, _int2) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'matrix_add' WITH (OID=3212, DESCRIPTION="perform matrix addition on two conformable matrices");

 CREATE FUNCTION int4_matrix_accum(_int8, _int4) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'matrix_add' WITH (OID=3213, DESCRIPTION="perform matrix addition on two conformable matrices");

 CREATE FUNCTION int8_matrix_accum(_int8, _int8) RETURNS _int8 LANGUAGE internal IMMUTABLE STRICT AS 'matrix_add' WITH (OID=3214, DESCRIPTION="perform matrix addition on two conformable matrices");

 CREATE FUNCTION float8_matrix_accum(_float8, _float8) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'matrix_add' WITH (OID=3215, DESCRIPTION="perform matrix addition on two conformable matrices");

 CREATE FUNCTION sum(_int2) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3216, proisagg="t");

 CREATE FUNCTION sum(_int4) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3217, proisagg="t");

 CREATE FUNCTION sum(_int8) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3218, proisagg="t");

 CREATE FUNCTION sum(_float8) RETURNS _float8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3219, proisagg="t");

-- 3220 - reserved for sum(numeric[]) 
 CREATE FUNCTION int4_pivot_accum(_int4, _text, text, int4) RETURNS _int4 LANGUAGE internal IMMUTABLE AS 'int4_pivot_accum' WITH (OID=3225);

 CREATE FUNCTION pivot_sum(_text, text, int4) RETURNS _int4 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3226, proisagg="t");

 CREATE FUNCTION int8_pivot_accum(_int8, _text, text, int8) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'int8_pivot_accum' WITH (OID=3227);

 CREATE FUNCTION pivot_sum(_text, text, int8) RETURNS _int8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3228, proisagg="t");

 CREATE FUNCTION float8_pivot_accum(_float8, _text, text, float8) RETURNS _float8 LANGUAGE internal IMMUTABLE AS 'float8_pivot_accum' WITH (OID=3229);

 CREATE FUNCTION pivot_sum(_text, text, float8) RETURNS _float8 LANGUAGE internal IMMUTABLE AS 'aggregate_dummy' WITH (OID=3230, proisagg="t");

-- 3241-324? reserved for unpivot, see pivot.c 

 CREATE FUNCTION gpaotidin(cstring) RETURNS gpaotid LANGUAGE internal IMMUTABLE STRICT AS 'gpaotidin' WITH (OID=3302, DESCRIPTION="I/O");

 CREATE FUNCTION gpaotidout(gpaotid) RETURNS cstring LANGUAGE internal IMMUTABLE STRICT AS 'gpaotidout' WITH (OID=3303, DESCRIPTION="I/O");

 CREATE FUNCTION gpaotidrecv(internal) RETURNS gpaotid LANGUAGE internal IMMUTABLE STRICT AS 'gpaotidrecv' WITH (OID=3304, DESCRIPTION="I/O");

 CREATE FUNCTION gpaotidsend(gpaotid) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'gpaotidsend' WITH (OID=3305, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocin(cstring) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocin' WITH (OID=3312, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocout(gpxlogloc) RETURNS cstring LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocout' WITH (OID=3313, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocrecv(internal) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocrecv' WITH (OID=3314, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocsend(gpxlogloc) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocsend' WITH (OID=3315, DESCRIPTION="I/O");

 CREATE FUNCTION gpxlogloclarger(gpxlogloc, gpxlogloc) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE STRICT AS 'gpxlogloclarger' WITH (OID=3318, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocsmaller(gpxlogloc, gpxlogloc) RETURNS gpxlogloc LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocsmaller' WITH (OID=3319, DESCRIPTION="I/O");

 CREATE FUNCTION gpxlogloceq(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxlogloceq' WITH (OID=3331, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocne(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocne' WITH (OID=3320, DESCRIPTION="I/O");

 CREATE FUNCTION gpxlogloclt(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxlogloclt' WITH (OID=3321, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocle(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocle' WITH (OID=3322, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocgt(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocgt' WITH (OID=3323, DESCRIPTION="I/O");

 CREATE FUNCTION gpxloglocge(gpxlogloc, gpxlogloc) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'gpxloglocge' WITH (OID=3324, DESCRIPTION="I/O");

-- Greenplum MPP exposed internally-defined functions. 
 CREATE FUNCTION gp_backup_launch(text, text, text, text, text) RETURNS text LANGUAGE internal VOLATILE AS 'gp_backup_launch__' WITH (OID=6003, DESCRIPTION="launch mpp backup on outboard Postgres instances");

 CREATE FUNCTION gp_restore_launch(text, text, text, text, text, text, int4, bool) RETURNS text LANGUAGE internal VOLATILE AS 'gp_restore_launch__' WITH (OID=6004, DESCRIPTION="launch mpp restore on outboard Postgres instances");

 CREATE FUNCTION gp_read_backup_file(text, text, regproc) RETURNS text LANGUAGE internal VOLATILE AS 'gp_read_backup_file__' WITH (OID=6005, DESCRIPTION="read mpp backup file on outboard Postgres instances");

 CREATE FUNCTION gp_write_backup_file(text, text, text) RETURNS text LANGUAGE internal VOLATILE AS 'gp_write_backup_file__' WITH (OID=6006, DESCRIPTION="write mpp backup file on outboard Postgres instances");

 CREATE FUNCTION gp_pgdatabase() RETURNS SETOF record LANGUAGE internal VOLATILE AS 'gp_pgdatabase__' WITH (OID=6007, DESCRIPTION="view mpp pgdatabase state");

 CREATE FUNCTION numeric_amalg(_numeric, _numeric) RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'numeric_amalg' WITH (OID=6008, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION numeric_avg_amalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'numeric_avg_amalg' WITH (OID=4104, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION int8_avg_amalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int8_avg_amalg' WITH (OID=6009, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION float8_amalg(_float8, _float8) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'float8_amalg' WITH (OID=6010, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION float8_avg_amalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float8_avg_amalg' WITH (OID=4111, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION interval_amalg(_interval, _interval) RETURNS _interval LANGUAGE internal IMMUTABLE STRICT AS 'interval_amalg' WITH (OID=6011, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION numeric_demalg(_numeric, _numeric) RETURNS _numeric LANGUAGE internal IMMUTABLE STRICT AS 'numeric_demalg' WITH (OID=6015, DESCRIPTION="aggregate inverse preliminary function");

 CREATE FUNCTION numeric_avg_demalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'numeric_avg_demalg' WITH (OID=4105, DESCRIPTION="aggregate inverse preliminary function");

 CREATE FUNCTION int8_avg_demalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'int8_avg_demalg' WITH (OID=6016, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION float8_demalg(_float8, _float8) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'float8_demalg' WITH (OID=6017);

 CREATE FUNCTION float8_avg_demalg(bytea, bytea) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'float8_avg_demalg' WITH (OID=4110, DESCRIPTION="aggregate inverse preliminary function");

 CREATE FUNCTION interval_demalg(_interval, _interval) RETURNS _interval LANGUAGE internal IMMUTABLE STRICT AS 'interval_demalg' WITH (OID=6018, DESCRIPTION="aggregate preliminary function");

 CREATE FUNCTION float8_regr_amalg(_float8, _float8) RETURNS _float8 LANGUAGE internal IMMUTABLE STRICT AS 'float8_regr_amalg' WITH (OID=6014);

 CREATE FUNCTION int8(tid) RETURNS int8 LANGUAGE internal IMMUTABLE STRICT AS 'tidtoi8' WITH (OID=6021, DESCRIPTION="convert tid to int8");
-- #define CDB_PROC_TIDTOI8    6021

 CREATE FUNCTION gp_execution_segment() RETURNS SETOF int4 LANGUAGE internal VOLATILE AS 'mpp_execution_segment' WITH (OID=6022, DESCRIPTION="segment executing function");
-- #define MPP_EXECUTION_SEGMENT_OID 6022
-- #define MPP_EXECUTION_SEGMENT_TYPE 23

 CREATE FUNCTION pg_highest_oid() RETURNS oid LANGUAGE internal VOLATILE STRICT READS SQL DATA AS 'pg_highest_oid' WITH (OID=6023, DESCRIPTION="Highest oid used so far");

 CREATE FUNCTION gp_distributed_xacts() RETURNS SETOF record LANGUAGE internal VOLATILE AS 'gp_distributed_xacts__' WITH (OID=6035, DESCRIPTION="view mpp distributed transaction state");

 CREATE FUNCTION gp_distributed_xid() RETURNS xid LANGUAGE internal VOLATILE STRICT AS 'gp_distributed_xid' WITH (OID=6037, DESCRIPTION="Current distributed transaction id");

 CREATE FUNCTION gp_transaction_log() RETURNS SETOF record LANGUAGE internal VOLATILE AS 'gp_transaction_log' WITH (OID=6043, DESCRIPTION="view logged local transaction status");

 CREATE FUNCTION gp_distributed_log() RETURNS SETOF record LANGUAGE internal VOLATILE AS 'gp_distributed_log' WITH (OID=6044, DESCRIPTION="view logged distributed transaction status");

 CREATE FUNCTION gp_changetracking_log(IN filetype int4, OUT segment_id int2, OUT dbid int2, OUT space oid, OUT db oid, OUT rel oid, OUT xlogloc gpxlogloc, OUT blocknum int4, OUT persistent_tid tid, OUT persistent_sn int8) RETURNS SETOF pg_catalog.record LANGUAGE internal VOLATILE AS 'gp_changetracking_log' WITH (OID=6435, DESCRIPTION="view logged change tracking records");

 CREATE FUNCTION gp_execution_dbid() RETURNS int4 LANGUAGE internal VOLATILE AS 'gp_execution_dbid' WITH (OID=6068, DESCRIPTION="dbid executing function");

 CREATE FUNCTION get_ao_distribution(IN reloid oid, OUT segmentid int4, OUT tupcount int8) RETURNS SETOF pg_catalog.record LANGUAGE internal VOLATILE READS SQL DATA AS 'get_ao_distribution_oid' WITH (OID=7169, DESCRIPTION="show append only table tuple distribution across segment databases");

 CREATE FUNCTION get_ao_distribution(IN relname text, OUT segmentid int4, OUT tupcount int8) RETURNS SETOF pg_catalog.record LANGUAGE internal VOLATILE READS SQL DATA AS 'get_ao_distribution_name' WITH (OID=7170, DESCRIPTION="show append only table tuple distribution across segment databases");

 CREATE FUNCTION get_ao_compression_ratio(oid) RETURNS float8 LANGUAGE internal VOLATILE STRICT READS SQL DATA AS 'get_ao_compression_ratio_oid' WITH (OID=7171, DESCRIPTION="show append only table compression ratio");

 CREATE FUNCTION get_ao_compression_ratio(text) RETURNS float8 LANGUAGE internal VOLATILE STRICT READS SQL DATA AS 'get_ao_compression_ratio_name' WITH (OID=7172, DESCRIPTION="show append only table compression ratio");

 CREATE FUNCTION gp_update_ao_master_stats(oid) RETURNS int8 LANGUAGE internal VOLATILE MODIFIES SQL DATA AS 'gp_update_ao_master_stats_oid' WITH (OID=7173, DESCRIPTION="append only tables utility function");

 CREATE FUNCTION gp_update_ao_master_stats(text) RETURNS int8 LANGUAGE internal VOLATILE MODIFIES SQL DATA AS 'gp_update_ao_master_stats_name' WITH (OID=7174, DESCRIPTION="append only tables utility function");

 CREATE FUNCTION gp_persistent_build_db(bool) RETURNS int4 LANGUAGE internal VOLATILE AS 'gp_persistent_build_db' WITH (OID=7178, DESCRIPTION="populate the persistent tables and gp_relation_node for the current database");

 CREATE FUNCTION gp_persistent_build_all(bool) RETURNS int4 LANGUAGE internal VOLATILE AS 'gp_persistent_build_all' WITH (OID=7179, DESCRIPTION="populate the persistent tables and gp_relation_node for the whole database instance");

 CREATE FUNCTION gp_persistent_reset_all() RETURNS int4 LANGUAGE internal VOLATILE AS 'gp_persistent_reset_all' WITH (OID=7180, DESCRIPTION="Remove the persistent tables and gp_relation_node for the whole database instance");

 CREATE FUNCTION gp_persistent_repair_delete(int4, tid) RETURNS int4 LANGUAGE internal VOLATILE AS 'gp_persistent_repair_delete' WITH (OID=7181, DESCRIPTION="Remove an entry specified by TID from a persistent table for the current database instance");

 CREATE FUNCTION xmlexists(text, xml) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'xmlexists' WITH (OID=7182, DESCRIPTION="test XML value against XPath expression");

 CREATE FUNCTION xpath_exists(text, xml, _text) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'xpath_exists' WITH (OID=7183, DESCRIPTION="test XML value against XPath expression, with namespace support");

 CREATE FUNCTION xpath_exists(text, xml) RETURNS bool LANGUAGE sql IMMUTABLE STRICT AS $$select pg_catalog.xpath_exists($1, $2, '{}'::pg_catalog.text[])$$ WITH (OID=3049, DESCRIPTION="test XML value against XPath expression");

 CREATE FUNCTION xml_is_well_formed(text) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'xml_is_well_formed' WITH (OID=7184, DESCRIPTION="determine if a string is well formed XML");

 CREATE FUNCTION xml_is_well_formed_document(text) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'xml_is_well_formed_document' WITH (OID=7185, DESCRIPTION="determine if a string is well formed XML document");

 CREATE FUNCTION xml_is_well_formed_content(text) RETURNS bool LANGUAGE internal IMMUTABLE STRICT AS 'xml_is_well_formed_content' WITH (OID=7186, DESCRIPTION="determine if a string is well formed XML content");


-- the bitmap index access method routines
 CREATE FUNCTION bmgettuple(internal, internal) RETURNS bool LANGUAGE internal VOLATILE STRICT AS 'bmgettuple' WITH (OID=3050, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmgetbitmap(internal, internal) RETURNS internal LANGUAGE internal VOLATILE STRICT AS 'bmgetbitmap' WITH (OID=3051, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bminsert(internal, internal, internal, internal, internal, internal) RETURNS bool LANGUAGE internal VOLATILE STRICT AS 'bminsert' WITH (OID=7187, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmbeginscan(internal, internal, internal) RETURNS internal LANGUAGE internal VOLATILE STRICT AS 'bmbeginscan' WITH (OID=7188, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmrescan(internal, internal) RETURNS void LANGUAGE internal VOLATILE STRICT AS 'bmrescan' WITH (OID=7189, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmendscan(internal) RETURNS void LANGUAGE internal VOLATILE STRICT AS 'bmendscan' WITH (OID=7190, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmmarkpos(internal) RETURNS void LANGUAGE internal VOLATILE STRICT AS 'bmmarkpos' WITH (OID=7191, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmrestrpos(internal) RETURNS void LANGUAGE internal VOLATILE STRICT AS 'bmrestrpos' WITH (OID=7192, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmbuild(internal, internal, internal) RETURNS internal LANGUAGE internal VOLATILE STRICT AS 'bmbuild' WITH (OID=7193, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmbulkdelete(internal, internal, internal, internal) RETURNS internal LANGUAGE internal VOLATILE STRICT AS 'bmbulkdelete' WITH (OID=7194, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmvacuumcleanup(internal, internal) RETURNS internal LANGUAGE internal VOLATILE STRICT AS 'bmvacuumcleanup' WITH (OID=7195);

 CREATE FUNCTION bmcostestimate(internal, internal, internal, internal, internal, internal, internal, internal) RETURNS void LANGUAGE internal VOLATILE STRICT AS 'bmcostestimate' WITH (OID=7196, DESCRIPTION="bitmap(internal)");

 CREATE FUNCTION bmoptions(_text, bool) RETURNS bytea LANGUAGE internal STABLE STRICT AS 'bmoptions' WITH (OID=7197, DESCRIPTION="btree(internal)");

-- AOCS functions.

 CREATE FUNCTION aocsvpinfo_decode(bytea, int4, int4) RETURNS int8 LANGUAGE internal IMMUTABLE STRICT AS 'aocsvpinfo_decode' WITH (OID=9900);

-- raises deprecation error
 CREATE FUNCTION gp_deprecated() RETURNS void LANGUAGE internal IMMUTABLE AS 'gp_deprecated' WITH (OID=9997, DESCRIPTION="raises function deprecation error");

-- A convenient utility
 CREATE FUNCTION pg_objname_to_oid(text) RETURNS oid LANGUAGE internal IMMUTABLE STRICT AS 'pg_objname_to_oid' WITH (OID=9998, DESCRIPTION="convert an object name to oid");

-- Fault injection
 CREATE FUNCTION gp_fault_inject(int4, int8) RETURNS int8 LANGUAGE internal VOLATILE STRICT AS 'gp_fault_inject' WITH (OID=9999, DESCRIPTION="Greenplum fault testing only");

-- Analyze related
 CREATE FUNCTION gp_statistics_estimate_reltuples_relpages_oid(oid) RETURNS _float4 LANGUAGE internal VOLATILE STRICT AS 'gp_statistics_estimate_reltuples_relpages_oid' WITH (OID=5032, DESCRIPTION="Return reltuples/relpages information for relation.");

-- Backoff related
 CREATE FUNCTION gp_adjust_priority(int4, int4, int4) RETURNS int4 LANGUAGE internal VOLATILE STRICT AS 'gp_adjust_priority_int' WITH (OID=5040, DESCRIPTION="change weight of all the backends for a given session id");

 CREATE FUNCTION gp_adjust_priority(int4, int4, text) RETURNS int4 LANGUAGE internal VOLATILE STRICT AS 'gp_adjust_priority_value' WITH (OID=5041, DESCRIPTION="change weight of all the backends for a given session id");

 CREATE FUNCTION gp_list_backend_priorities() RETURNS SETOF record LANGUAGE internal VOLATILE AS 'gp_list_backend_priorities' WITH (OID=5042, DESCRIPTION="list priorities of backends");

-- Functions to deal with SREH error logs
 CREATE FUNCTION gp_read_error_log(exttable text, OUT cmdtime timestamptz, OUT relname text, OUT filename text, OUT linenum int4, OUT bytenum int4, OUT errmsg text, OUT rawdata text, OUT rawbytes bytea) RETURNS SETOF record LANGUAGE INTERNAL STRICT VOLATILE EXECUTE ON ALL SEGMENTS AS 'gp_read_error_log' WITH (OID = 7076, DESCRIPTION="read the error log for the specified external table");

 CREATE FUNCTION gp_truncate_error_log(text) RETURNS bool LANGUAGE INTERNAL STRICT VOLATILE AS 'gp_truncate_error_log' WITH (OID=3069, DESCRIPTION="truncate the error log for the specified external table");

-- elog related
 CREATE FUNCTION gp_elog(text) RETURNS void LANGUAGE internal IMMUTABLE STRICT AS 'gp_elog' WITH (OID=5044, DESCRIPTION="Insert text into the error log");

 CREATE FUNCTION gp_elog(text, bool) RETURNS void LANGUAGE internal IMMUTABLE STRICT AS 'gp_elog' WITH (OID=5045, DESCRIPTION="Insert text into the error log");

-- Segment and master administration functions, see utils/gp/segadmin.c
 CREATE FUNCTION gp_add_master_standby(text, text, _text) RETURNS int2 LANGUAGE internal VOLATILE AS 'gp_add_master_standby' WITH (OID=5046, DESCRIPTION="Perform the catalog operations necessary for adding a new standby");

 CREATE FUNCTION gp_add_master_standby(text, text, _text, int4) RETURNS int2 LANGUAGE internal VOLATILE AS 'gp_add_master_standby_port' WITH (OID=5038, DESCRIPTION="Perform the catalog operations necessary for adding a new standby");

 CREATE FUNCTION gp_remove_master_standby() RETURNS bool LANGUAGE internal VOLATILE AS 'gp_remove_master_standby' WITH (OID=5047, DESCRIPTION="Remove a master standby from the system catalog");

 CREATE FUNCTION gp_add_segment_primary(text, text, int4, _text) RETURNS int2 LANGUAGE internal VOLATILE AS 'gp_add_segment_primary' WITH (OID=5039, DESCRIPTION="Perform the catalog operations necessary for adding a new primary segment");

 CREATE FUNCTION gp_add_segment_mirror(int2, text, text, int4, int4, _text) RETURNS int2 LANGUAGE internal VOLATILE AS 'gp_add_segment_mirror' WITH (OID=5048, DESCRIPTION="Perform the catalog operations necessary for adding a new segment mirror");

 CREATE FUNCTION gp_remove_segment_mirror(int2) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_remove_segment_mirror' WITH (OID=5049, DESCRIPTION="Remove a segment mirror from the system catalog");

 CREATE FUNCTION gp_add_segment(int2, int2, "char", "char", "char", "char", int4, text, text, int4, _text) RETURNS int2 LANGUAGE internal VOLATILE AS 'gp_add_segment' WITH (OID=5050, DESCRIPTION="Perform the catalog operations necessary for adding a new segment");

 CREATE FUNCTION gp_remove_segment(int2) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_remove_segment' WITH (OID=5051, DESCRIPTION="Remove a primary segment from the system catalog");

 CREATE FUNCTION gp_prep_new_segment(_text) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_prep_new_segment' WITH (OID=5052, DESCRIPTION="Convert a cloned master catalog for use as a segment");

 CREATE FUNCTION gp_activate_standby() RETURNS bool LANGUAGE internal VOLATILE AS 'gp_activate_standby' WITH (OID=5053, DESCRIPTION="Activate a standby");

 CREATE FUNCTION gp_request_fts_probe_scan() RETURNS bool LANGUAGE internal VOLATILE AS 'gp_request_fts_probe_scan' EXECUTE ON MASTER WITH (OID=5035, DESCRIPTION="Request a FTS probe scan and wait for response");

-- We cheat in the following two functions: they are technically volatile but
-- we can only dispatch them if they're immutable :(.


 CREATE FUNCTION gp_add_segment_persistent_entries(int2, int2, _text) RETURNS bool LANGUAGE internal IMMUTABLE AS 'gp_add_segment_persistent_entries' WITH (OID=5054, DESCRIPTION="Persist object nodes on a segment");

 CREATE FUNCTION gp_remove_segment_persistent_entries(int2, int2) RETURNS bool LANGUAGE internal IMMUTABLE AS 'gp_remove_segment_persistent_entries' WITH (OID=5055, DESCRIPTION="Remove persistent object node references at a segment");

-- persistent table repair functions

 CREATE FUNCTION gp_add_persistent_filespace_node_entry(tid, oid, int2, text, int2, text, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_persistent_filespace_node_entry' WITH (OID=5056, DESCRIPTION="Add a new entry to gp_persistent_filespace_node");

 CREATE FUNCTION gp_add_persistent_tablespace_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_persistent_tablespace_node_entry' WITH (OID=5057, DESCRIPTION="Add a new entry to gp_persistent_tablespace_node");

 CREATE FUNCTION gp_add_persistent_database_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_persistent_database_node_entry' WITH (OID=5058, DESCRIPTION="Add a new entry to gp_persistent_database_node");

 CREATE FUNCTION gp_add_persistent_relation_node_entry(tid, oid, oid, oid, int4, int2, int2, int8, int2, int2, bool, int8, gpxlogloc, int4, int8, int8, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_persistent_relation_node_entry' WITH (OID=5059, DESCRIPTION="Add a new entry to gp_persistent_relation_node");

 CREATE FUNCTION gp_add_global_sequence_entry(tid, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_global_sequence_entry' WITH (OID=5060, DESCRIPTION="Add a new entry to gp_global_sequence");

 CREATE FUNCTION gp_add_relation_node_entry(tid, oid, oid, int4, int8, tid, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_add_relation_node_entry' WITH (OID=5061, DESCRIPTION="Add a new entry to gp_relation_node");

 CREATE FUNCTION gp_update_persistent_filespace_node_entry(tid, oid, int2, text, int2, text, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_persistent_filespace_node_entry' WITH (OID=5062, DESCRIPTION="Update an entry in gp_persistent_filespace_node");

 CREATE FUNCTION gp_update_persistent_tablespace_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_persistent_tablespace_node_entry' WITH (OID=5063, DESCRIPTION="Update an entry in gp_persistent_tablespace_node");

 CREATE FUNCTION gp_update_persistent_database_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_persistent_database_node_entry' WITH (OID=5064, DESCRIPTION="Update an entry in gp_persistent_database_node");

 CREATE FUNCTION gp_update_persistent_relation_node_entry(tid, oid, oid, oid, int4, int2, int2, int8, int2, int2, bool, int8, gpxlogloc, int4, int8, int8, int4, int4, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_persistent_relation_node_entry' WITH (OID=5065, DESCRIPTION="Update an entry in gp_persistent_relation_node");

 CREATE FUNCTION gp_update_global_sequence_entry(tid, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_global_sequence_entry' WITH (OID=5066, DESCRIPTION="Update an entry in gp_global_sequence");

 CREATE FUNCTION gp_update_relation_node_entry(tid, oid, oid, int4, int8, tid, int8) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_update_relation_node_entry' WITH (OID=5067, DESCRIPTION="Update an entry in gp_relation_node");

 CREATE FUNCTION gp_delete_persistent_filespace_node_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_persistent_filespace_node_entry' WITH (OID=5068, DESCRIPTION="Remove an entry from gp_persistent_filespace_node");

 CREATE FUNCTION gp_delete_persistent_tablespace_node_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_persistent_tablespace_node_entry' WITH (OID=5069, DESCRIPTION="Remove an entry from gp_persistent_tablespace_node");

 CREATE FUNCTION gp_delete_persistent_database_node_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_persistent_database_node_entry' WITH (OID=5070, DESCRIPTION="Remove an entry from gp_persistent_database_node");

 CREATE FUNCTION gp_delete_persistent_relation_node_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_persistent_relation_node_entry' WITH (OID=5071, DESCRIPTION="Remove an entry from gp_persistent_relation_node");

 CREATE FUNCTION gp_delete_global_sequence_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_global_sequence_entry' WITH (OID=5072, DESCRIPTION="Remove an entry from gp_global_sequence");

 CREATE FUNCTION gp_delete_relation_node_entry(tid) RETURNS bool LANGUAGE internal VOLATILE AS 'gp_delete_relation_node_entry' WITH (OID=5073, DESCRIPTION="Remove an entry from gp_relation_node");

 CREATE FUNCTION gp_persistent_relation_node_check() RETURNS SETOF gp_persistent_relation_node LANGUAGE internal VOLATILE AS 'gp_persistent_relation_node_check' WITH (OID=5074, DESCRIPTION="physical filesystem information");

CREATE FUNCTION gp_dbspecific_ptcat_verification() RETURNS bool LANGUAGE internal VOLATILE AS 'gp_dbspecific_ptcat_verification' WITH (OID=5075, DESCRIPTION="perform database specific PersistentTables-Catalog verification");

CREATE FUNCTION gp_nondbspecific_ptcat_verification() RETURNS bool LANGUAGE internal VOLATILE AS 'gp_nondbspecific_ptcat_verification' WITH (OID=5080, DESCRIPTION="perform non-database specific PersistentTables-Catalog verification");

 CREATE FUNCTION cosh(float8) RETURNS float8 LANGUAGE internal IMMUTABLE AS 'dcosh' WITH (OID=3539, DESCRIPTION="Hyperbolic cosine function");

 CREATE FUNCTION sinh(float8) RETURNS float8 LANGUAGE internal IMMUTABLE AS 'dsinh' WITH (OID=3540, DESCRIPTION="Hyperbolic sine function");

 CREATE FUNCTION tanh(float8) RETURNS float8 LANGUAGE internal IMMUTABLE AS 'dtanh' WITH (OID=3541, DESCRIPTION="Hyperbolic tangent function");

 CREATE FUNCTION anytable_in(cstring) RETURNS anytable LANGUAGE internal IMMUTABLE STRICT AS 'anytable_in' WITH (OID=3054, DESCRIPTION="anytable type serialization input function");

 CREATE FUNCTION anytable_out(anytable) RETURNS cstring LANGUAGE internal IMMUTABLE STRICT AS 'anytable_out' WITH (OID=3055, DESCRIPTION="anytable type serialization output function");

 CREATE FUNCTION gp_zlib_constructor(internal, internal, bool) RETURNS internal LANGUAGE internal VOLATILE AS 'zlib_constructor' WITH (OID=9910, DESCRIPTION="zlib constructor");

 CREATE FUNCTION gp_zlib_destructor(internal) RETURNS void LANGUAGE internal VOLATILE AS 'zlib_destructor' WITH(OID=9911, DESCRIPTION="zlib destructor");

 CREATE FUNCTION gp_zlib_compress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zlib_compress' WITH(OID=9912, DESCRIPTION="zlib compressor");

 CREATE FUNCTION gp_zlib_decompress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zlib_decompress' WITH(OID=9913, DESCRIPTION="zlib decompressor");

 CREATE FUNCTION gp_zlib_validator(internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zlib_validator' WITH(OID=9924, DESCRIPTION="zlib compression validator");

 CREATE FUNCTION gp_rle_type_constructor(internal, internal, bool) RETURNS internal LANGUAGE internal VOLATILE AS 'rle_type_constructor' WITH (OID=9914, DESCRIPTION="Type specific RLE constructor");

 CREATE FUNCTION gp_rle_type_destructor(internal) RETURNS void LANGUAGE internal VOLATILE AS 'rle_type_destructor' WITH(OID=9915, DESCRIPTION="Type specific RLE destructor");

 CREATE FUNCTION gp_rle_type_compress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'rle_type_compress' WITH(OID=9916, DESCRIPTION="Type specific RLE compressor");

 CREATE FUNCTION gp_rle_type_decompress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'rle_type_decompress' WITH(OID=9917, DESCRIPTION="Type specific RLE decompressor");

 CREATE FUNCTION gp_rle_type_validator(internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'rle_type_validator' WITH(OID=9923, DESCRIPTION="Type speific RLE compression validator");

 CREATE FUNCTION gp_zstd_constructor(internal, internal, bool) RETURNS internal LANGUAGE internal VOLATILE AS 'zstd_constructor' WITH (OID=3071, DESCRIPTION="zstd compressor and decompressor constructor");

 CREATE FUNCTION gp_zstd_destructor(internal) RETURNS void LANGUAGE internal VOLATILE AS 'zstd_destructor' WITH(OID=3072, DESCRIPTION="zstd compressor and decompressor destructor");

 CREATE FUNCTION gp_zstd_compress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zstd_compress' WITH(OID=3073, DESCRIPTION="zstd compressor");

 CREATE FUNCTION gp_zstd_decompress(internal, int4, internal, int4, internal, internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zstd_decompress' WITH(OID=3074, DESCRIPTION="zstd decompressor");

 CREATE FUNCTION gp_zstd_validator(internal) RETURNS void LANGUAGE internal IMMUTABLE AS 'zstd_validator' WITH(OID=3075, DESCRIPTION="zstdcompression validator");

 CREATE FUNCTION gp_dummy_compression_constructor(internal, internal, bool) RETURNS internal LANGUAGE internal VOLATILE AS 'dummy_compression_constructor' WITH (OID=3064, DESCRIPTION="Dummy compression destructor");

 CREATE FUNCTION gp_dummy_compression_destructor(internal) RETURNS internal LANGUAGE internal VOLATILE AS 'dummy_compression_destructor' WITH (OID=3065, DESCRIPTION="Dummy compression destructor");

 CREATE FUNCTION gp_dummy_compression_compress(internal, int4, internal, int4, internal, internal) RETURNS internal LANGUAGE internal VOLATILE AS 'dummy_compression_compress' WITH (OID=3066, DESCRIPTION="Dummy compression compressor");

 CREATE FUNCTION gp_dummy_compression_decompress(internal, int4, internal, int4, internal, internal) RETURNS internal LANGUAGE internal VOLATILE AS 'dummy_compression_decompress' WITH (OID=3067, DESCRIPTION="Dummy compression decompressor");

 CREATE FUNCTION gp_dummy_compression_validator(internal) RETURNS internal LANGUAGE internal VOLATILE AS 'dummy_compression_validator' WITH (OID=3068, DESCRIPTION="Dummy compression validator");

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, int8, anyelement, int8 ) RETURNS int8 LANGUAGE internal IMMUTABLE STRICT AS 'linterp_int64' WITH (OID=6072, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, int4, anyelement, int4 ) RETURNS int4 LANGUAGE internal IMMUTABLE STRICT AS 'linterp_int32' WITH (OID=6073, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, int2, anyelement, int2 ) RETURNS int2 LANGUAGE internal IMMUTABLE STRICT AS 'linterp_int16' WITH (OID=6074, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, float8, anyelement, float8 ) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'linterp_float8' WITH (OID=6075, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, float4, anyelement, float4 ) RETURNS float4 LANGUAGE internal IMMUTABLE STRICT AS 'linterp_float4' WITH (OID=6076, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, date, anyelement, date ) RETURNS date LANGUAGE internal IMMUTABLE STRICT AS 'linterp_DateADT' WITH (OID=6077, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, time, anyelement, time ) RETURNS time LANGUAGE internal IMMUTABLE STRICT AS 'linterp_TimeADT' WITH (OID=6078, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, timestamp, anyelement, timestamp ) RETURNS timestamp LANGUAGE internal IMMUTABLE STRICT AS 'linterp_Timestamp' WITH (OID=6079, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, timestamptz, anyelement, timestamptz ) RETURNS timestamptz LANGUAGE internal IMMUTABLE STRICT AS 'linterp_TimestampTz' WITH (OID=6080, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, "interval", anyelement, "interval" ) RETURNS "interval" LANGUAGE internal IMMUTABLE STRICT AS 'linterp_Interval' WITH (OID=6081, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION linear_interpolate( anyelement, anyelement, "numeric", anyelement, "numeric" ) RETURNS "numeric" LANGUAGE internal IMMUTABLE STRICT AS 'linterp_Numeric' WITH (OID=6082, DESCRIPTION="linear interpolation: x, x0,y0, x1,y1"); 

 CREATE FUNCTION gp_dump_query_oids(text) RETURNS text LANGUAGE internal VOLATILE STRICT AS 'gp_dump_query_oids' WITH (OID = 6086, DESCRIPTION="List function and relation OIDs that a query depends on, as a JSON object");

 CREATE FUNCTION disable_xform(text) RETURNS text LANGUAGE internal IMMUTABLE STRICT AS 'disable_xform' WITH (OID=6087, DESCRIPTION="disables transformations in the optimizer");

 CREATE FUNCTION enable_xform(text) RETURNS text LANGUAGE internal IMMUTABLE STRICT AS 'enable_xform' WITH (OID=6088, DESCRIPTION="enables transformations in the optimizer");

 CREATE FUNCTION gp_opt_version() RETURNS text LANGUAGE internal IMMUTABLE STRICT AS 'gp_opt_version' WITH (OID=6089, DESCRIPTION="Returns the optimizer and gpos library versions");
 
 
  -- functions for the complex data type
 CREATE FUNCTION complex_in(cstring) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_in' WITH (OID=6460, DESCRIPTION="I/O");
 
 CREATE FUNCTION complex_out(complex) RETURNS cstring LANGUAGE internal IMMUTABLE STRICT AS 'complex_out' WITH (OID=3548, DESCRIPTION="I/O");
 
 CREATE FUNCTION complex_recv(internal) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_recv' WITH (OID=3549, DESCRIPTION="I/O");
 
 CREATE FUNCTION complex_send(complex) RETURNS bytea LANGUAGE internal IMMUTABLE STRICT AS 'complex_send' WITH (OID=3550, DESCRIPTION="I/O");

 CREATE FUNCTION complex(float8, float8) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'construct_complex' WITH (OID=3551, DESCRIPTION="constructs a complex number with given real part and imaginary part");
 
 CREATE FUNCTION complex_trig(float8, float8) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'construct_complex_trig' WITH (OID=3552, DESCRIPTION="constructs a complex number with given magnitude and phase");
 
 CREATE FUNCTION re(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_re' WITH (OID=3553, DESCRIPTION="returns the real part of the argument");
 
 CREATE FUNCTION im(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_im' WITH (OID=3554, DESCRIPTION="returns the imaginary part of the argument");
 
 CREATE FUNCTION radians(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_arg' WITH (OID=3555, DESCRIPTION="returns the phase of the argument");
 
 CREATE FUNCTION complexabs(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_mag' WITH (OID=3556, DESCRIPTION="returns the magnitude of the argument");
  
 CREATE FUNCTION abs(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_mag' WITH (OID=3557, DESCRIPTION="returns the magnitude of the argument");
 
 CREATE FUNCTION conj(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_conj' WITH (OID=3558, DESCRIPTION="returns the conjunction of the argument");
 
 CREATE FUNCTION hashcomplex(complex) RETURNS int4 LANGUAGE internal IMMUTABLE STRICT AS 'complex_hash' WITH (OID=3559, DESCRIPTION="hash");
 
 CREATE FUNCTION complex_eq(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_eq' WITH (OID=3560, DESCRIPTION="equal");
 
 CREATE FUNCTION complex_ne(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_ne' WITH (OID=3561, DESCRIPTION="not equal");
 
 CREATE FUNCTION complex_pl(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_pl' WITH (OID=3562, DESCRIPTION="plus");
 
 CREATE FUNCTION complex_up(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_up' WITH (OID=3563, DESCRIPTION="unary plus");
 
 CREATE FUNCTION complex_mi(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_mi' WITH (OID=3564, DESCRIPTION="minus");
 
 CREATE FUNCTION complex_um(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_um' WITH (OID=3565, DESCRIPTION="unary minus");
 
 CREATE FUNCTION complex_mul(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_mul' WITH (OID=3566, DESCRIPTION="multiply");
 
 CREATE FUNCTION complex_div(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_div' WITH (OID=3567, DESCRIPTION="divide");
 
 CREATE FUNCTION complex_power(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_pow' WITH (OID=3568, DESCRIPTION="exponentiation (x^y)");
 
 CREATE FUNCTION complex_sqrt(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_sqrt' WITH (OID=3569, DESCRIPTION="squre root");
 
 CREATE FUNCTION complex_cbrt(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_cbrt' WITH (OID=3570, DESCRIPTION="cube root");
 
 CREATE FUNCTION degrees(complex) RETURNS float8 LANGUAGE internal IMMUTABLE STRICT AS 'complex_degrees' WITH (OID=3571, DESCRIPTION="phase to degrees");
 
 CREATE FUNCTION exp(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_exp' WITH (OID=3572, DESCRIPTION="natural exponential (e^x)");
 
 CREATE FUNCTION ln(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_ln' WITH (OID=3573, DESCRIPTION="natural logarithm");
 
 CREATE FUNCTION log(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_log10' WITH (OID=3574, DESCRIPTION="base 10 logarithm");
 
 CREATE FUNCTION log(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_log' WITH (OID=3575, DESCRIPTION="logarithm base arg1 of arg2");
 
 CREATE FUNCTION acos(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_acos' WITH (OID=3576, DESCRIPTION="acos");
 
 CREATE FUNCTION asin(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_asin' WITH (OID=3577, DESCRIPTION="asin");
 
 CREATE FUNCTION atan(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_atan' WITH (OID=3578, DESCRIPTION="atan");
 
 CREATE FUNCTION cos(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_cos' WITH (OID=3579, DESCRIPTION="cos");
 
 CREATE FUNCTION cot(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_cot' WITH (OID=3580, DESCRIPTION="cot");
 
 CREATE FUNCTION sin(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_sin' WITH (OID=3581, DESCRIPTION="sin");
 
 CREATE FUNCTION tan(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_tan' WITH (OID=3582, DESCRIPTION="tan");
 
 CREATE FUNCTION dotproduct(_complex, _complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_dot_product' WITH (OID=3583, DESCRIPTION="dot product");
 
 CREATE FUNCTION float82complex(float8) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'float82complex' WITH (OID=3584, DESCRIPTION="(internal) type cast from float8 to complex");
 
 CREATE FUNCTION float42complex(float4) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'float42complex' WITH (OID=3585, DESCRIPTION="(internal) type cast from float4 to complex");
 
 CREATE FUNCTION int82complex(int8) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'int82complex' WITH (OID=3586, DESCRIPTION="(internal) type cast from int8 to complex");
 
 CREATE FUNCTION int42complex(int4) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'int42complex' WITH (OID=3587, DESCRIPTION="(internal) type cast from int4 to complex");
 
 CREATE FUNCTION int22complex(int2) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'int22complex' WITH (OID=3588, DESCRIPTION="(internal) type cast from int2 to complex");
 
 CREATE FUNCTION power(complex, complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_pow' WITH (OID=3589, DESCRIPTION="exponentiation (x^y)");
 
 CREATE FUNCTION sqrt(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_sqrt' WITH (OID=3590, DESCRIPTION="squre root");
 
 CREATE FUNCTION cbrt(complex) RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'complex_cbrt' WITH (OID=3591, DESCRIPTION="cube root"); 
 
 CREATE FUNCTION numeric2point("numeric") RETURNS complex LANGUAGE internal IMMUTABLE STRICT AS 'numeric2complex' WITH (OID=3592, DESCRIPTION="(internal) type cast from numeric to complex");
 
 CREATE FUNCTION complex_lt(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_lt' WITH (OID=3593, DESCRIPTION="less than");
 
 CREATE FUNCTION complex_gt(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_gt' WITH (OID=3594, DESCRIPTION="greater than");
 
 CREATE FUNCTION complex_lte(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_lte' WITH (OID=3595, DESCRIPTION="less than or equal");
 
 CREATE FUNCTION complex_gte(complex, complex) RETURNS bool  LANGUAGE internal IMMUTABLE STRICT AS 'complex_gte' WITH (OID=3596, DESCRIPTION="greater than or equal");
