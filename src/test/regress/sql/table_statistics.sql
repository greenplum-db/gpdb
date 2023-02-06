\echo '-- start_ignore'
drop schema stat_heap1 cascade;
create schema stat_heap1;
set search_path=stat_heap1,"$user",public;
\echo '-- end_ignore'
set optimizer_print_missing_stats = off;
-- Regular Table
Drop table if exists stat_heap_t1;

Create table stat_heap_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t1 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Create index stat_idx_heap_t1 on stat_heap_t1(i);

Analyze stat_heap_t1;

select count(*) from pg_class where relname like 'stat_heap_t1%';

-- Alter table without a default value
Alter table stat_heap_t1 add column new_col varchar;
 
select  count(*) from pg_class where relname like 'stat_heap_t1%';

-- Alter table with a default value
Alter table stat_heap_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_heap_t1%';

-- Partiiton Table
Drop table if exists stat_part_heap_t1;

Create table stat_part_heap_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t1 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t1;

select  count(*) from pg_class where relname like 'stat_part_heap_t1';

-- Alter table without a default value
Alter table stat_part_heap_t1 add column new_col varchar;

select  count(*) from pg_class where relname like 'stat_part_heap_t1';

-- Alter table with a default value
Alter table stat_part_heap_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_part_heap_t1';

\echo '-- start_ignore'
drop schema stat_ao1 cascade;
create schema stat_ao1;
set search_path=stat_ao1,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t1;

Create table stat_ao_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t1 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t1;

select  count(*) from pg_class where relname like 'stat_ao_t1%';

-- Alter table with a default value
Alter table stat_ao_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_ao_t1%';

-- Partiiton Table
Drop table if exists stat_part_ao_t1;

Create table stat_part_ao_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t1 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Create index stat_part_idx_ao_t1 on stat_part_ao_t1(d);

Analyze stat_part_ao_t1;

select  count(*) from pg_class where relname like 'stat_part_ao_t1';

-- Alter table with a default value
Alter table stat_part_ao_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_part_ao_t1';

\echo '-- start_ignore'
drop schema stat_co1 cascade;
create schema stat_co1;
set search_path=stat_co1,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t1;

Create table stat_co_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t1 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t1;

select  count(*) from pg_class where relname like 'stat_co_t1%';

-- Alter table with a default value
Alter table stat_co_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_co_t1%';

-- Partiiton Table
Drop table if exists stat_part_co_t1;

Create table stat_part_co_t1 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t1 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t1;

select  count(*) from pg_class where relname like 'stat_part_co_t1';

-- Alter table with a default value
Alter table stat_part_co_t1 add column new_col2 text default 'new column with new value';

select  count(*) from pg_class where relname like 'stat_part_co_t1';

\echo '-- start_ignore'
drop schema stat_heap2 cascade;
create schema stat_heap2;
set search_path=stat_heap2,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t2;

Create table stat_heap_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t2 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t2;

select  count(*) from pg_class where relname like 'stat_heap_t2%';

-- Alter table drop column
Alter table stat_heap_t2 drop column d;
 
select  count(*) from pg_class where relname like 'stat_heap_t2%';

-- Partiiton Table
Drop table if exists stat_part_heap_t2;

Create table stat_part_heap_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t2 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t2;

select  count(*) from pg_class where relname like 'stat_part_heap_t2';

-- Alter table drop column
Alter table stat_part_heap_t2 drop column d;

select  count(*) from pg_class where relname like 'stat_part_heap_t2';

\echo '-- start_ignore'
drop schema stat_ao2 cascade;
create schema stat_ao2;
set search_path=stat_ao2,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t2;

Create table stat_ao_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t2 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t2;

select  count(*) from pg_class where relname like 'stat_ao_t2%';

-- Alter table drop column
Alter table stat_ao_t2 drop column t;

select  count(*) from pg_class where relname like 'stat_ao_t2%';

-- Partiiton Table
Drop table if exists stat_part_ao_t2;

Create table stat_part_ao_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t2 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t2;

select  count(*) from pg_class where relname like 'stat_part_ao_t2';

