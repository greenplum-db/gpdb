set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

-- ALTER TABLE RANGE ADD
-- FIRST LEVEL
CREATE TABLE PART_ALTER(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );

insert into part_alter (a) values (1);
insert into part_alter (a) values (1);
insert into part_alter (a) values (1);
insert into part_alter (a) values (2);
insert into part_alter (a) values (3);
insert into part_alter (a) values (4);
insert into part_alter (a) values (5);
insert into part_alter (a) values (6);
insert into part_alter (a) values (7);
insert into part_alter (a) values (8);
insert into part_alter (a) values (9);
-- failed
insert into part_alter (a) values (10);

-- MPP-3216, MPP-3059
alter table part_alter rename to part_alter2;
\d part_alter*
alter table part_alter2 rename to part_alter;
\d part_alter*

alter table part_alter add partition aa;
-- Add to the beginning of partition
alter table part_alter add partition bb start (0) end (1);
-- Expected failure, require end
alter table part_alter add partition dd start (11);
-- Add to the end of partition
alter table part_alter add partition dd end (10);
alter table part_alter add partition dd end (11);
alter table part_alter add partition ee end (12);
-- Expected failure, require spec
alter table part_alter add partition end (13);
-- Add to the end of partition
alter table part_alter add partition ff start (15) end (16);
-- Add to the middle of partition
alter table part_alter add partition gg start (13) end (14);
-- Overlapping, expected to fail
alter table part_alter add partition hh start (13) end (14);
alter table part_alter add partition hh start (1) end (10);
alter table part_alter add partition hh start (1) end (15);
alter table part_alter add partition hh start (0) end (11);
-- Add to the middle of partition
alter table part_alter add partition hh start (14) end (15);
-- Syntax error
alter table part_alter add partition zz start (-1) end (0);
-- Workaround to add single quote for negative values
alter table part_alter add partition zz start ('-1') end (0);

-- Alter Add Subpartition
alter table part_alter add subpartition template ( subpartition start (0) end (1) );

-- Requires to drop cascade?
drop table part_alter;
drop table part_alter cascade;

-- Need to SPLIT
CREATE TABLE PART_ALTER(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
-- Add default partition
alter table part_alter add default partition default_part;
alter table part_alter add partition zz start ('-1') end (0);
alter table part_alter add partition dd end (10);

drop table part_alter cascade;

-- Two Level Subpartition ALTER
CREATE TABLE PART_ALTER(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))
( partition aa start (1) end (10) every (1) );

alter table part_alter add default partition default_part;
alter table part_alter add partition bb end (11);
-- Failed
alter table part_alter add partition bb end (11);

drop table part_alter cascade;

CREATE TABLE PART_ALTER(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))
( partition aa start (1) end (10) every (1) );
-- Overlapping
alter table part_alter add partition bb end (10);
alter table part_alter add partition bb start (1) end (10);
alter table part_alter add partition bb start (2) end (5);
alter table part_alter add partition bb start ('-1') end (0);

