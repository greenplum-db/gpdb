\c gptest;

-- Alter table add column
Alter Table sto_alt_heap1  ADD COLUMN added_col character varying(30) default 'default';
insert into sto_alt_heap1 values ('3_zero', 3, '3_zero', 3, 3, 3, '{3}', 3, 3, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',3, 'newcol');

-- Alter table Drop column
Alter table sto_alt_heap1 Drop column float_col;
insert into sto_alt_heap1 values ('4_zero', 4, '4_zero', 4, 4, '{4}', 4, 4, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',4, 'newcol');
select * from sto_alt_heap1 order by bigint_col;

-- Alter table rename column
Alter Table sto_alt_heap1 RENAME COLUMN before_rename_col TO after_rename_col;
select * from sto_alt_heap1 order by bigint_col;

-- Alter table column type
Alter Table sto_alt_heap1 ALTER COLUMN change_datatype_col TYPE int4;
insert into sto_alt_heap1 values ('5_zero', 5, '5_zero', 5, 5, '{5}', 5, 5, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',5, 'newcol');
select * from sto_alt_heap1 order by bigint_col;

-- Alter column set default expression
Alter Table sto_alt_heap1 ALTER COLUMN col_set_default SET DEFAULT 0;
select * from sto_alt_heap1 order by bigint_col;

-- Alter column Drop default
Alter table sto_alt_heap1  alter column text_col drop default;
select * from sto_alt_heap1 order by bigint_col;

-- Alter column drop NOT NULL
Alter Table sto_alt_heap1 ALTER COLUMN int_col DROP NOT NULL;
select * from sto_alt_heap1 order by bigint_col;

-- Alter column set NOT NULL
Alter Table sto_alt_heap1 ALTER COLUMN int_col SET NOT NULL;
select * from sto_alt_heap1 order by bigint_col;

-- Alter table SET STORAGE
Alter Table sto_alt_heap1 ALTER char_vary_col SET STORAGE PLAIN;
insert into sto_alt_heap1 values ('6_zero', 6, '6_zero', 6, 6, '{6}', 6, 6, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',6, 'newcol');
select * from sto_alt_heap1 order by bigint_col;

-- Alter table inherit parent table
ALTER TABLE sto_alt_heap2 INHERIT sto_heap_parent;
select * from sto_alt_heap2 order by bigint_col;

-- Alter table set distributed byt to a new column
Alter Table sto_alt_heap2 SET distributed by(text_col);
insert into sto_alt_heap2 values ('2_zero', 2, '2_zero', 2);
select * from sto_alt_heap2 order by bigint_col;

--Alter table set distributed randomly
Alter Table sto_alt_heap2 SET distributed randomly;
insert into sto_alt_heap2 values ('3_zero', 3, '3_zero', 3);
select * from sto_alt_heap2 order by bigint_col;

-- Alter table set Reorganize to true 
Alter Table sto_alt_heap2 SET WITH (reorganize=true);
insert into sto_alt_heap2 values ('4_zero', 4, '4_zero', 4);
select * from sto_alt_heap2 order by bigint_col;

-- Alter table set Reorganize to false 
Alter Table sto_alt_heap2 SET WITH (reorganize=false);
insert into sto_alt_heap2 values ('5_zero', 5, '5_zero', 5);
select * from sto_alt_heap2 order by bigint_col;

-- Alter table add table constraint
ALTER TABLE sto_alt_heap2  ADD CONSTRAINT lenchk CHECK (char_length(char_vary_col) < 10);
insert into sto_alt_heap2 values ('6_zero', 6, '6_zero', 6);
select * from sto_alt_heap2 order by bigint_col;

-- Alter table drop constriant
Alter table sto_alt_heap2  Drop constraint lenchk;
insert into sto_alt_heap2 values ('7_zero', 7, '7_zero', 7);
select * from sto_alt_heap2 order by bigint_col;

-- Alter column  set statistics
Alter table sto_alt_heap3  alter column before_rename_col set statistics 3;
select * from sto_alt_heap3 order by bigint_col;

-- Alter table SET without cluster
Alter table sto_alt_heap3 set without cluster;
select * from sto_alt_heap3 order by bigint_col;

--Alter table SET without OIDs
Alter table sto_alt_heap3 SET without oids;
select * from sto_alt_heap3 order by bigint_col;

--Alter table to a new owner
-- start_ignore
Drop table if exists sto_alt_heap4 ;
Drop role if exists heap_user1;
-- end_ignore
Create role heap_user1;
Create table sto_alt_heap4  as select * from sto_alt_heap3;
Alter Table sto_alt_heap4  Owner to heap_user1;
select * from sto_alt_heap4 order by bigint_col;
