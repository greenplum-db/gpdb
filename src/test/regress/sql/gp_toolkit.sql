-- Tests for the functions and views in gp_toolkit

-- Create an empty database to test in, because some of the gp_toolkit views
-- are really slow, when there are a lot of objects in the database.
create database toolkit_testdb;
\c toolkit_testdb

create role toolkit_admin superuser createdb;
create role toolkit_user1 login;

CREATE TABLE toolkit_ao (id INTEGER) WITH (appendonly = true);
CREATE TABLE toolkit_heap (id INTEGER) WITH (appendonly = false);

CREATE TABLE toolkit_aopart (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )
partition by range (n_nationkey)
subpartition by range (n_regionkey) subpartition template (start('0') end('1') inclusive,start('1') exclusive
)
(
partition p1 start('0') end('10') WITH (appendonly=true,checksum=true,compresslevel=9), partition p2 start('10') end('25') WITH (checksum=false,appendonly=true,compresslevel=7)
);

select count(iaotype),iaotype
from gp_toolkit.__gp_is_append_only iao
inner join pg_catalog.pg_class c on iao.iaooid = c.oid
where c.relname like 'toolkit_%'
group by iaotype;

drop table toolkit_aopart;

select count(iaotype),iaotype
from gp_toolkit.__gp_is_append_only iao
inner join pg_catalog.pg_class c on iao.iaooid = c.oid
where c.relname like 'toolkit_%'
group by iaotype;
select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='tktest';

create schema tktest;

select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='tktest';

drop schema tktest;

select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='tktest';
select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;

create table toolkit_a (a int);
create table toolkit_b (b int);

select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;

drop table toolkit_a;
drop table toolkit_b;

select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;

-- Test log reading functions

select logseverity from gp_toolkit.__gp_log_segment_ext where logseverity='LOG' limit 5;
select logseverity from gp_toolkit.__gp_log_master_ext where logseverity='LOG' limit 5;
select logseverity from gp_toolkit.gp_log_system where logseverity='LOG' limit 5;
select logseverity from gp_toolkit.gp_log_database where logseverity='LOG' limit 3;
select logseverity from gp_toolkit.gp_log_master_concise where logseverity='LOG' limit 5; -- superuser only

-- GP Command Timings
--
-- The output of this is so unpredictable that it's hard to create a useful test case.
select count(*) from
(
  select logsession, logcmdcount, logpid, logduration from gp_toolkit.gp_log_command_timings
  limit 5
) as timings_test;

-- GP Disk Free
-- Shows only the first segment
-- Ignore dfhostname column
select dfsegment,
       dfspace > 10 as "dfspace > 10 kB",
       dfspace < 1000000000000 as "dfspace < 1 PB"
from gp_toolkit.gp_disk_free where dfsegment=0;


-- GP Missing Stats
-- New table or without any data will not have any stats
-- After analyze or with auto-stats, then we will not have any table in gp_stats_missing
set gp_autostats_mode='none';
create table toolkit_miss_stat (a int, b int);
select * from gp_toolkit.gp_stats_missing where smitable='toolkit_miss_stat';
insert into toolkit_miss_stat select i,i from generate_series(1,10) i;
analyze toolkit_miss_stat;
select * from gp_toolkit.gp_stats_missing where smitable='toolkit_miss_stat';
drop table toolkit_miss_stat;


-- Test the gp_skew_idle_fractions view
create table toolkit_skew (a int);
insert into toolkit_skew select i from generate_series(1,50000) i;
select sifnamespace, sifrelname from gp_toolkit.gp_skew_idle_fractions where sifoid = 'toolkit_skew'::regclass;

-----------------------------------
-- Test gp_bloat_expected_pages and gp_bloat_diag views
-- (re-using the toolkit_skew table)
analyze toolkit_skew;
select btdrelpages > 40 as btdrelpages_over_40,
       btdrelpages < 80 as btdrelpages_below_80,
       btdexppages > 40 as btdexppages_over_40,
       btdexppages < 80 as btdexppages_below_80
