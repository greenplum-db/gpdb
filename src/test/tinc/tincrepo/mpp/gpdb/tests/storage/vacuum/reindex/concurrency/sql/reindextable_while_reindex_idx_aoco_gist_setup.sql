-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_crtab_aoco_gist;

CREATE TABLE reindex_crtab_aoco_gist (id INTEGER, box1 BOX) WITH (appendonly=true, orientation=column);
insert into reindex_crtab_aoco_gist (id, box1) VALUES (300, '( (1, 1), (2, 2) )');
insert into reindex_crtab_aoco_gist (id, box1) VALUES (301, '( (3, 4), (4, 5) )');
insert into reindex_crtab_aoco_gist (id, box1) VALUES (304, '( (2, 2), (4, 4) )');
create index idx_reindex_crtab_aoco_gist on reindex_crtab_aoco_gist USING Gist(box1);
