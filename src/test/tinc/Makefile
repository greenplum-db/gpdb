CUR_DIR:=`pwd`
EXT:=$(CUR_DIR)/ext


unit: tincunit
regress: tincregress
check: tinccheck tincmmgrcheck
tinccheck: tincunit tincregress
tincmmgrcheck: tincmmgrunit tincmmgrregress

tincunit:
	@echo unit test tinc
	make -C tinctest unit

tincregress:
	@echo regression test tinc
	make -C tinctest regress

tincmmgrunit:
	@echo unit test tincmmgr
	make -C tincmmgr unit

tincmmgrregress:
	@echo regression test tincmmgr
	make -C tincmmgr regress

TESTER=tinc.py

DISCOVER=discover

# cs-storage
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-storage
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

storage_vacuum_xidlimits:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/vacuum/xidlimits

aocoalter_catalog_loaders:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests \
	-s storage/aoco_alter \
	-s catalog/mpp25256 \
	-s catalog/oid_inconsistency \
	-s catalog/udf_exception_handling \
	-s storage/loaders

storage_uao_and_transactionmanagement:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests \
	-s storage/transaction_management \
	-s storage/basic \
	-s storage/uao

storage_persistent_accessmethods_and_vacuum:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests/storage \
	-s persistent \
	-s access_methods \
	-s vacuum \
	-q "class!=XidlimitsTests"

storage_filerep:
	$(TESTER) $(DISCOVER) \
	-q "class!=Mpp18816" \
	-s tincrepo/mpp/gpdb/tests/storage/filerep

# cs-walrepl-multinode
walrep_1:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests/storage/walrepl \
	-s gpactivatestandby \
	-s gpcrondump \
	-s gpinitstandby \
	-s gpstart \
	-s gpstate \
	-s gpstop \
	-s run

walrep_2:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests/storage/walrepl \
	-s basebackup \
	-s filerep \
	-s oom \
	-s regress \
	-s smart_shutdown \
	-s syncrep \
	-s walreceiver \
	-s walsender \
	-s xact \
	-s crash \
	-q "class!=DDLTestCase" \
	-q "class!=DMLTestCase"


# cs-aoco-compression
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-aoco-compression
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

aoco_compression_large_01:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_01

aoco_compression_large_02:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_02

aoco_compression_small:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_small

aoco_compression_small_serial:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_small_serial

# cs-filerep-end-to-end
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-filerep-end-to-end
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

filerep_end_to_end_full_primary:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_full_primary

filerep_end_to_end_incr_primary:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_incr_primary

filerep_end_to_end_full_mirror:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_full_mirror

filerep_end_to_end_incr_mirror:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_incr_mirror


# cs-pg-two-phase
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-pg-two-phase
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

test_pg_twophase_01_10:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase_01_10.py

test_pg_twophase_11_20:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase_11_20.py

test_pg_twophase_21_30:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase_21_30.py

test_pg_twophase_31_40:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase_31_40.py

test_pg_twophase_41_49:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase_41_49.py

test_switch_01_12:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_switch_01_12.py

test_switch_13_24:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_switch_13_24.py

test_switch_25_33:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_switch_25_33.py


# cs-fts
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-fts
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

fts_transitions_part01:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_01

fts_transitions_part02:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_02

fts_transitions_part03:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_03

# cs-filerep-schema-topology-crashrecov
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/cs-filerep-schema-topology-crashrecov
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

crash_recovery_04_10:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_suspendcheckpoint_crashrecovery_04_to_10.py

crash_recovery_11_20:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_suspendcheckpoint_crashrecovery_11_to_20.py

crash_recovery_21_30:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_suspendcheckpoint_crashrecovery_21_to_30.py

crash_recovery_31_42:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_suspendcheckpoint_crashrecovery_31_to_42.py

crash_recovery_schema_topology:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_crash_recovery_schema_topology.py \
	-p test_reindex_pg_class.py


# Crash recovery test during ptrebuild

persistent_table_rebuild:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-p test_PT_RebuildPT.py

# Optimizer functional tests
optimizer_functional_part1:
	$(TESTER) $(DISCOVER) \
	-s partitioning \
	-s query \
	-q tags=ORCA \
	-q tags!=FEATURE_BRANCH_ONLY \
	-q tags!=dpe

optimizer_functional_part2:
	$(TESTER) $(DISCOVER) \
	-s dml \
	-q tags=ORCA \
	-q tags!=FEATURE_BRANCH_ONLY \
	-q tags!=dpe

runaway_query:
	$(TESTER) \
		resource_management.runaway_query.runaway_query_scenario.test_runaway_query_scenario.RQTScenarioTestCase \
		resource_management.runaway_query.runaway_detector.test_runaway_detector.RunawayDetectorTestCase \
		resource_management.runaway_query.runaway_query_limits.test_runaway_query.RunawayQueryTestCase \
		resource_management.runaway_query.runaway_query_multisession.test_runaway_multisession.RunawayMultiSessionTestCase \
		resource_management.runaway_query.runaway_query_vmem_memoryaccounting.test_runaway_query_vmem_memoryaccounting.RQTMemoryAccountingTestCase \
		resource_management.runaway_query.runaway_query_stress.test_runaway_query_stress.RunawayQueryStressTestCase

memory_accounting:
	$(TESTER) \
		resource_management.memory_accounting.test_oom.OOMTestCase \
		resource_management.memory_accounting.too_many_exec_accounts.test_exec_accounts
