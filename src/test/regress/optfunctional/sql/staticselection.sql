
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema staticselection;
set search_path to staticselection;
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
            result[0] = int(temp_line[temp_line.index('selected:  ')+10:temp_line.index(' (out')])
            result[1] = int(temp_line[temp_line.index('out of')+6:temp_line.index(')')])
return result
$$
language plpythonu;

drop table if exists foo;
create table foo(a int, b int, c int) partition by range (b) (start (1) end (101) every (10));
insert into foo select generate_series(1,5), generate_series(1,100), generate_series(1,10);
analyze foo;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_1.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo;');

select * from foo;

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_2.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b = 35;');

select * from foo where b = 35;

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_3.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b < 35;');

select * from foo where b < 35;

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_4.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b in (5, 6, 14, 23);');

select * from foo where b in (5, 6, 14, 23);

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_5.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b < 15 or b > 60;');

select * from foo where b < 15 or b > 60;

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_6.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b = 150;');

select * from foo where b = 150;

-- ----------------------------------------------------------------------
-- Test: sql/static_selection_7.sql
-- ----------------------------------------------------------------------

-- @author elhela
-- @created 2014-10-14 12:00:00 
-- @modified 2014-10-14 12:00:00
-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)
-- @tags MPP-24709 GPSQL-2879 ORCA HAWQ
-- @product_version gpdb: [4.3.3.0-], hawq: [1.2.2.0-]
-- @optimizer_mode on
-- @gpopt 1.499

select get_selected_parts('explain select * from foo where b = a*5;');

select * from foo where b = a*5;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema staticselection cascade;
-- end_ignore
