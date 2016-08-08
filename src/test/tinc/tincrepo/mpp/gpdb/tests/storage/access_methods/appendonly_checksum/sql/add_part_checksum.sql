-- 
-- @created 2014-10-9 12:00:00
-- @modified 2014-10-9 12:00:00
-- @tags storage
-- @description Test Alter table add column with checksum GUC off

SET gp_default_storage_options = 'checksum=false';
DROP TABLE IF EXISTS alter_part_tab1;
Create table alter_part_tab1 (id SERIAL,a1 int ,a2 char(5) ,a3 text ) 
WITH (appendonly=true, orientation=column, checksum=true,compresstype=zlib) partition by list(a2) subpartition by range(a1) 
subpartition template (default subpartition subothers,subpartition sp1 start(1) end(9) with(appendonly=true,orientation=column,checksum=true,compresstype=rle_type), subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,checksum=true,compresstype=QUICKLZ))
(partition p1 values('val1') , partition p2 values('val2'));
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1');
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p1');
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p2_2_prt_sp2');

Insert into alter_part_tab1(a1,a2,a3) values(generate_series(1,10),'val1','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall');
Insert into alter_part_tab1(a1,a2,a3) values(generate_series(11,20),'val2','We are pleased to share the September schedule for onsite legal consultations with Duane Morris, our legal counsel for all Pivotal immigration matters.  Representatives will be onsite for one-on-one consultations regarding your individual case');
select count(*) from alter_part_tab1;

alter table alter_part_tab1 add column a4 numeric default 5.5;
ALTER TABLE alter_part_tab1 ADD partition p31 values(1) WITH (appendonly=true, orientation=column, checksum=true,compresstype=zlib);

SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p31');
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p31_2_prt_sp1');
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p31_2_prt_sp2');
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'alter_part_tab1_1_prt_p31_2_prt_subothers');
