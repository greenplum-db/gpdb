-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--Zlib

--start_ignore
Drop table if exists sto_co_en_zlib;
--end_ignore
Create table sto_co_en_zlib (id SERIAL,
     a1 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=65536),a2 char(5) ENCODING (compresstype=zlib,compresslevel=3,blocksize=32768),a3 text ENCODING (compresstype=zlib,compresslevel=7,blocksize=8192)) WITH (appendonly=true, orientation=column) distributed randomly;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_co_en_zlib');

Create index sto_en_zlb_idx1 on sto_co_en_zlib(a1);
Create index sto_en_zlb_idx2 on sto_co_en_zlib using bitmap(a3);

Insert into sto_co_en_zlib(a1,a2,a3) values(generate_series(1,10),'val1','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall');
Insert into sto_co_en_zlib(a1,a2,a3) values(generate_series(11,20),'val2','We are pleased to share the September schedule for onsite legal consultations with Duane Morris, our legal counsel for all Pivotal immigration matters.  Representatives will be onsite for one-on-one consultations regarding your individual case');
\d+ sto_co_en_zlib

select count(*) from sto_co_en_zlib;

Update sto_co_en_zlib set a3='Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please b
erespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a saf
ety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.' where id=3;
 
select count(*) AS only_visi_tups  from sto_co_en_zlib;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_zlib ;

set gp_select_invisible = false;

Delete from sto_co_en_zlib where a1>8 and a1<13;

select count(*) AS only_visi_tups  from sto_co_en_zlib;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_zlib ;

set gp_select_invisible = false;

VACUUM sto_co_en_zlib;

select count(*) AS only_visi_tups_vacuum  from sto_co_en_zlib;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_co_en_zlib;
set gp_select_invisible = false;

--Rle_type
--start_ignore
Drop table if exists sto_co_en_rlt;
--end_ignore
Create table sto_co_en_rlt (id SERIAL,
     a1 int ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),a2 char(5) ENCODING (compresstype= rle_type,compresslevel=1,blocksize=32768),a3 text ENCODING (compresstype= rle_type,compresslevel=1,blocksize=8192)) WITH (appendonly=true, orientation=column) distributed randomly;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_co_en_rlt');

Create index sto_en_rlt_idx1 on sto_co_en_rlt(a1);
Create index sto_en_rlt_idx2 on sto_co_en_rlt using bitmap(a3);

Insert into sto_co_en_rlt(a1,a2,a3) values(generate_series(1,10),'val1','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall');
Insert into sto_co_en_rlt(a1,a2,a3) values(generate_series(11,20),'val2','We are pleased to share the September schedule for onsite legal consultations with Duane Morris, our legal counsel for all Pivotal immigration matters.  Representatives will be onsite for one-on-one consultations regarding your individual case');
\d+ sto_co_en_rlt

select count(*) from sto_co_en_rlt;

Update sto_co_en_rlt set a3='Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please be respectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.' where id=3;
 
select count(*) AS only_visi_tups  from sto_co_en_rlt;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_rlt ;

set gp_select_invisible = false;

Delete from sto_co_en_rlt where a1>8 and a1<13;

select count(*) AS only_visi_tups  from sto_co_en_rlt;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_co_en_rlt ;

set gp_select_invisible = false;


VACUUM sto_co_en_rlt;

select count(*) AS only_visi_tups_vacuum  from sto_co_en_rlt;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_co_en_rlt;
set gp_select_invisible = false;

