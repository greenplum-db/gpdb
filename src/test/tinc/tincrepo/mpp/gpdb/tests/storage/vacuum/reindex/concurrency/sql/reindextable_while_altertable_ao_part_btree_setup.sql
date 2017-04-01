-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
DROP TABLE IF EXISTS reindex_crtabforalter_part_ao_btree;

CREATE TABLE reindex_crtabforalter_part_ao_btree (id int, date date, amt decimal(10,2)) with (appendonly=true)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE
END (date '2014-01-01') EXCLUSIVE );

Insert into reindex_crtabforalter_part_ao_btree select i, to_date('2013-07-'||i, 'YYYY/MM/DD') , 19.21+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_ao_btree select i, to_date('2013-08-'||i, 'YYYY/MM/DD') , 9.31+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_ao_btree select i, to_date('2013-09-'||i, 'YYYY/MM/DD') , 12.25+i from generate_series(10,30) i;
Insert into reindex_crtabforalter_part_ao_btree select i, to_date('2013-11-'||i, 'YYYY/MM/DD') , 29.51+i from generate_series(10,30) i;

create index idx_reindex_crtabforalter_part_ao_btree on reindex_crtabforalter_part_ao_btree(id);

-- start_ignore
create table before_reindex_crtabforalter_part_ao_btree as select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode, relname as c_relname from gp_dist_random('pg_class') where relname like 'idx_reindex_crtabforalter_part_ao_btree%';
-- end_ignore

select c_relname, 1 as have_same_number_of_rows from before_reindex_crtabforalter_part_ao_btree group by c_oid, c_relname having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);