-- Alter table drop column
Alter table stat_part_ao_t2 drop column t;

select  count(*) from pg_class where relname like 'stat_part_ao_t2';

\echo '-- start_ignore'
drop schema stat_co2 cascade;
create schema stat_co2;
set search_path=stat_co2,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t2;

Create table stat_co_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t2 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t2;

select  count(*) from pg_class where relname like 'stat_co_t2%';

-- Alter table drop column
Alter table stat_co_t2 drop column t;


select  count(*) from pg_class where relname like 'stat_co_t2%';

-- Partiiton Table
Drop table if exists stat_part_co_t2;

Create table stat_part_co_t2 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t2 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t2;

select  count(*) from pg_class where relname like 'stat_part_co_t2';

-- Alter table drop column
Alter table stat_part_co_t2 drop column t;

select  count(*) from pg_class where relname like 'stat_part_co_t2';

\echo '-- start_ignore'
drop schema stat_heap3 cascade;
create schema stat_heap3;
set search_path=stat_heap3,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t3;

Create table stat_heap_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t3 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t3;

select  count(*) from pg_class where relname like 'stat_heap_t3%';

-- Alter distribution
Alter table stat_heap_t3 set distributed by (j);
 
select  count(*) from pg_class where relname like 'stat_heap_t3%';

-- Partiiton Table
Drop table if exists stat_part_heap_t3;

Create table stat_part_heap_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed by (i)  partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t3 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Create index stat_part_idx_heap_t3 on stat_part_heap_t3(d);

Analyze stat_part_heap_t3;

select  count(*) from pg_class where relname like 'stat_part_heap_t3';

-- Alter distribution
Alter table stat_part_heap_t3 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_heap_t3';

\echo '-- start_ignore'
drop schema stat_ao3 cascade;
create schema stat_ao3;
set search_path=stat_ao3,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t3;

Create table stat_ao_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t3 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t3;

select  count(*) from pg_class where relname like 'stat_ao_t3%';

-- Alter distribution
Alter table stat_ao_t3 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_ao_t3%';

-- Partiiton Table
Drop table if exists stat_part_ao_t3;

Create table stat_part_ao_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t3 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t3;

select  count(*) from pg_class where relname like 'stat_part_ao_t3';

-- Alter distribution
Alter table stat_part_ao_t3 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_ao_t3';

\echo '-- start_ignore'
drop schema stat_co3 cascade;
create schema stat_co3;
set search_path=stat_co3,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t3;

Create table stat_co_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t3 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t3;

select  count(*) from pg_class where relname like 'stat_co_t3%';

-- Alter distribution
Alter table stat_co_t3 set distributed by (j);


select  count(*) from pg_class where relname like 'stat_co_t3%';

-- Partiiton Table
Drop table if exists stat_part_co_t3;

Create table stat_part_co_t3 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t3 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t3;

select  count(*) from pg_class where relname like 'stat_part_co_t3';

-- Alter distribution
Alter table stat_part_co_t3 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_co_t3';

\echo '-- start_ignore'
drop schema stat_heap4 cascade;
create schema stat_heap4;
set search_path=stat_heap4,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t4;

Create table stat_heap_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t4 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t4;

select  count(*) from pg_class where relname like 'stat_heap_t4%';

-- Alter distribution
Alter table stat_heap_t4 set distributed randomly;
 
select  count(*) from pg_class where relname like 'stat_heap_t4%';

Alter table stat_heap_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_heap_t4%';

-- Partiiton Table
Drop table if exists stat_part_heap_t4;

Create table stat_part_heap_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed by (i)  partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t4 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t4;

select  count(*) from pg_class where relname like 'stat_part_heap_t4';

-- Alter distribution
Alter table stat_part_heap_t4 set distributed randomly;

select  count(*) from pg_class where relname like 'stat_part_heap_t4';

Alter table stat_part_heap_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_heap_t4';

\echo '-- start_ignore'
drop schema stat_ao4 cascade;
create schema stat_ao4;
set search_path=stat_ao4,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t4;

Create table stat_ao_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t4 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t4;

select  count(*) from pg_class where relname like 'stat_ao_t4%';

