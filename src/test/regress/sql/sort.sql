create schema sort_schema;
set search_path to sort_schema;
 
-- start_ignore
create language plpythonu;
-- end_ignore
 
-- Check if analyze output has Sort Method
 create or replace function sort_schema.has_sortmethod(explain_analyze_query text)
 returns setof int as
 $$
 rv = plpy.execute(explain_analyze_query)
 search_text = 'Sort Method'
 result = []
 for i in range(len(rv)):
     cur_line = rv[i]['QUERY PLAN']
     if search_text.lower() in cur_line.lower():
         result.append(1)
 return result
 $$
 language plpythonu;
 
 set gp_enable_mk_sort = on;
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g limit 100;');
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g;');
 
 set gp_enable_mk_sort = off;
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g limit 100;');
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g;');
 
 -- start_ignore
 create table sort_a(i int, j int);
 insert into sort_a values(1, 2);
 -- end_ignore
 
 set gp_enable_mk_sort = on;
 select sort_schema.has_sortmethod('explain analyze select i from sort_a order by i;');
 
 set gp_enable_mk_sort = off;
 select sort_schema.has_sortmethod('explain analyze select i from sort_a order by i;');
 
 
 -- start_ignore
 drop schema sort_schema cascade;
 -- end_ignore