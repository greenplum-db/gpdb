CREATE TABLE reindex_dropindex_crtab_part_heap_btree ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, target CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ')DISTRIBUTED BY (id) PARTITION BY RANGE (id) ( PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE, DEFAULT PARTITION de_fault );
insert into reindex_dropindex_crtab_part_heap_btree (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
insert into reindex_dropindex_crtab_part_heap_btree (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
create index idx_reindex_dropindex_crtab_part_heap_btree on reindex_dropindex_crtab_part_heap_btree(id);
create index idx_reindex_dropindex_crtab_part_owner_heap_btree on reindex_dropindex_crtab_part_heap_btree(owner);

-- start_ignore
create table before_reindex_dropindex_crtab_part_heap_btree as select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode, relname as c_relname from gp_dist_random('pg_class') where relname like 'idx_reindex_dropindex_crtab_part_heap_btree%';
-- end_ignore

select c_relname, 1 as have_same_number_of_rows from before_reindex_dropindex_crtab_part_heap_btree group by c_oid, c_relname having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);

select 1 AS index_exists_on_all_segs from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_dropindex_crtab_part_owner_heap_btree' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
