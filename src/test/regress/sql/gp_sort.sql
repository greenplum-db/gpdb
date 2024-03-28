-- GPDB used to define Datum as a signed type. That caused below
-- soring issues.
create table mac_tbl (id int, mac macaddr) distributed by (id);
insert into mac_tbl values
(0, '00:00:00:00:00:00'),
(0, 'ff:ff:ff:ff:ff:ff'),
(0, '11:11:11:11:11:11');
select * from mac_tbl order by mac;
create table uuid_tbl (id int, uid uuid) distributed by (id);
insert into uuid_tbl values
(0, '00000000000000000000000000000000'),
(0, 'ffffffffffffffffffffffffffffffff'),
(0, '11111111111111111111111111111111');
select * from uuid_tbl order by uid;

-- Test cases for GP's specical mksort
-- start_ignore
create language plpython3u;
-- end_ignore

-- compare the results between mksort and qsort
create or replace function gp_mksort_test() returns int as $$
# the compare utility function
def compare_results(query):
    plpy.execute("set gp_enable_mk_sort to on;")
    mksort_res = plpy.execute(query)
    plpy.execute("set gp_enable_mk_sort to off;")
    #plpy.info(plpy.execute("show gp_enable_mk_sort;")[0]["gp_enable_mk_sort"])
    qsort_res = plpy.execute(query)
    res1 = mksort_res
    res2 = qsort_res
    if res1.nrows() != res2.nrows():
        plpy.error("gp_mksort_test: query count failed")

    for i in range(res1.nrows()):
        if  res1[i]['a'] != res2[i]['a'] or \
            res1[i]['b'] != res2[i]['b'] or \
            res1[i]['c'] != res2[i]['c']:
            plpy.error("gp_mksort_test: query results failed")

# basic test: 1~13 rows
for i in range(13):
    plpy.execute("truncate gp_mksort_test_table;")
    insert = "insert into gp_mksort_test_table \
        select floor(random()*10), floor(random()*100), left(md5(g::text),4) \
        from generate_series(1,%d) g;" % (i+1)
    plpy.execute(insert)
    compare_results("select * from gp_mksort_test_table order by a,c")
plpy.info('gp_mksort_test: basic test passed')

# random test: run 10 times (10w rows)
for i in range(10):
    plpy.execute("truncate gp_mksort_test_table;")
    insert = "insert into gp_mksort_test_table \
        select floor(random()*10), floor(random()*100), left(md5(g::text),4) \
        from generate_series(1,100000) g;"
    plpy.execute(insert)
    compare_results("select * from gp_mksort_test_table order by a,b,c")
plpy.info('gp_mksort_test: random test passed')

return 0
$$ language plpython3u;

create table gp_mksort_test_table(a int, b int, c varchar);
set statement_mem='100MB';
select gp_mksort_test();

drop table gp_mksort_test_table;
drop function gp_mksort_test();
