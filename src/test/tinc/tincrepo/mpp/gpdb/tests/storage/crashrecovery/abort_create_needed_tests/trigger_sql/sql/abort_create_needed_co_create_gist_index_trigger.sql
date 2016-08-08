begin;
CREATE INDEX abort_create_needed_cr_co_gist_idx ON abort_create_needed_cr_co_table_gist_index USING GiST (property);
drop index abort_create_needed_cr_co_gist_idx;
commit;
