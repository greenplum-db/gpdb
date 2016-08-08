-- @Description Truncate on uao partition tables
-- 

Drop table if exists sto_alt_uao_part_droptable;
CREATE TABLE sto_alt_uao_part_droptable (id int, date date, amt decimal(10,2)) with (appendonly=true)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE 
END (date '2014-01-01') EXCLUSIVE );

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_droptable%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao_part_droptable');
\d+ sto_alt_uao_part_droptable

Insert into sto_alt_uao_part_droptable values(1,'2013-07-05',19.20);
Insert into sto_alt_uao_part_droptable values(2,'2013-08-15',10.20);
Insert into sto_alt_uao_part_droptable values(3,'2013-09-09',14.20);
Insert into sto_alt_uao_part_droptable values(10,'2013-07-22',12.52);
select count(*) AS only_visi_tups  from sto_alt_uao_part_droptable;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_alt_uao_part_droptable;
set gp_select_invisible = false;
drop table sto_alt_uao_part_droptable;
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_droptable%');
