DROP TABLE IF EXISTS reindex_crtabforadd_part_aoco_btree;

CREATE TABLE reindex_crtabforadd_part_aoco_btree (id int, date date, amt decimal(10,2)) with (appendonly=true, orientation=column)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE
END (date '2014-01-01') EXCLUSIVE );

Insert into reindex_crtabforadd_part_aoco_btree select i, to_date('2013-07-'||i, 'YYYY/MM/DD') , 19.21+i from generate_series(10,30) i;
Insert into reindex_crtabforadd_part_aoco_btree select i, to_date('2013-08-'||i, 'YYYY/MM/DD') , 9.31+i from generate_series(10,30) i;
Insert into reindex_crtabforadd_part_aoco_btree select i, to_date('2013-09-'||i, 'YYYY/MM/DD') , 12.25+i from generate_series(10,30) i;
Insert into reindex_crtabforadd_part_aoco_btree select i, to_date('2013-11-'||i, 'YYYY/MM/DD') , 29.51+i from generate_series(10,30) i;

create index on reindex_crtabforadd_part_aoco_btree(id);

-- start_ignore
create table before_reindex_crtabforadd_part_aoco_btree as select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode, relname as c_relname from gp_dist_random('pg_class') where relname like 'reindex_crtabforadd_part_aoco_btree%_idx';
-- end_ignore

select c_relname, 1 as have_same_number_of_rows from before_reindex_crtabforadd_part_aoco_btree group by c_oid, c_relname having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);
-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
-- @Description Ensures that a reindex table during alter tab add partition  operations is ok
-- 

DELETE FROM reindex_crtabforadd_part_aoco_btree  WHERE id < 12;
1: BEGIN;
1: LOCK reindex_crtabforadd_part_aoco_btree IN ACCESS EXCLUSIVE MODE;
2&: REINDEX TABLE  reindex_crtabforadd_part_aoco_btree;
3&: alter table reindex_crtabforadd_part_aoco_btree add default partition part_others with(appendonly=true, orientation=column);
1: COMMIT;
2<:
3<:
3: Insert into reindex_crtabforadd_part_aoco_btree values(29,'2013-04-22',12.52);
3: select count(*) from reindex_crtabforadd_part_aoco_btree where id = 29;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_crtabforadd_part_aoco_btree where id = 29;

3: select c_relname, 1 as different_relfilenode from before_reindex_crtabforadd_part_aoco_btree b where exists (select oid, gp_segment_id, relfilenode from gp_dist_random('pg_class') where relname like 'reindex_crtabforadd_part_aoco_btree%_idx' and b.c_oid = oid and b.c_gp_segment_id = gp_segment_id and b.c_relfilenode != relfilenode) group by b.c_oid, b.c_relname;

3: select 1 AS oid_same_on_all_segs_partition_new_data from gp_dist_random('pg_class') WHERE relname = 'reindex_crtabforadd_part_aoco_btree_1_prt_part_others' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: select 1 AS oid_same_on_all_segs_partition_new_idx from gp_dist_random('pg_class') WHERE relname = 'reindex_crtabforadd_part_aoco_btree_1_prt_part_others_id_idx' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
