-- @author xiongg1 
-- @created 2015-01-20 12:00:00
-- @modified 2015-01-20 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3.5.0-]
-- @description Tests for MPP-24621

select to_date('-4713-11-23', 'yyyy-mm-dd');
select to_date('-4713-11-24', 'yyyy-mm-dd');
select to_date('5874897-12-31', 'yyyy-mm-dd');
select to_date('5874898-01-01', 'yyyy-mm-dd');

-- start_ignore
drop table if exists t_to_date;
create table t_to_date(c1 date);
-- end_ignore
insert into t_to_date values (to_date('-4713-11-24', 'yyyy-mm-dd'));
insert into t_to_date values (to_date('5874897-12-31', 'yyyy-mm-dd'));
select * from t_to_date;
