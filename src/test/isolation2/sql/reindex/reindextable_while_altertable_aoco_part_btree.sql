DROP TABLE IF EXISTS reindex_crtabforalter_part_aoco_btree;

CREATE TABLE reindex_crtabforalter_part_aoco_btree (id int, date date, amt decimal(10,2)) with (appendonly=true, orientation=column)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE
END (date '2014-01-01') EXCLUSIVE );

Insert into reindex_crtabforalter_part_aoco_btree select i, to_date('2013-07-'||i, 'YYYY/MM/DD') , 19.21+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_aoco_btree select i, to_date('2013-08-'||i, 'YYYY/MM/DD') , 9.31+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_aoco_btree select i, to_date('2013-09-'||i, 'YYYY/MM/DD') , 12.25+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_aoco_btree select i, to_date('2013-11-'||i, 'YYYY/MM/DD') , 29.51+i from generate_series(10,30) i;

create index on reindex_crtabforalter_part_aoco_btree(id);

-- start_ignore
create table before_reindex_crtabforalter_part_aoco_btree as select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode, relname as c_relname from gp_dist_random('pg_class') where relname like 'reindex_crtabforalter_part_aoco_btree%_idx';
-- end_ignore

select c_relname, 1 as have_same_number_of_rows from before_reindex_crtabforalter_part_aoco_btree group by c_oid, c_relname having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);
-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
-- @Description Ensures that a reindex table during alter tab drop col operations is ok
-- 

DELETE FROM reindex_crtabforalter_part_aoco_btree  WHERE id < 12;
1: BEGIN;
1: LOCK reindex_crtabforalter_part_aoco_btree IN ACCESS EXCLUSIVE MODE;
2&: REINDEX TABLE  reindex_crtabforalter_part_aoco_btree;
3&: alter table reindex_crtabforalter_part_aoco_btree drop column amt;
1: COMMIT;
2<:
3<:
3: select count(*) from reindex_crtabforalter_part_aoco_btree where id = 29;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_crtabforalter_part_aoco_btree where id = 29;

3: select c_relname, 1 as different_relfilenode from before_reindex_crtabforalter_part_aoco_btree b where exists (select oid, gp_segment_id, relfilenode from gp_dist_random('pg_class') where relname like 'reindex_crtabforalter_part_aoco_btree%_idx' and b.c_oid = oid and b.c_gp_segment_id = gp_segment_id and b.c_relfilenode != relfilenode) group by b.c_oid, b.c_relname;

3: select count(*) from pg_attribute where attname = 'amt' and attrelid = (select oid from pg_class where relname = 'reindex_crtabforalter_part_aoco_btree');