-- Alter distribution
Alter table stat_ao_t4 set distributed randomly;

select  count(*) from pg_class where relname like 'stat_ao_t4%';

Alter table stat_ao_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_ao_t4%';

-- Partiiton Table
Drop table if exists stat_part_ao_t4;

Create table stat_part_ao_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t4 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t4;

select  count(*) from pg_class where relname like 'stat_part_ao_t4';

-- Alter distribution
Alter table stat_part_ao_t4 set distributed randomly;

select  count(*) from pg_class where relname like 'stat_part_ao_t4';

Alter table stat_part_ao_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_ao_t4';

\echo '-- start_ignore'
drop schema stat_co4 cascade;
create schema stat_co4;
set search_path=stat_co4,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t4;

Create table stat_co_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t4 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t4;

select  count(*) from pg_class where relname like 'stat_co_t4%';

-- Alter distribution
Alter table stat_co_t4 set distributed randomly;

select  count(*) from pg_class where relname like 'stat_co_t4%';

Alter table stat_co_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_co_t4%';

-- Partiiton Table
Drop table if exists stat_part_co_t4;

Create table stat_part_co_t4 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t4 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t4;

select  count(*) from pg_class where relname like 'stat_part_co_t4';

-- Alter distribution
Alter table stat_part_co_t4 set distributed randomly;

select  count(*) from pg_class where relname like 'stat_part_co_t4';

Alter table stat_part_co_t4 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_co_t4';

\echo '-- start_ignore'
drop schema stat_heap5 cascade;
create schema stat_heap5;
set search_path=stat_heap5,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t5;

Create table stat_heap_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed randomly;

Insert into stat_heap_t5 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t5;

select  count(*) from pg_class where relname like 'stat_heap_t5%';

-- Alter distribution
Alter table stat_heap_t5 set distributed by (j);
 
select  count(*) from pg_class where relname like 'stat_heap_t5%';

-- Partiiton Table
Drop table if exists stat_part_heap_t5;

Create table stat_part_heap_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly  partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t5 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t5;

select  count(*) from pg_class where relname like 'stat_part_heap_t5';

-- Alter distribution
Alter table stat_part_heap_t5 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_heap_t5';

\echo '-- start_ignore'
drop schema stat_ao5 cascade;
create schema stat_ao5;
set search_path=stat_ao5,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t5;

Create table stat_ao_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed randomly ;

Insert into stat_ao_t5 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t5;

select  count(*) from pg_class where relname like 'stat_ao_t5%';

-- Alter distribution
Alter table stat_ao_t5 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_ao_t5%';

-- Partiiton Table
Drop table if exists stat_part_ao_t5;

Create table stat_part_ao_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t5 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t5;

select  count(*) from pg_class where relname like 'stat_part_ao_t5';

-- Alter distribution
Alter table stat_part_ao_t5 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_ao_t5';

\echo '-- start_ignore'
drop schema stat_co5 cascade;
create schema stat_co5;
set search_path=stat_co5,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t5;

Create table stat_co_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed randomly;

Insert into stat_co_t5 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t5;

select  count(*) from pg_class where relname like 'stat_co_t5%';

-- Alter distribution
Alter table stat_co_t5 set distributed by (j);


select  count(*) from pg_class where relname like 'stat_co_t5%';

-- Partiiton Table
Drop table if exists stat_part_co_t5;

Create table stat_part_co_t5 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t5 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t5;

select  count(*) from pg_class where relname like 'stat_part_co_t5';

-- Alter distribution
Alter table stat_part_co_t5 set distributed by (j);

select  count(*) from pg_class where relname like 'stat_part_co_t5';

\echo '-- start_ignore'
drop schema stat_heap6 cascade;
create schema stat_heap6;
set search_path=stat_heap6,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t6;

Create table stat_heap_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t6 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t6;

select  count(*) from pg_class where relname like 'stat_heap_t6%';

-- Alter table to reorganize = true
Alter table stat_heap_t6 set with (reorganize=true);
 
select  count(*) from pg_class where relname like 'stat_heap_t6%';

