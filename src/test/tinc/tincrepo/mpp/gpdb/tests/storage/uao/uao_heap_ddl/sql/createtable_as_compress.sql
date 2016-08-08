-- @Description Tests creating a table with CREATE TABLE AS.
-- Here the original table is compressed.
-- 

select * from mytab;
CREATE TABLE mytab2  WITH (appendonly=true, compresstype = zlib, compresslevel=3) AS SELECT * FROM mytab;
select * from mytab;
select * from mytab2;
