-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE if EXISTS appendonly_verify_block_checksums_co;
create table appendonly_verify_block_checksums_co (i int)  with (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1);
insert into appendonly_verify_block_checksums_co select i from generate_series(1,100) i;