-- Alter table to reorganize = false
Alter table stat_heap_t6 set with (reorganize=false);

select  count(*) from pg_class where relname like 'stat_heap_t6%';

-- Partiiton Table
Drop table if exists stat_part_heap_t6;

Create table stat_part_heap_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed by (i)  partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t6 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Create index stat_part_idx_heap_t6 on stat_part_heap_t6(d);

Analyze stat_part_heap_t6;

select  count(*) from pg_class where relname like 'stat_part_heap_t6';

-- Alter table to reorganize = true
Alter table stat_part_heap_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_heap_t6';

-- Alter table to reorganize = false
Alter table stat_part_heap_t6 set with (reorganize=false);

select  count(*) from pg_class where relname like 'stat_part_heap_t6';

\echo '-- start_ignore'
drop schema stat_ao6 cascade;
create schema stat_ao6;
set search_path=stat_ao6,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t6;

Create table stat_ao_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t6 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t6;

select  count(*) from pg_class where relname like 'stat_ao_t6%';

-- Alter table to reorganize = true
Alter table stat_ao_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_ao_t6%';

-- Alter table to reorganize = false
Alter table stat_ao_t6 set with (reorganize=false);

select  count(*) from pg_class where relname like 'stat_ao_t6%';

-- Partiiton Table
Drop table if exists stat_part_ao_t6;

Create table stat_part_ao_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t6 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t6;

select  count(*) from pg_class where relname like 'stat_part_ao_t6';

-- Alter table to reorganize = true
Alter table stat_part_ao_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_ao_t6';

-- Alter table to reorganize = false
Alter table stat_part_ao_t6 set with (reorganize=false);

select  count(*) from pg_class where relname like 'stat_part_ao_t6';

\echo '-- start_ignore'
drop schema stat_co6 cascade;
create schema stat_co6;
set search_path=stat_co6,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t6;

Create table stat_co_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t6 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t6;

select  count(*) from pg_class where relname like 'stat_co_t6%';

-- Alter table to reorganize = true
Alter table stat_co_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_co_t6%';

-- Alter table to reorganize = false
Alter table stat_co_t6 set with (reorganize=false);

select  count(*) from pg_class where relname like 'stat_co_t6%';

-- Partiiton Table
Drop table if exists stat_part_co_t6;

Create table stat_part_co_t6 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t6 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t6;

select  count(*) from pg_class where relname like 'stat_part_co_t6';

-- Alter table to reorganize = true
Alter table stat_part_co_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_co_t6';

-- Alter table to reorganize = false
Alter table stat_part_co_t6 set with (reorganize=true);

select  count(*) from pg_class where relname like 'stat_part_co_t6';

\echo '-- start_ignore'
drop schema stat_heap7 cascade;
create schema stat_heap7;
set search_path=stat_heap7,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t7;

Create table stat_heap_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t7 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t7;

select  count(*) from pg_class where relname like 'stat_heap_t7%';

-- Alter table alter column type
Alter table stat_heap_t7 alter column c type varchar;
 
select  count(*) from pg_class where relname like 'stat_heap_t7%';

-- Partiiton Table
Drop table if exists stat_part_heap_t7;

Create table stat_part_heap_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t7 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t7;

select  count(*) from pg_class where relname like 'stat_part_heap_t7';

-- Alter table alter type of a column
Alter table stat_part_heap_t7 alter column j type numeric;

select  count(*) from pg_class where relname like 'stat_part_heap_t7';


\echo '-- start_ignore'
drop schema stat_ao7 cascade;
create schema stat_ao7;
set search_path=stat_ao7,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_ao_t7;

Create table stat_ao_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5) distributed by (i) ;

Insert into stat_ao_t7 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_ao_t7;

select  count(*) from pg_class where relname like 'stat_ao_t7%';

-- Alter table alter column type
Alter table stat_ao_t7 alter column j type numeric;

select  count(*) from pg_class where relname like 'stat_ao_t7%';

-- Partiiton Table
Drop table if exists stat_part_ao_t7;

Create table stat_part_ao_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_ao_t7 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_ao_t7;

