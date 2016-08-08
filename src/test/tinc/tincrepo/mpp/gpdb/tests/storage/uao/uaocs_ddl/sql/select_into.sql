-- @Description Tests that SELECT INTO works
-- 

select * from mytab;
SELECT * INTO mytab2 FROM mytab;
select * from mytab;
select * from mytab2;
