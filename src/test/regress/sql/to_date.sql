-- -----------------------------------------------------------------
-- Test to_date() and to_date_valid()
-- -----------------------------------------------------------------
show datestyle;
-- test to_date() first
-- also make sure that it still returns invalid results 
select to_date('2016/01/15', 'YYYY/MM/DD');
select to_date('2016/01/31', 'YYYY/MM/DD');
select to_date('2016/02/31', 'YYYY/MM/DD');
select to_date('2016-01-15', 'YYYY-MM-DD');
select to_date('2016-02-31', 'YYYY-MM-DD');
select to_date('15-01-2016', 'DD-MM-YYYY');
select to_date('31-02-2016', 'DD-MM-YYYY');
select to_date('2016/02/29', 'YYYY/MM/DD');

-- test to_date_valid()
select to_date_valid('2016/01/15', 'YYYY/MM/DD');
select to_date_valid('2016/01/31', 'YYYY/MM/DD');
select to_date_valid('2016/02/31', 'YYYY/MM/DD');
select to_date_valid('2016-01-15', 'YYYY-MM-DD');
select to_date_valid('2016-02-31', 'YYYY-MM-DD');
select to_date_valid('15-01-2016', 'DD-MM-YYYY');
select to_date_valid('31-02-2016', 'DD-MM-YYYY');
select to_date_valid('2016/02/29', 'YYYY/MM/DD');
