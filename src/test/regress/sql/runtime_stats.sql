--
-- Test runtime_stats 
--
-- Examines if run-time statistics are correct.
--

-- Returns 1 if workfiles are spilled during query execution; 0 otherwise.
create or replace function isSpilling(explain_query text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = 'Workfile: ('
result = 0
for i in range(len(rv)):
   cur_line = rv[i]['QUERY PLAN']
   if search_text.lower() in cur_line.lower():
      str_trim = cur_line.split('Workfile: (')
      str_trim = str_trim[1].split(' spilling')
      if int(str_trim[0]) > 0:
         result = 1
return result
$$
language plpython3u;


-- Force Sort operator to spill workfiles and examine if stats are correct
drop table if exists testsort; 
create table testsort (i1 int, i2 int, i3 int, i4 int); 
insert into testsort select i, i % 1000, i % 100000, i % 75 from generate_series(0,199999) i; 

set statement_mem="2MB";
set gp_resqueue_print_operator_memory_limits=on;

select isSpilling('explain (analyze, verbose) select i1,i2 from testsort order by i2');
