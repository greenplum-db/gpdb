-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Negative cases- Checksum not allowed in encoding

\c dsp_db1

SET gp_default_storage_options= "appendonly=true, orientation=column";
show gp_default_storage_options;
-- In storage_directive
Drop table if exists ch_ng_t1;
Create table ch_ng_t1 ( i int, j int encoding(compresstype=quicklz, checksum=true));

-- In column encoding
Create table ch_ng_t1 ( i int, j int, column i  encoding(compresstype=quicklz, checksum=true));

-- In default column encoding

Create table ch_ng_t1 ( i int, j int, default column  encoding(compresstype=quicklz, checksum=true));


-- Partition level
Create table ch_ng_t1 ( i int, j int) partition by range(j) (partition p1 start(1) end(5), partition p2 start(6) end(10), column j encoding(compresstype=quicklz, checksum=true));

-- Sub partition level
Create table ch_ng_t1 ( i int, j int) partition by range(j) subpartition by list(i) 
    subpartition template (subpartition sp1 values(1,2,3,4), column i encoding(compresstype=quicklz, checksum=true)) (start(1) end(10) every(2));

