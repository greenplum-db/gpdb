-- @Description Alter on uao partition tables
-- 

Drop table if exists sto_alt_uao_part_addpartition;
CREATE TABLE sto_alt_uao_part_addpartition (id int, date date, amt decimal(10,2)) with (appendonly=true)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE 
END (date '2014-01-01') EXCLUSIVE );

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_addpartition%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao_part_addpartition');
\d+ sto_alt_uao_part_addpartition

Insert into sto_alt_uao_part_addpartition values(1,'2013-07-05',19.20);
Insert into sto_alt_uao_part_addpartition values(2,'2013-08-15',10.20);
Insert into sto_alt_uao_part_addpartition values(3,'2013-09-09',14.20);

select count(*) from sto_alt_uao_part_addpartition;


-- Alter table add heap partition
Alter table sto_alt_uao_part_addpartition add partition new_p START (date '2013-06-01') INCLUSIVE ;
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_addpartition%');

\d+ sto_alt_uao_part_addpartition

Insert into sto_alt_uao_part_addpartition values(5,'2013-06-09',14.20);
select * from sto_alt_uao_part_addpartition order by date;

-- Alter table add ao partition
Alter table sto_alt_uao_part_addpartition add partition p1 START (date '2013-05-01') INCLUSIVE with(appendonly=true);
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_addpartition%');

\d+ sto_alt_uao_part_addpartition

Insert into sto_alt_uao_part_addpartition values(9,'2013-05-22',14.22);
select * from sto_alt_uao_part_addpartition order by date;

-- Alter table add default partition
Alter table sto_alt_uao_part_addpartition add default partition part_others with(appendonly=true);
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part_addpartition%');

\d+ sto_alt_uao_part_addpartition

Insert into sto_alt_uao_part_addpartition values(10,'2013-04-22',12.52);
select * from sto_alt_uao_part_addpartition order by  date;


