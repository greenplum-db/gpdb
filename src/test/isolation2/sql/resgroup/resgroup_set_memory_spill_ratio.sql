-- This query must be the first one in this case.
-- SHOW command will be bypassed in resgroup, when it's the first command
-- in a connection it needs special handling to show memory_spill_ratio
-- correctly.  Verify that it shows the correct value 10 instead of default 20.
SHOW memory_spill_ratio;

--start_ignore
DROP ROLE role1_spill_test;
DROP ROLE role2_spill_test;
DROP RESOURCE GROUP rg1_spill_test;
DROP RESOURCE GROUP rg2_spill_test;
--end_ignore

CREATE RESOURCE GROUP rg1_spill_test WITH
	(CONCURRENCY=10, MEMORY_LIMIT=10, CPU_RATE_LIMIT=10, memory_shared_quota=20, memory_spill_ratio=30);
CREATE RESOURCE GROUP rg2_spill_test WITH
	(CONCURRENCY=10, MEMORY_LIMIT=10, CPU_RATE_LIMIT=10, memory_shared_quota=50, memory_spill_ratio=10);
CREATE ROLE role1_spill_test RESOURCE GROUP rg1_spill_test;
CREATE ROLE role2_spill_test RESOURCE GROUP rg2_spill_test;

-- positive set to resource group level
--start_ignore
SET ROLE role1_spill_test;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;
--end_ignore

-- positive set to session level
SET MEMORY_SPILL_RATIO TO 70;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level use work_mem
SET MEMORY_SPILL_RATIO TO 0;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO 101;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO 90;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO 20;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO '20MB';
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO '20000kB';
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level? can run?
SET MEMORY_SPILL_RATIO TO '20GB';
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level? can run?
SET MEMORY_SPILL_RATIO TO '20  MB  ';
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- negative oom when oprator memory is too large
SET MEMORY_SPILL_RATIO TO '1TB';
SHOW MEMORY_SPILL_RATIO;
SELECT * FROM gp_toolkit.gp_resgroup_config ;
RESET MEMORY_SPILL_RATIO;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO '10 %';
SHOW MEMORY_SPILL_RATIO;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO '10.M';
SHOW MEMORY_SPILL_RATIO;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO '-10M';
SHOW MEMORY_SPILL_RATIO;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO '-10';
SHOW MEMORY_SPILL_RATIO;

-- negative set to session level
SET MEMORY_SPILL_RATIO TO '0x10M';
SHOW MEMORY_SPILL_RATIO;

-- reset to resource group level
RESET MEMORY_SPILL_RATIO;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO 60;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- change role, positive for session level
SET ROLE role2_spill_test;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- positive set to session level
SET MEMORY_SPILL_RATIO TO 20;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- reset to resource group level
RESET MEMORY_SPILL_RATIO;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

-- cleanup
RESET ROLE;
DROP ROLE role1_spill_test;
DROP ROLE role2_spill_test;
DROP RESOURCE GROUP rg1_spill_test;
DROP RESOURCE GROUP rg2_spill_test;