from gp_toolkit.gp_bloat_expected_pages where btdrelid = 'toolkit_skew'::regclass;
select * from gp_toolkit.gp_bloat_diag where bdirelid = 'toolkit_skew'::regclass;

-- Create bloat by deleting all rows
delete from toolkit_skew;
analyze toolkit_skew;
select btdrelpages > 40 as btdrelpages_over_40,
       btdrelpages < 80 as btdrelpages_below_80,
       btdexppages > 0 as btdexppages_over_0,
       btdexppages < 10 as btdexppages_below_10
from gp_toolkit.gp_bloat_expected_pages where btdrelid = 'toolkit_skew'::regclass;

-- gp_bloat_diag view should complain that there is a lot of bloat.
select bdirelid::regclass, bdinspname, bdirelname,
       bdirelpages > 40 as bdirelpages_over_40,
       bdirelpages < 80 as bdirelpages_below_80,
       bdiexppages > 40 as bdiexppages_over_40,
       bdiexppages < 80 as bdiexppages_below_80,
       bdidiag
from gp_toolkit.gp_bloat_diag where bdirelid = 'toolkit_skew'::regclass;

-- After vacuum, bloat should be gone
vacuum toolkit_skew;
select btdrelpages > 0 as btdrelpages_over_0,
       btdrelpages < 10 as btdrelpages_below_10,
       btdexppages > 0 as btdexppages_over_0,
       btdexppages < 10 as btdexppages_below_10
from gp_toolkit.gp_bloat_expected_pages where btdrelid = 'toolkit_skew'::regclass;
select * from gp_toolkit.gp_bloat_diag where bdirelid = 'toolkit_skew'::regclass;


-- Check that gp_bloat_diag can deal with big numbers. (This used to provoke an
-- integer overflow error, before the view was fixed to use numerics for all the
-- calculations.)
create table wide_width_test as select * from pg_attribute;
set allow_system_table_mods=dml ;
update pg_statistic set stawidth=2034567890 where starelid = (select oid from pg_class where relname='test');

select * from gp_toolkit.gp_bloat_diag WHERE bdinspname <> 'pg_catalog';

-- MPP-5871 : ERROR: GetSnapshotData timed out waiting for Writer to set the shared snapshot.
set client_min_messages='warning';
CREATE TABLE mpp5871_statistics (
   id bigint NOT NULL,
   idaffiliate integer NOT NULL,
   idsite integer NOT NULL,
   idoffer integer NOT NULL,
   idcreative integer NOT NULL,
   idoptimizer integer,
   date date NOT NULL,
   subid character varying,
   impressions integer NOT NULL,
   clicks integer NOT NULL,
   conversions integer NOT NULL,
   salevalue numeric(10,2) NOT NULL,
   revenue numeric(10,2),
   cost numeric(10,2)
) DISTRIBUTED BY (idaffiliate,date)
PARTITION BY RANGE (DATE)
(
PARTITION monthly START (date '2007-10-01') INCLUSIVE
END (date '2010-01-01') EXCLUSIVE
EVERY (INTERVAL '1 MONTH'),
PARTITION old_data END (date '2007-10-01') EXCLUSIVE
);
set client_min_messages='notice';

SELECT sifoid::regclass, siffraction from gp_toolkit.gp_skew_idle_fraction('mpp5871_statistics'::regclass);


create schema tktest;
set search_path=tktest;

create table gptoolkit_user_table_heap (a int);
create table gptoolkit_user_table_ao (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )
partition by range (n_nationkey)
subpartition by range (n_regionkey) subpartition template (start('0') end('1') inclusive,start('1') exclusive
)
(
partition p1 start('0') end('10') WITH (appendonly=true,checksum=true,compresslevel=9), partition p2 start('10') end('25') WITH (checksum=false,appendonly=true,compresslevel=7)
);

create table gptoolkit_user_table_co (id VARCHAR, lname CHAR(20), fname CHAR(10), tincan FLOAT
    )
 WITH (orientation='column', appendonly=true)
 DISTRIBUTED BY (id)
 ;