select  count(*) from pg_class where relname like 'stat_part_ao_t7';

-- Alter table alter column type
Alter table stat_part_ao_t7 alter column c type varchar;

select  count(*) from pg_class where relname like 'stat_part_ao_t7';

\echo '-- start_ignore'
drop schema stat_co7 cascade;
create schema stat_co7;
set search_path=stat_co7,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_co_t7;

Create table stat_co_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true,compresslevel=5, orientation=column) distributed by (i) ;

Insert into stat_co_t7 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_co_t7;

select  count(*) from pg_class where relname like 'stat_co_t7%';

-- Alter table alter type
Alter table stat_co_t7 alter column j type float;

select  count(*) from pg_class where relname like 'stat_co_t7%';

-- Partiiton Table
Drop table if exists stat_part_co_t7;

Create table stat_part_co_t7 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
with(appendonly=true,compresslevel=5, orientation=column) distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_co_t7 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_co_t7;

select  count(*) from pg_class where relname like 'stat_part_co_t7';

-- Alter table alter type
Alter table stat_part_co_t7 alter column i type numeric;

select  count(*) from pg_class where relname like 'stat_part_co_t7';

\echo '-- start_ignore'
drop schema stat_heap8 cascade;
create schema stat_heap8;
set search_path=stat_heap8,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t8;

Create table stat_heap_t8 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t8 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t8;

select  count(*) from pg_class where relname like 'stat_heap_t8%';

-- Create Index
Create index stat_idx_heap_t8 on stat_heap_t8(n);

-- Cluster on index
Cluster stat_idx_heap_t8 on stat_heap_t8;

select  count(*) from pg_class where relname like 'stat_heap_t8%';

-- Partiiton Table
Drop table if exists stat_part_heap_t8;

Create table stat_part_heap_t8 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t8 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t8;

select  count(*) from pg_class where relname like 'stat_part_heap_t8';

-- Create Index
Create index stat_part_idx_heap_t8 on stat_part_heap_t8(d);

-- Cluster on index
Cluster stat_part_idx_heap_t8 on stat_part_heap_t8;

select  count(*) from pg_class where relname like 'stat_part_heap_t8';

\echo '-- start_ignore'
drop schema stat_heap9 cascade;
create schema stat_heap9;
set search_path=stat_heap9,"$user",public;
\echo '-- end_ignore'

-- Regular Table
Drop table if exists stat_heap_t9;

Create table stat_heap_t9 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i);

Insert into stat_heap_t9 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_heap_t9;

select  count(*) from pg_class where relname like 'stat_heap_t9%';

-- Create Index
Create index stat_idx_heap_t9 on stat_heap_t9(i);

-- Cluster on index
Cluster stat_idx_heap_t9 on stat_heap_t9;

select  count(*) from pg_class where relname like 'stat_heap_t9%';

Insert into stat_heap_t9 values(generate_series(1,10),500,'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

-- Cluster again 
Cluster stat_heap_t9;

select  count(*) from pg_class where relname like 'stat_heap_t9%';


-- Partiiton Table
Drop table if exists stat_part_heap_t9;

Create table stat_part_heap_t9 (i int,j int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone)  
distributed randomly partition by range (i) 
subpartition by list (j) subpartition template 
(
default subpartition subothers,
subpartition sub1 values(1,2,3), 
subpartition sub2 values(4,5,6)
) 
(default partition others, start (0) inclusive end (10) exclusive every (5) );

Insert into stat_part_heap_t9 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Analyze stat_part_heap_t9;

select  count(*) from pg_class where relname like 'stat_part_heap_t9';

-- Create Index
Create index stat_part_idx_heap_t9 on stat_part_heap_t9(j);

-- Cluster on index
Cluster stat_part_idx_heap_t9 on stat_part_heap_t9;

select  count(*) from pg_class where relname like 'stat_part_heap_t9';

Insert into stat_part_heap_t9 values(generate_series(1,10),generate_series(1,5),'table statistics should be kept after alter','s', 'regular table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

-- Cluster again
Cluster stat_part_heap_t9;

select  count(*) from pg_class where relname like 'stat_part_heap_t9';

