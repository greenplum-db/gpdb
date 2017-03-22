-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_ao_gist;

CREATE TABLE reindex_ao_gist (
 id INTEGER,
 owner VARCHAR,
 description VARCHAR,
 property BOX, 
 poli POLYGON,
 target CIRCLE,
 v VARCHAR,
 t TEXT,
 f FLOAT, 
 p POINT,
 c CIRCLE,
 filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ' 
 ) with (appendonly=true) 
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (id)
  (
  PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE,
  DEFAULT PARTITION de_fault
  );

insert into reindex_ao_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;
insert into reindex_ao_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;
insert into reindex_ao_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;
insert into reindex_ao_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;
insert into reindex_ao_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;

create index idx_gist_reindex_vacuum_ao on reindex_ao_gist USING Gist(target);
