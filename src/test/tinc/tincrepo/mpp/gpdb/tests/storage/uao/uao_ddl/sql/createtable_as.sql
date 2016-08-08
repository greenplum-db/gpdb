-- @Description Tests creating a table with CREATE TABLE AS
-- 
select * from mytab;
CREATE TABLE mytab2  WITH (appendonly=true) AS SELECT * FROM mytab DISTRIBUTED RANDOMLY;
select * from mytab;
select * from mytab2;
