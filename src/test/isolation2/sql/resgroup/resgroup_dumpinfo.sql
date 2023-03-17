DROP ROLE IF EXISTS role_dumpinfo_test;
DROP ROLE IF EXISTS role_permission;
-- start_ignore
DROP RESOURCE GROUP rg_dumpinfo_test;
CREATE LANGUAGE plpython3u;
-- end_ignore

CREATE FUNCTION dump_test_check() RETURNS bool
as $$
import json
import pg

def validate(json_obj, segnum):
   array = json_obj.get("info")
   #validate segnum
   if len(array) != segnum:
      return False
   qd_info = [j for j in array if j["segid"] == -1][0]
   #validate keys
   keys = ["segid", "segmentsOnMaster", "loaded", "groups"]
   for key in keys:
       if key not in qd_info:
           return False

   groups = [g for g in qd_info["groups"] if g["group_id"] > 6441]
   #validate user created group
   if len(groups) != 1:
      return False
   group = groups[0]
   #validate group keys
   keys = ["group_id", "nRunning", "locked_for_drop"]
   for key in keys:
      if key not in group:
         return False

   #validate waitqueue
   wait_queue = group["wait_queue"]
   if wait_queue["wait_queue_size"] != 1:
      return False
   #validate nrunning
   nrunning = group["nRunning"]
   if nrunning != 2:
      return False

   return True

conn = pg.connect(dbname="postgres")

r = conn.query("select count(*) from gp_segment_configuration where  role = 'p';")
n = r.getresult()[0][0]

# The pg_resgroup_get_status_kv() function must output valid result in CTAS
# and simple select queries

r = conn.query("select value from pg_resgroup_get_status_kv('dump');")
json_text =  r.getresult()[0][0]
json_obj = json.loads(json_text)
if not validate(json_obj, n):
   return False

conn.query("""CREATE TEMPORARY TABLE t_pg_resgroup_get_status_kv AS
              SELECT * FROM pg_resgroup_get_status_kv('dump');""")
r = conn.query("SELECT value FROM t_pg_resgroup_get_status_kv;")
json_text =  r.getresult()[0][0]
json_obj = json.loads(json_text)

return validate(json_obj, n)

$$ LANGUAGE plpython3u;

CREATE RESOURCE GROUP rg_dumpinfo_test WITH (concurrency=2, cpu_hard_quota_limit=20);
CREATE ROLE role_dumpinfo_test RESOURCE GROUP rg_dumpinfo_test;

2:SET ROLE role_dumpinfo_test;
2:BEGIN;
3:SET ROLE role_dumpinfo_test;
3:BEGIN;
4:SET ROLE role_dumpinfo_test;
4&:BEGIN;

SELECT dump_test_check();

2:END;
3:END;
4<:
4:END;
2q:
3q:
4q:

CREATE ROLE role_permission;
SET ROLE role_permission;
select value from pg_resgroup_get_status_kv('dump');

RESET ROLE;

-- Now 'dump' is the only value at which the function outputs tuples, but the
-- function must correctly handle any value
SELECT count(*) FROM pg_resgroup_get_status_kv('not_dump');
SELECT count(*) FROM pg_resgroup_get_status_kv(NULL);

DROP ROLE role_dumpinfo_test;
DROP ROLE role_permission;
DROP RESOURCE GROUP rg_dumpinfo_test;
DROP LANGUAGE plpython3u CASCADE;
