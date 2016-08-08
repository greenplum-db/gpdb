-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
DROP TABLE IF EXISTS reindex_crtabforalter_part_heap_btree;

CREATE TABLE reindex_crtabforalter_part_heap_btree (id int, date date, amt decimal(10,2)) 
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE
END (date '2014-01-01') EXCLUSIVE );

Insert into reindex_crtabforalter_part_heap_btree select i, to_date('2013-07-'||i, 'YYYY/MM/DD') , 19.21+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_heap_btree select i, to_date('2013-08-'||i, 'YYYY/MM/DD') , 9.31+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_heap_btree select i, to_date('2013-09-'||i, 'YYYY/MM/DD') , 12.25+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_heap_btree select i, to_date('2013-11-'||i, 'YYYY/MM/DD') , 29.51+i from generate_series(10,30) i;

create index idx_reindex_crtabforalter_part_heap_btree on reindex_crtabforalter_part_heap_btree(id);

SELECT 1 AS relfilenode_same_on_all_segs_maintable from gp_dist_random('pg_class') WHERE relname = 'reindex_crtabforalter_part_heap_btree' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select 1 AS relfilenode_same_on_all_segs_mainidx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtabforalter_part_heap_btree' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select  1 AS relfilenode_same_on_all_segs_partition_1_data from gp_dist_random('pg_class') WHERE relname = 'reindex_crtabforalter_part_heap_btree_1_prt_sales_aug13' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);


select  1 AS relfilenode_same_on_all_segs_partition_1_idx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtabforalter_part_heap_btree_1_prt_sales_aug13' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);


select 1 AS relfilenode_same_on_all_segs_partition_3_data from gp_dist_random('pg_class') WHERE relname = 'reindex_crtabforalter_part_heap_btree_1_prt_sales_sep13' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select 1 AS relfilenode_same_on_all_segs_partition_3_idx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtabforalter_part_heap_btree_1_prt_sales_sep13' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

