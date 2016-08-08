begin;
CREATE INDEX abort_create_needed_cr_co_gist_idx ON abort_create_needed_cr_co_table_gist_index USING GiST (property);
INSERT INTO abort_create_needed_cr_co_table_gist_index (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO abort_create_needed_cr_co_table_gist_index (id, property) VALUES (2, '( (0,0), (2,2) )');
INSERT INTO abort_create_needed_cr_co_table_gist_index (id, property) VALUES (3, '( (0,0), (3,3) )');
INSERT INTO abort_create_needed_cr_co_table_gist_index (id, property) VALUES (4, '( (0,0), (4,4) )');
INSERT INTO abort_create_needed_cr_co_table_gist_index (id, property) VALUES (5, '( (0,0), (5,5) )');
commit;
drop index abort_create_needed_cr_co_gist_idx;
drop table abort_create_needed_cr_co_table_gist_index;
