-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Insert into delta compressable data types with delta range that is less than 3 bytes

-- start_ignore
Drop table if exists delta_3_byte;
-- end_ignore

Create table delta_3_byte(
    i int, 
    a1 integer, 
    a2 bigint, 
    a3 date, 
    a4 time, 
    a5 timestamp, 
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type); 

-- Inserts such that the values goes to a single segment. this can ensure a constant compression ratio for comparison
Insert into delta_3_byte values 
    (1, 1, 2147483648, '2000-01-01', '14:22:23.776892', '2014-07-30 10:22:30', '2014-07-30 14:22:23.776892-07'),
    (1, 800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_3_byte');

Select * from delta_3_byte order by a1;

 