select autnspname,autrelname,autrelkind from gp_toolkit.__gp_user_tables where autrelname like 'gptoolkit_user_table%';

-- gp_param_settings\(\)
select paramsegment,paramname from gp_toolkit.gp_param_settings() where paramname like 'gp_%' and paramsegment=0 and paramname IN ('gp_contentid', 'gp_dbid', 'gp_interconnect_type', 'gp_role', 'gp_session_id') order by 2;


-- Expected to have no results
-- However it will show there is a difference if segment guc is different
select * from gp_toolkit.gp_param_settings_seg_value_diffs;


-- gp_locks_on_relation
select lorlocktype,lorrelname,lormode,lorgranted from gp_toolkit.gp_locks_on_relation where lorrelname = 'pg_locks';

-- gp_roles_assigned
select rarolename,ramemberid,ramembername from gp_toolkit.gp_roles_assigned where rarolename like 'toolkit%';

-----------------------------------
-- Test size views.
--
-- We can't include the exact sizes in the output, as they differ slightly depending
-- on configuration. We check they are in a reasonable range.

create table toolkit_ao2 (i int, j int) with(appendonly=true);
insert into toolkit_ao2 select i, i%10 from generate_series(0, 9999) i;
create index tookit_ao2_index_j on toolkit_ao2 using bitmap (j);

select relname,
       sotusize > 50000 as sz_over50kb,
       sotusize < 5000000 as sz_under5mb
from pg_class pg, gp_toolkit.gp_size_of_table_uncompressed sotu
where relname = 'toolkit_ao2' and pg.oid=sotu.sotuoid;

-- gp_size_of_index
select pg.relname,
       si.soisize > 50000 as sz_over50kb,
       si.soisize < 5000000 as sz_under5mb
from pg_class pg, gp_toolkit.gp_size_of_index si
where relname = 'toolkit_ao2' and pg.oid=si.soitableoid;

-- gp_size_of_table_disk
select relname, 
       sotdsize > 50000 as dsz_over_50kb,
       sotdsize < 500000 as dsz_under_500kb,
       sotdtoastsize, -- should be zero
       sotdadditionalsize > 50000 as daddsz_over_50kb,
       sotdadditionalsize < 5000000 as daddsz_under_5mb
from pg_class pg, gp_toolkit.gp_size_of_table_disk st
where relname = 'toolkit_ao2' and pg.oid=st.sotdoid;

-- gp_size_of_table_uncompressed
select relname,
       sotusize > 500000 as uncomp_over_500kb,
       sotusize < 5000000 as uncomp_below_5mb
from pg_class pg, gp_toolkit.gp_size_of_table_uncompressed sotu
where relname = 'toolkit_ao2' and pg.oid=sotu.sotuoid;

-- gp_table_indexes
select pg.relname,
       ti.tiidxoid::regclass
from pg_class pg, gp_toolkit.gp_table_indexes ti
where relname = 'toolkit_ao2' and pg.oid=ti.tireloid;

-- gp_size_of_all_table_indexes
select pg.relname,
       soati.soatisize > 50000 as sz_over_50kb,
       soati.soatisize < 5000000 as sz_under_5mb
from pg_class pg, gp_toolkit.gp_size_of_all_table_indexes soati
where relname = 'toolkit_ao2' and pg.oid=soati.soatioid;

-- gp_size_of_table_and_indexes_disk
select pg.relname,
       sotai.sotaidtablesize > 500000 as tablesz_over_500kb,
       sotai.sotaididxsize < 5000000 as tablesz_below_5mb
from pg_class pg, gp_toolkit.gp_size_of_table_and_indexes_disk sotai
where relname = 'toolkit_ao2' and pg.oid=sotai.sotaidoid;

