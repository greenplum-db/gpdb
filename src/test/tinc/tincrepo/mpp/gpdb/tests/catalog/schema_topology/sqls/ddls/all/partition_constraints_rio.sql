-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Partiton constraints

--start_ignore
drop table if exists mulpt_pk;
--end_ignore
-- Create multi-level partitioned tables with CONSTRAINTS on distcol and ptcol
create table mulpt_pk
(
distcol int,
ptcol text,
subptcol int,
col1 int,
PRIMARY KEY(distcol, ptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);
--start_ignore
drop table if exists mulpt_un;
--end_ignore
create table mulpt_un
(
distcol int,
ptcol text,
subptcol int,
col1 int,
UNIQUE(distcol, ptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

--check that the tables are not created
select tablename from pg_tables where tablename in ('mulpt_pk','mulpt_un') order by tablename;

-- create the tables with CONSTRAINTS on distcol,ptcol and subptcol, check that the tables are created.

--start_ignore
drop table if exists mulpt_pk;
--end_ignore
create table mulpt_pk
(
distcol int,
ptcol text,
subptcol int,
col1 int,
PRIMARY KEY(distcol, ptcol,subptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

--start_ignore
drop table if exists mulpt_un;
--end_ignore
create table mulpt_un
(
distcol int,
ptcol text,
subptcol int,
col1 int,
UNIQUE(distcol, ptcol,subptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

select tablename from pg_tables where tablename in ('mulpt_pk','mulpt_un') order by tablename;


--check the constraints are present on the parent table and its partitions
select distinct conname from anyconstraint where contype='p' and tableid='mulpt_pk'::regclass ;
select conname,partid from anyconstraint where contype='p' and tableid='mulpt_pk'::regclass order by partid;
select distinct conname from anyconstraint where contype='u' and tableid='mulpt_un'::regclass;
select conname,partid from anyconstraint where contype='u' and tableid='mulpt_un'::regclass order by partid;

