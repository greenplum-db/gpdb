-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Insert into delta compressable data types with delta range that is less than 4 bytes

-- start_ignore
Drop table if exists delta_neg_4_byte;
-- end_ignore

Create table delta_neg_4_byte(
    i int, 
    a1 integer, 
    a2 bigint, 
    a3 date, 
    a4 time, 
    a5 timestamp, 
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type); 

-- Inserts such that the values goes to a single segment. this can ensure a constant compression ratio for comparison
Insert into delta_neg_4_byte values 
    (1, 80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (1, 1, 2147483648, '2000-01-01', '14:22:23.776892', '2014-07-30 14:22:23.776892', '2014-07-30 14:22:23.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_neg_4_byte');

Select * from delta_neg_4_byte order by a1;

 

