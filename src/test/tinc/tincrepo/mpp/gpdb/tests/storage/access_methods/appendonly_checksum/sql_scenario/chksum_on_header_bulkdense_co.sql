-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Bulk dense content header
drop table if exists chksum_on_header_bulkdense_co;
create table chksum_on_header_bulkdense_co (a int) with (appendonly=true, orientation=column, compresstype='rle_type', compresslevel=3, checksum=true); 
insert into chksum_on_header_bulkdense_co select i/50 from generate_series(1, 1000000)i;
