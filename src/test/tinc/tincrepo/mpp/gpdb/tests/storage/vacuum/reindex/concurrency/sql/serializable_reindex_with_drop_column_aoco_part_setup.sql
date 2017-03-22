-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
DROP TABLE IF EXISTS reindex_serialize_tab_aoco_part;

CREATE TABLE reindex_serialize_tab_aoco_part ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, target CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ') with (appendonly = true, orientation = column) DISTRIBUTED BY (id) PARTITION BY RANGE (id) ( PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE, DEFAULT PARTITION de_fault );
insert into reindex_serialize_tab_aoco_part (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
insert into reindex_serialize_tab_aoco_part (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;

create index idxa_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part(id);
create index idxb_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part(owner);
create index idxc_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part USING GIST(property);
create index idxd_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part USING GIST(poli);
create index idxe_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part USING GIST(target);
create index idxf_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part USING BITMAP(v);
create index idxh_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part USING GIST(c);
create index idxi_reindex_serialize_tab_aoco_part on reindex_serialize_tab_aoco_part(f);
