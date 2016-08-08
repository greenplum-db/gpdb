-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
set gp_appendonly_verify_block_checksums = false;
select count(*) from appendonly_verify_block_checksums_co;
