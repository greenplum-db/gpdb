-- Given a partitioned table with data
create table partitioned_table_with_data(a numeric, b date, c date)
       distributed by(a)
       partition by range(b) (
       		 start('2012-01-01')
		 end('2012-03-01')
		 every(interval '1 month')
       );

insert into partitioned_table_with_data values(2, '2012-01-01', '2012-01-01');

-- when selecting a small select list
-- and using more columns in the where clause
-- then the executed plan should succeed.
select a from partitioned_table_with_data where b>='2012-01-01' and c=b;