-- gp_size_of_table_and_indexes_licensing
select pg.relname,
       sotail.sotailtablesizedisk > 500000 as tables_over_500kb,
       sotail.sotailtablesizedisk < 5000000 as tables_below_5mb,
       sotail.sotailtablesizeuncompressed > 500000 as uncompressed_over_500kb,
       sotail.sotailtablesizeuncompressed < 5000000 as uncompressed_below_5mb,
       sotail.sotailindexessize > 50000 as indexes_over_500k,
       sotail.sotailindexessize < 5000000 as uncompressed_below_5mb
from pg_class pg, gp_toolkit.gp_size_of_table_and_indexes_licensing sotail
where relname = 'toolkit_ao2' and pg.oid=sotail.sotailoid;

-- gp_size_of_schema_disk
select sosdnsp,
       sosdschematablesize > 50000 as "schema size over 50 kB",
       sosdschematablesize < 10000000 as "schema size below 10 MB"
from gp_toolkit.gp_size_of_schema_disk where sosdnsp='tktest' order by 1;

-- gp_size_of_database
-- We assume the regression database is between 30 MB and 5GB in size
select sodddatname,
       sodddatsize > 30000000 as "db size over 30MB",
       sodddatsize < 5000000000 as "db size below 5 GB"
from gp_toolkit.gp_size_of_database where sodddatname='regression';

-- gp_size_of_partition_and_indexes_disk
select pg.relname,
       sopaidpartitiontablesize > 50000 as tblsz_over50k,
       sopaidpartitiontablesize < 5000000 as tblsz_under5mb,
       sopaidpartitionindexessize
from pg_class pg,gp_toolkit.gp_size_of_partition_and_indexes_disk gsopai
where pg.oid=gsopai.sopaidpartitionoid and pg.relname like 'gptoolkit_user_table_ao%';

-- This also depends on the number of segments
select count(*) > 0 from gp_toolkit.__gp_number_of_segments;


-----------------------------------
-- Test Resource Queue views


-- GP Resource Queue Activity
select * from gp_toolkit.gp_resq_activity;

-- GP Resource Queue Activity by Queue
-- There is no resource queue, so should be empty
select * from gp_toolkit.gp_resq_activity_by_queue;

-- gp_resq_role
select * from gp_toolkit.gp_resq_role where rrrolname like 'toolkit%';

-- gp_locks_on_resqueue
-- Should be empty because there is no one in the queue
select * from gp_toolkit.gp_locks_on_resqueue;

-- GP Resource Queue Activity for User
set session authorization toolkit_user1;

select resqname, resqstatus from gp_toolkit.gp_resq_activity where resqname='pg_default';
reset session authorization;
-- should be empty because the sql is completed
select * from gp_toolkit.gp_resq_activity where resqrole = 'toolkit_user1';

-- gp_pgdatabase_invalid
-- Should be empty unless there is failure in the segment, it's a view from gp_pgdatabase
select * from gp_toolkit.gp_pgdatabase_invalid;

-- Test that the statistics on resource queue usage are properly updated and
-- reflected in the pg_stat_resqueues view
set stats_queue_level=on;
create resource queue q with (active_statements = 10);
create user resqueuetest with resource queue q;
set role resqueuetest;
select 1;
select n_queries_exec from pg_stat_resqueues where queuename = 'q';
reset role;
drop role resqueuetest;
drop resource queue q;

-- GP Readable Data Table

-- Check that the tables created above are present in gp_toolkit.__gp_user_data_tables_readable
-- view.
select autnspname, autrelname, autrelkind, autoid::regclass, autrelacl
from gp_toolkit.__gp_user_data_tables_readable where autrelname like 'toolkit%';

-- Switch to non-privileged user, and test that they are no longer visible.

set session authorization toolkit_user1;
create table public.toolkit_usertest (id int4) distributed by (id);

select autnspname, autrelname, autrelkind, autoid::regclass, autrelacl
from gp_toolkit.__gp_user_data_tables_readable where autrelname like 'toolkit%';

reset session authorization;

\c regression
drop database toolkit_testdb;
drop role toolkit_user1;
drop role toolkit_admin;
