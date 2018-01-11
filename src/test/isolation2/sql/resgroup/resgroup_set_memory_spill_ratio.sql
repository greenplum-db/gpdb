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

-- positive set to session level
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
