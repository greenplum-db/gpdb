CREATE TABLE reindex_dropindex_crtab_part_ao_btree ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, target CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ') with (appendonly=true) DISTRIBUTED BY (id) PARTITION BY RANGE (id) ( PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE, DEFAULT PARTITION de_fault );
insert into reindex_dropindex_crtab_part_ao_btree (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
insert into reindex_dropindex_crtab_part_ao_btree (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
create index on reindex_dropindex_crtab_part_ao_btree(id);
create index on reindex_dropindex_crtab_part_ao_btree(owner);

-- start_ignore
create table before_reindex_dropindex_crtab_part_ao_btree as select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode, relname as c_relname from gp_dist_random('pg_class') where relname like 'reindex_dropindex_crtab_part_ao_btree%_id_idx';
-- end_ignore

select c_relname, 1 as have_same_number_of_rows from before_reindex_dropindex_crtab_part_ao_btree group by c_oid, c_relname having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);

select 1 AS index_exists_on_all_segs from gp_dist_random('pg_class') WHERE relname = 'reindex_dropindex_crtab_part_ao_btree_owner_idx' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

DELETE FROM reindex_dropindex_crtab_part_ao_btree  WHERE id < 128;
1: BEGIN;
1: LOCK reindex_dropindex_crtab_part_ao_btree IN ACCESS EXCLUSIVE MODE;
2&: REINDEX TABLE  reindex_dropindex_crtab_part_ao_btree;
3&: DROP INDEX reindex_dropindex_crtab_part_ao_btree_owner_idx;
1: COMMIT;
2<:
3<:
3: select count(*) from reindex_dropindex_crtab_part_ao_btree where id = 998;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_dropindex_crtab_part_ao_btree where id = 999;

3: select c_relname, 1 as different_relfilenode from before_reindex_dropindex_crtab_part_ao_btree b where exists (select oid, gp_segment_id, relfilenode from gp_dist_random('pg_class') where relname like 'reindex_dropindex_crtab_part_ao_btree%_id_idx' and b.c_oid = oid and b.c_gp_segment_id = gp_segment_id and b.c_relfilenode != relfilenode) group by b.c_oid, b.c_relname;

-- expect only the index on parent table to be dropped; drop index will not drop indexes on child partitions
3: select 1-count(1) as index_dropped from (select * from pg_class union all select * from gp_dist_random('pg_class')) t where t.relname = 'reindex_dropindex_crtab_part_ao_btree_owner_idx';
