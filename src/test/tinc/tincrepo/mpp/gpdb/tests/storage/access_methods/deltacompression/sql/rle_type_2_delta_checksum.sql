-- @product_version gpdb: [4.3.4.0-]
-- Insert into columns with rle_type compresslevel 2 + delta.

-- start_ignore
Drop table if exists rle_type_2_delta_chkt;
-- end_ignore

Create table rle_type_2_delta_chkt(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=2, checksum=true);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_2_delta_chkt'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_2_delta_chkt


Insert into rle_type_2_delta_chkt values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_2_delta_chkt');

Select * from rle_type_2_delta_chkt order by a1;

-- start_ignore
Drop table if exists rle_type_2_delta_chkf;
-- end_ignore

Create table rle_type_2_delta_chkf(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=2, checksum=false);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_2_delta_chkf'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_2_delta_chkf


Insert into rle_type_2_delta_chkf values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_2_delta_chkf');

Select * from rle_type_2_delta_chkf order by a1;

