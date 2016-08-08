-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Insert into delta compressable data types with delta range that is more than 4 bytes

-- start_ignore
Drop table if exists delta_more_4_byte;
-- end_ignore

Create table delta_more_4_byte(
    a1 integer, 
    a2 bigint, 
    a3 date, 
    a4 time, 
    a5 timestamp, 
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type); 

-- Inserts such that the values goes to a single segment. this can ensure a constant compression ratio for comparison
Insert into delta_more_4_byte values 
    (1, 2147483648, '2014-07-29', '14:22:23.776892', '2014-07-30 10:22:30', '2014-07-30 14:22:23.776892-07'),
    ( 800000000, 2743322399, '2014-07-30', '14:46:23.776899', '2014-07-30 10:24:30', '2014-07-30 14:46:23.776899-07');

Select get_ao_compression_ratio('delta_more_4_byte');

Select * from delta_more_4_byte order by a1;

 

