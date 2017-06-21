-- Memory consumption of operators 

-- start_ignore
create schema memconsumption;
set search_path to memconsumption;
-- end_ignore

create table test (i int, j int);

set explain_memory_verbosity=detail;
set execute_pruned_plan=on;

insert into test select i, i % 100 from generate_series(1,1000) as i;

-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function sum_alien_consumption(query text) returns int as
$$
import re
rv = plpy.execute('EXPLAIN ANALYZE '+ query)
search_text = 'X_Alien'
total_consumption = 0
count = 0
comp_regex = re.compile("[^0-9]+(\d+)\/(\d+).+")
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        print search_text
        m = comp_regex.match(cur_line)
        if m is not None:
            count = count + 1
            total_consumption = total_consumption + int(m.group(2))
return total_consumption
$$
language plpythonu;

select sum_alien_consumption('SELECT t1.i, t2.j FROM test as t1 join test as t2 on t1.i = t2.j') = 0;

set execute_pruned_plan=off;
select sum_alien_consumption('SELECT t1.i, t2.j FROM test as t1 join test as t2 on t1.i = t2.j') > 0;
