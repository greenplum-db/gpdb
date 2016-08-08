drop table if exists errcopy, errcopy_err, errcopy_temp;

create table errcopy(a int, b int, c text) distributed by (a);
insert into errcopy select i, i, case when i <> 5 then i end || '_text' from generate_series(1, 10)i;
copy errcopy to '/tmp/errcopy.csv' csv null '';

-- check if not null constraint not affect error log.
truncate errcopy;
alter table errcopy alter c set not null;

copy errcopy from '/tmp/errcopy.csv' csv null '' log errors segment reject limit 10 rows;

select * from errcopy;

-- reject rows with invalid format for int / TODO: dropped column name
alter table errcopy alter c drop not null;
alter table errcopy drop column c;
alter table errcopy add column c int;

copy errcopy from '/tmp/errcopy.csv' csv null '' log errors segment reject limit 10 rows;
select * from errcopy;
select relname, linenum, errmsg, rawdata from gp_read_error_log('errcopy') order by linenum;

-- reject one row with extra column, one row with fewer columns
truncate errcopy;
select gp_truncate_error_log('errcopy');

copy (select i::text || ',' || i::text || case when i = 4 then '' else ',' || i::text || case when i = 5 then ',5' else '' end end from generate_series(1, 10)i) to '/tmp/errcopy.csv';
copy errcopy from '/tmp/errcopy.csv' csv null '' log errors segment reject limit 10 rows;

select * from errcopy order by a;
select relname, linenum, errmsg, rawdata from gp_read_error_log('errcopy') order by linenum;

-- metacharacter
truncate errcopy;
copy errcopy from stdin csv newline 'LF' log errors segment reject limit 3 rows;
1,2,0
1,3,4^M
1,3,3
\.

select * from errcopy;

-- exceed reject limit
truncate errcopy;
select gp_truncate_error_log('errcopy');

copy errcopy from stdin delimiter '\t' log errors segment reject limit 3 rows;
1       2       0
1       3       4
1       4
1       2
1
1       3       0
1       30      999
\.
select * from errcopy;
select relname, filename, linenum, bytenum, errmsg from gp_read_error_log('errcopy') order by linenum;

-- abort and keep
truncate errcopy;
select gp_truncate_error_log('errcopy');

copy errcopy from stdin delimiter '/' log errors segment reject limit 3 rows;
1/2/3
1/5
7/8/9
1/11/12/
1
1/17/18
\.
select relname, filename, linenum, bytenum, errmsg from gp_read_error_log('errcopy');

