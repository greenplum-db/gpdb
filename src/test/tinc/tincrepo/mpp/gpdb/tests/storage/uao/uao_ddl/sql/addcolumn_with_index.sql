-- @Description Tests that it is possible to SELECT from a AO table after a column as been
-- added. Here the AO table has an index.
-- 
select * from mytab;
alter table mytab ADD COLUMN address varchar(30) default 'none';
select * from mytab;
