

drop table if exists fooaot;
create table fooaot(a int, b int) with (appendonly=true);

drop table if exists fooheap;
create table fooheap(a int, b int);

--

select * from gp_toolkit.__gp_is_append_only
where iaooid = 'fooaot'::regclass;

select * from gp_toolkit.__gp_is_append_only
where iaooid = 'fooheap'::regclass;

select * from gp_toolkit.__gp_fullname
where fnoid = 'fooaot'::regclass;

select * from gp_toolkit.__gp_user_namespaces;

select count(*) from gp_toolkit.__gp_user_tables;
select count(*) from gp_toolkit.__gp_user_data_tables;

select count(distinct localid) as distlocid FROM gp_toolkit.__gp_localid;
select * FROM gp_toolkit.__gp_masterid;


select * from gp_toolkit.__gp_number_of_segments;

select count(*) from gp_toolkit.__gp_log_segment_ext;
select count(*) from gp_toolkit.__gp_log_master_ext;
select count(*) from gp_toolkit.gp_log_system;
select count(*) from gp_toolkit.gp_log_database;
select count(*) from gp_toolkit.gp_log_master_concise;
select count(*) from gp_toolkit.gp_log_command_timings;

select * FROM gp_toolkit.__gp_param_local_setting('max_connections');
select * FROM gp_toolkit.gp_param_setting('max_connections');
select * from gp_toolkit.gp_param_settings() limit 10;
select * from gp_toolkit.gp_param_settings_seg_value_diffs;

select * FROM gp_toolkit.gp_skew_details('fooaot'::regclass);
select * FROM gp_toolkit.gp_skew_details('fooheap'::regclass);

select * from gp_toolkit.gp_skew_coefficient('fooheap'::regclass);
select * from gp_toolkit.gp_skew_coefficient('fooaot'::regclass);

select * from gp_toolkit.__gp_skew_coefficients() order by 1 limit 10 ;
select * from gp_toolkit.gp_skew_coefficients order by 1 limit 10 ;

select * from gp_toolkit.gp_skew_idle_fraction('fooaot'::regclass);
select * from gp_toolkit.gp_skew_idle_fraction('fooheap'::regclass);

select * from gp_toolkit.__gp_skew_idle_fractions() order by 1 limit 10 ;
select * from gp_toolkit.gp_skew_idle_fractions order by 1 limit 10 ;

--create skew
insert into fooaot select 1, generate_series(1,1000000);
select * from gp_toolkit.gp_skew_idle_fraction('fooaot'::regclass);
select * from gp_toolkit.gp_skew_coefficient('fooaot'::regclass);


select * from gp_toolkit.gp_stats_missing order by 1 limit 10;

select * from gp_toolkit.gp_disk_free;

insert into fooheap select 1, generate_series(1,1000000);
delete from fooheap;
analyze fooheap;

select * from gp_toolkit.gp_bloat_diag;


select * from gp_toolkit.gp_resq_activity_by_queue;
select * from gp_toolkit.gp_resq_priority_statement;
select * from gp_toolkit.gp_resq_priority_backend;


select * from gp_toolkit.gp_pgdatabase_invalid;

select * from gp_toolkit.gp_resq_role;

select * from gp_toolkit.gp_locks_on_resqueue;
select * from gp_toolkit.gp_locks_on_relation;

select * from gp_toolkit.gp_roles_assigned;

select * from gp_toolkit.gp_size_of_index;
select * from gp_toolkit.gp_size_of_table_disk;
select * from gp_toolkit.gp_size_of_table_uncompressed;
select * from gp_toolkit.gp_table_indexes limit 10;
select * from gp_toolkit.gp_size_of_all_table_indexes;
select * from gp_toolkit.gp_size_of_table_and_indexes_disk;
select * from gp_toolkit.gp_size_of_partition_and_indexes_disk;
select * from gp_toolkit.gp_size_of_schema_disk;
select * from gp_toolkit.gp_size_of_database;

select * from gp_toolkit.gp_resgroup_role;


