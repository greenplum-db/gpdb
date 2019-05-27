set optimizer_print_missing_stats = off;
-- MPP-23647 Create a partitioned appendonly table, let its age
-- increase during the test.  We will vacuum it at the end of the
-- test.
create table ao_age_test (i int, b bool, c char, d date)
with (appendonly=true)
distributed by (i)
partition by list (b)
(partition b1 values ('f'),
 partition b2 values ('t')
);
insert into ao_age_test select i, (i%123 > 50), (i/11) || '',
'2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;

create index ao_age_test_i on ao_age_test(i);

-- MPP-23647 Create a empty table with no segments, let its age
-- increase during the test.  We will vacuum it at the end of the
-- test.
create table ao_empty (a int, b int, c varchar)
with (appendonly=true) distributed by (a);

-- MPP-23647 Empty table with non-zero segments but each segment has
-- tupcount = 0.  Also cover column oriented table.
create table aocs_empty (a int, b int, c varchar)
with (appendonly=true, orientation=column) distributed by (a);
insert into aocs_empty select i, i, i::text from generate_series(1,20)i;
-- This update should create a new appendonly segment.
update aocs_empty set c = 'updated' where a > 9;
delete from aocs_empty;
-- Make both the appendonly segments for this table empty.  Let the
-- age of this table grow during the test.  We will vacuum it again at
-- the end.
vacuum aocs_empty;

-- MPP-23647 Ensure pg_class.relhasindex for an empty appendonly table
-- is correctly interpreted by vacuum.
create index ao_empty_a on ao_empty(a);
create index aocs_empty_a on aocs_empty(a);
\d ao_empty
\d aocs_empty

-- vacuum/analyze a table with indexes
create table vactst (i int, b bool, c char, d date);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;

create index vactst_c on vactst (c);
vacuum vactst;
analyze vactst;

create index vactst_b on vactst using bitmap(b);
vacuum vactst;
analyze vactst;

vacuum analyze vactst;

drop table vactst;

create table vactst (i int, b bool, c char, d date);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;

create index vactst_c on vactst (c);
create index vactst_b on vactst using bitmap(b);
vacuum analyze vactst;

drop table vactst;

-- vacuum analyze a table that has dropped a column
create table vactst (i int, b bool, c char, d date);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;
alter table vactst drop column b;
vacuum analyze vactst;

alter table vactst drop column i;
vacuum analyze vactst;
drop table vactst;

-- vacuum analyze a table whose index has pg_statistic stats
create table vactst (i int, b bool, c char, d date);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;
create index vactst_c on vactst (upper(c));
vacuum analyze vactst;
drop table vactst;

-- vacuum analyze an AO table
create table vactst (i int, b bool, c char, d date) with (appendonly=true);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;
vacuum analyze vactst;
drop table vactst;

-- vacuum analyze a partition table
create table vactst (i int, b bool, c char, d date)
distributed by (i)
partition by list (b)
(partition b1 values ('f'),
 partition b2 values ('t')
);
insert into vactst select i, (i%123 > 50), (i/11) || '', '2008/10/12'::date + (i || ' days')::interval
from generate_series(0, 99) i;
vacuum analyze vactst;
drop table vactst;

-- MPP-23647 Vacuum appendonly partitioned table ao_age_test, check
-- that its age is correctly updated along with its partitions.
set vacuum_freeze_min_age=20;
show vacuum_freeze_min_age;
select relname from pg_class where relname like 'ao_age_test%'
order by relname;
vacuum ao_age_test;

-- Note: in checking age() below, be mindful of not checking absolute
-- age value in expected output so as to make the test reliable.

-- Assuming no other activity, vacuum needs one transaction ID for
-- each of the three tables.
-- AO/CO tables should have relfrozenxid = 0.
select relname, relfrozenxid from pg_class
where relname like 'ao_age_test%' and relkind = 'r' order by 1;

