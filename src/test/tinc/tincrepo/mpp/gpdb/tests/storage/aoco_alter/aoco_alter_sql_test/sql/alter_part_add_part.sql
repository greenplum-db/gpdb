-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO table: Alter add column and then add partition

--start_ignore
Drop table if exists alter_part_tab1;
--end_ignore
Create table alter_part_tab1 (id SERIAL,a1 int ,a2 char(5) ,a3 text ) 
WITH (appendonly=true, orientation=column, compresstype=zlib) partition by list(a2) subpartition by range(a1) 
subpartition template (default subpartition subothers,subpartition sp1 start(1) end(9) with(appendonly=true,orientation=column,compresstype=rle_type), subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,compresstype=QUICKLZ))
(partition p1 values('val1') , partition p2 values('val2'));
--(default partition others, partition p1 values('val1') , partition p2 values('val2'));

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='alter_part_tab1');

Create index alter_part_tab1_idx1 on alter_part_tab1(a1);
Create index alter_part_tab1_idx2 on alter_part_tab1 using bitmap(a3);

Insert into alter_part_tab1(a1,a2,a3) values(generate_series(1,10),'val1','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall');
Insert into alter_part_tab1(a1,a2,a3) values(generate_series(11,20),'val2','We are pleased to share the September schedule for onsite legal consultations with Duane Morris, our legal counsel for all Pivotal immigration matters.  Representatives will be onsite for one-on-one consultations regarding your individual case');

\d+ alter_part_tab1_1_prt_others_2_prt_sp1

\d+ alter_part_tab1_1_prt_others_2_prt_sp2

\d+ alter_part_tab1_1_prt_others_2_prt_subothers

select count(*) from alter_part_tab1;

Update alter_part_tab1 set a3='Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.' where id>8 and id <12;
alter table alter_part_tab1 add column a4 numeric default 5.5;
update alter_part_tab1 set a4 = a1 % 2;
ALTER TABLE alter_part_tab1 ADD partition p31 values(1);
