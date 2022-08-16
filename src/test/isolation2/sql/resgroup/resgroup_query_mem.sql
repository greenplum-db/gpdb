-- This test is to verify that query_mem is set correctly in QEs.
-- Previously, resgroup does not consider that different number of
-- segments among coordinator and segments. Now we let QEs to re-calculate
-- query_mem in each segment locally. This test case use the following
-- steps to verify the new method's correctness:
-- 1. fetch available memory in coordinator and a single segment,
--    compute the ratio
-- 2. use fault inject and plpython invokes pygresql with notice,
--    get a distributed plan's sort's operator memory in a QE
-- 3. Get sort's operator memory in a pure QD's plan (catalog order by)
-- 4. compute the ratio of two operator memorys
-- 5. these two ratios should be the same.

create extension if not exists gp_inject_fault;
create or replace language plpython3u;

create table t_qmem(a int);
select gp_inject_fault('rg_qmem_qd_qe', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = 0;

create function rg_qmem_test() returns boolean as $$
from pg import DB
from copy import deepcopy
import re

# 1: get resgroup available mem in QD and QE and compute ratio
sql = ("select memory_available m from "
       "gp_toolkit.gp_resgroup_status_per_segment "
       "where segment_id = %d and rsgname = 'admin_group'")
qd_mem = int(plpy.execute(sql % -1)[0]["m"])
qe_mem = int(plpy.execute(sql % 0)[0]["m"])
ratio1 = int(round(float(qd_mem) / qe_mem))

# 2. use notice to get qe operator mem
dbname = plpy.execute("select current_database() db")[0]["db"]
db = DB(dbname=dbname)
qe_opmem_info = []
db.set_notice_receiver(lambda n: qe_opmem_info.append(deepcopy(n.message)))
sql = "select * from t_qmem order by 1"
db.query(sql)
qe_opmem = int(re.findall(r"op_mem=(\d+)", qe_opmem_info[0])[0])
db.set_notice_receiver(None)

# 3. get qd operator mem
sql = "explain analyze select * from pg_class order by relpages limit 10"
db.query("set gp_resgroup_print_operator_memory_limits = on;")
r = db.query(sql).getresult()
for (line, ) in r:
    if "->  Sort" not in line: continue
    qd_opmem = int(re.findall(r"operatorMem: (\d+)", line)[0])
    break

db.close()

ratio2 = int(round(float(qd_opmem) / qe_opmem))

return ratio1 == ratio2

$$ language plpython3u;

select rg_qmem_test();
select gp_inject_fault('rg_qmem_qd_qe', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;
drop function rg_qmem_test();
drop table t_qmem;
