create language plpythonu;

create or replace function get_selected_parts(explain_query text) returns text as
$$
rv = plpy.execute(explain_query)
search_text = 'Partition Selector'
result = []
result.append(0)
result.append(0)
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        j = i+1
        temp_line = rv[j]['QUERY PLAN']
        while temp_line.find('Partitions selected:') == -1:
            j += 1
            if j == len(rv) - 1:
                break
            temp_line = rv[j]['QUERY PLAN']
        
        if temp_line.find('Partitions selected:') != -1:
            result[0] = int(temp_line[temp_line.index('selected: ')+10:temp_line.index(' (out')])
            result[1] = int(temp_line[temp_line.index('out of')+6:temp_line.index(')')])
return result
$$
language plpythonu;

drop table if exists foo;
create table foo(a int, b int, c int) partition by range (b) (start (1) end (101) every (10));
insert into foo select generate_series(1,5), generate_series(1,100), generate_series(1,10);
analyze foo;
