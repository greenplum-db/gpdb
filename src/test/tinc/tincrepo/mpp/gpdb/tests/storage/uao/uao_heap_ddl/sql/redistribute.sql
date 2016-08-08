-- @Description Tests that reoganizing a table works as expected.
-- 

select * from mytab;
Alter table mytab set with ( reorganize='true') distributed randomly;
select * from mytab;
