-- start_ignore
DROP RESOURCE GROUP rg_spill_test;
-- end_ignore
CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=20, memory_spill_ratio=10);

CREATE OR REPLACE VIEW rg_spill_status AS
	SELECT groupname, memory_shared_quota, proposed_memory_shared_quota, memory_spill_ratio, proposed_memory_spill_ratio
	FROM gp_toolkit.gp_resgroup_config
	WHERE groupname='rg_spill_test';

-- ALTER MEMORY_SPILL_RATIO

SELECT * FROM rg_spill_status;

-- positive
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 20;
SELECT * FROM rg_spill_status;

-- positive, memory_spill_ratio range is [0, 100]
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 0;
SELECT * FROM rg_spill_status;

-- positive: no limit on the sum of shared and spill
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 81;
SELECT * FROM rg_spill_status;

-- negative: memory_spill_ratio is invalid
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 20.0;
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO a;
SELECT * FROM rg_spill_status;

-- negative: memory_spill_ratio is larger than RESGROUP_MAX_MEMORY_SPILL_RATIO
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 101;
SELECT * FROM rg_spill_status;

-- cleanup
DROP VIEW rg_spill_status;
DROP RESOURCE GROUP rg_spill_test;
