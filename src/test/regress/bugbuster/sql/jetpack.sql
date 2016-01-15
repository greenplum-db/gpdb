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
select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='toolkit_testschema';

create schema toolkit_testschema;

select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='toolkit_testschema';

drop schema toolkit_testschema;

select aunnspname from gp_toolkit.__gp_user_namespaces where aunnspname='toolkit_testschema';
select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;

create table toolkit_a (a int);
create table toolkit_b (b int);

select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;

drop table toolkit_a;
drop table toolkit_b;

select  autnspname, autrelname, autrelkind, autreltuples, autrelpages, autrelacl from gp_toolkit.__gp_user_data_tables where autrelname like 'toolkit%' order by 2;
select * from gp_toolkit.__gp_masterid;
\d gp_toolkit.gp_log_database
select logseverity from gp_toolkit.gp_log_database where logseverity='LOG' limit 10;

-- Test the gp_skew_idle_fractions view
create table toolkit_skew (a int);
insert into toolkit_skew select i from generate_series(1,1000) i;
select sifnamespace, sifrelname from gp_toolkit.gp_skew_idle_fractions where sifoid = 'toolkit_skew'::regclass;
drop table toolkit_skew;

create schema jetpack_test;
set search_path=jetpack_test;

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

-- Expected to have no results
-- However it will show there is a difference if segment guc is different
select * from gp_toolkit.gp_param_settings_seg_value_diffs;
select lorlocktype,lorrelname,lormode,lorgranted from gp_toolkit.gp_locks_on_relation where lorrelname like 'pg_%';

-- Test size views. We can't include the exact sizes in the output, as they differ
-- slightly depending on configuration, so just check that it's reasonable
select relname, sotusize > 50000 as sz_over50k, sotusize < 1000000 as sz_under1mb from pg_class pg, gp_toolkit.gp_size_of_table_uncompressed sotu where relname = 'toolkit_ao' and pg.relfilenode=sotu.sotuoid;

select pg.relname,
       sopaidpartitiontablesize > 50000 as tblsz_over50k, sopaidpartitiontablesize < 1000000 as tblsz_under1mb,
       sopaidpartitionindexessize
from pg_class pg,gp_toolkit.gp_size_of_partition_and_indexes_disk gsopai
where pg.relfilenode=gsopai.sopaidpartitionoid and pg.relname like 'gptoolkit_user_table_ao%';

-- Test __gp_localid and __gp_masterid functions. The output of __gp_localid
-- depends on the number of segments, so just check that it returns something.
select count(*) > 0 from gp_toolkit.__gp_localid;
select * from gp_toolkit.__gp_masterid;
