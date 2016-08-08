-- @Description Alter on uao partition tables
-- 

Drop table if exists sto_alt_uao_part_splitpartition;
CREATE TABLE sto_alt_uao_part_splitpartition (id int, date date, amt decimal(10,2)) with (appendonly=true, orientation=column)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE 
END (date '2014-01-01') EXCLUSIVE );

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_splitpartition%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao_part_splitpartition');
\d+ sto_alt_uao_part_splitpartition

Insert into sto_alt_uao_part_splitpartition values(1,'2013-07-05',19.20);
Insert into sto_alt_uao_part_splitpartition values(2,'2013-08-15',10.20);
Insert into sto_alt_uao_part_splitpartition values(3,'2013-09-09',14.20);

select count(*) from sto_alt_uao_part_splitpartition;


-- Alter table add default partition
Alter table sto_alt_uao_part_splitpartition add default partition part_others with(appendonly=true, orientation=column);
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_splitpartition%');

\d+ sto_alt_uao_part_splitpartition

Insert into sto_alt_uao_part_splitpartition values(10,'2013-04-22',12.52);
Insert into sto_alt_uao_part_splitpartition values(11,'2013-02-22',13.53);
Insert into sto_alt_uao_part_splitpartition values(12,'2013-01-22',14.54);

select * from sto_alt_uao_part_splitpartition order by  date,amt;
-- Alter table split default partition

Alter table sto_alt_uao_part_splitpartition split default partition start(date '2013-01-01') end(date '2013-03-01') into (partition p1, partition part_others);
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_splitpartition%');
\d+ sto_alt_uao_part_splitpartition
select * from sto_alt_uao_part_splitpartition order by  date,amt;

Update sto_alt_uao_part_splitpartition set amt =20.5 where date='2013-02-22';

set gp_select_invisible = true;
select *  from sto_alt_uao_part_splitpartition order by  date,amt;
set gp_select_invisible = false;
select *  from sto_alt_uao_part_splitpartition order by  date,amt;

Delete from sto_alt_uao_part_splitpartition where date='2013-01-22';

set gp_select_invisible = true;
select *  from sto_alt_uao_part_splitpartition order by  date,amt;
set gp_select_invisible = false;
select *  from sto_alt_uao_part_splitpartition order by  date,amt;

VACUUM sto_alt_uao_part_splitpartition;

select count(*) AS only_visi_tups_vacuum  from sto_alt_uao_part_splitpartition;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_alt_uao_part_splitpartition;
set gp_select_invisible = false;