-- Vacuum the other two empty tables and verify the age of auxiliary tables is
-- updated correctly.
vacuum ao_empty;
vacuum aocs_empty;
-- AO/CO tables should have relfrozenxid = 0.
select relname, relfrozenxid from pg_class
where relname in ('ao_empty', 'aocs_empty') order by 1;
select * from ao_empty;
select * from aocs_empty;

-- Verify that age of appendonly auxiliary tables is update by vacuum.
select 0 < age(relfrozenxid) as age_positive,
       age(relfrozenxid) < 100 as age_within_limit
from pg_class c, pg_appendonly a
where c.oid = a.segrelid and
	   (a.relid = 'ao_empty'::regclass or
	    a.relid = 'aocs_empty'::regclass);
-- Verify that age of toast table is updated by vacuum.
select 0 < age(relfrozenxid) as age_positive,
       age(relfrozenxid) < 100 as age_within_limit
from pg_class where oid in (select reltoastrelid from pg_class
       where relname = 'ao_empty' or relname = 'aocs_empty');

-- Verify that index is displayed by \d after vacuum.
\d ao_empty;
\d aocs_empty;

-- Ensure that reindex after vacuum works fine.
alter table ao_age_test set with (reorganize='true')
distributed by (c);

-- Force an index scan and verify index lookup work fine after vacuum
-- and reorganize.
set enable_seqscan = false;
select * from ao_age_test where i in (1, 2, 11, 13, 15)
order by i;

drop table ao_age_test;
drop table ao_empty;
drop table aocs_empty;

-- Create some dead tuples, and test VACUUM FULL on the AO/CO auxiliary
-- relations. (There used to be a bug where it didn't do anything, and you got a
-- NOTICE saying a transaction is in progress instead.)
create table co_t1(a int, b int) with (appendonly=true, orientation=column) distributed by(a);
insert into co_t1 select i, i from generate_series(1, 10000) i;
update co_t1 set b = b + 1;
vacuum full co_t1;
drop table co_t1;

create table ao_t1(a int, b int) with (appendonly=true) distributed by(a);
insert into ao_t1 select i, i from generate_series(1, 10000) i;
update ao_t1 set b = b + 1;
vacuum full ao_t1;
drop table ao_t1;

-- superuser must be able to vacuum analyze the table
CREATE ROLE r_priv_test;
CREATE SCHEMA s_priv_test;
CREATE TABLE s_priv_test.t_priv_table(a INT);
INSERT INTO s_priv_test.t_priv_table SELECT i FROM generate_series(1, 10)i;
ALTER TABLE s_priv_test.t_priv_table OWNER TO r_priv_test;
VACUUM ANALYZE s_priv_test.t_priv_table;
DROP SCHEMA s_priv_test CASCADE;
DROP ROLE r_priv_test;

-- Check how reltuples/relpages are updated on a partitioned table, on
-- VACUUM and ANALYZE.
set gp_autostats_mode='none';
CREATE TABLE vacuum_gp_pt (a int, b int) DISTRIBUTED BY (a) PARTITION BY range (b) (END(5), START(5));
INSERT INTO vacuum_gp_pt SELECT 0, 6 FROM generate_series(1, 12);
SELECT relname, reltuples, relpages FROM pg_catalog.pg_class WHERE relname like 'vacuum_gp_pt%';
ANALYZE vacuum_gp_pt;
SELECT relname, reltuples, relpages FROM pg_catalog.pg_class WHERE relname like 'vacuum_gp_pt%';
VACUUM vacuum_gp_pt;
SELECT relname, reltuples, relpages FROM pg_catalog.pg_class WHERE relname like 'vacuum_gp_pt%';
VACUUM ANALYZE vacuum_gp_pt;
SELECT relname, reltuples, relpages FROM pg_catalog.pg_class WHERE relname like 'vacuum_gp_pt%';
reset gp_autostats_mode;

-- Check forbidden relkind for vacuum is correctly skipped
CREATE SEQUENCE s_serial START 100;
VACUUM (ANALYZE, VERBOSE) s_serial;
DROP SEQUENCE s_serial;
VACUUM gp_toolkit.__gp_log_master_ext;
