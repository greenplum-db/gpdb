-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--start_ignore
Drop table if exists sto_co_en_sp;
--end_ignore
Create table sto_co_en_sp (id SERIAL,a1 int ,a2 char(5) ,a3 text ) 
WITH (appendonly=true, orientation=column, compresstype=zlib) partition by list(a2) subpartition by range(a1) 
subpartition template (default subpartition subothers,subpartition sp1 start(1) end(9) with(appendonly=true,orientation=column,compresstype=rle_type), subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,compresstype=zlib)) (default partition others, partition p1 values('val1') , partition p2 values('val2'));

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_co_en_sp');

Create index sto_en_sp_idx1 on sto_co_en_sp(a1);
Create index sto_en_sp_idx2 on sto_co_en_sp using bitmap(a3);

Insert into sto_co_en_sp(a1,a2,a3) values(generate_series(1,10),'val1','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall');
Insert into sto_co_en_sp(a1,a2,a3) values(generate_series(11,20),'val2','We are pleased to share the September schedule for onsite legal consultations with Duane Morris, our legal counsel for all Pivotal immigration matters.  Representatives will be onsite for one-on-one consultations regarding your individual case');

\d+ sto_co_en_sp_1_prt_others_2_prt_sp1

\d+ sto_co_en_sp_1_prt_others_2_prt_sp2

\d+ sto_co_en_sp_1_prt_others_2_prt_subothers

select count(*) from sto_co_en_sp;

Update sto_co_en_sp set a3='Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.' where id>8 and id <12;

select count(*) AS only_visi_tups  from sto_co_en_sp;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_sp;

set gp_select_invisible = false;

Delete from sto_co_en_sp where a1 <3 or a1>17;

select count(*) AS only_visi_tups  from sto_co_en_sp;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_sp;

set gp_select_invisible = false;

VACUUM sto_co_en_sp;

select count(*) AS only_visi_tups_vacuum  from sto_co_en_sp;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_co_en_sp;
set gp_select_invisible = false;


