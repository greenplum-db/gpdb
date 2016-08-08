CUR_DIR:=`pwd`
EXT:=$(CUR_DIR)/ext


#-------------------------------------------------------------------------------
# DOCS
#
# To get documentation for Python scripts, run the make gen_epydoc target.
#
# Note that the $(CUR_DIR)/sbin/gen_epydocs.sh script assumes that the SRC_DIR
# is set to the top of the Greenplum source tree.
#-------------------------------------------------------------------------------

gen_epydoc:
	$(CUR_DIR)/sbin/gen_epydocs.sh

#-------------------------------------------------------------------------------
# clean
#-------------------------------------------------------------------------------
clean:
	@rm -rf $(CUR_DIR)/epydocs
	@rm -rf $(CURDIR)/tincrepo/epydocs


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

storage_targets: discover_targets non_discover_targets

discover_targets: storage persistent_tables walrep_multinode \
	uao_orca_both_regression upgrade pgtwophase \
	sub_transaction_limit_removal filerep_schematopology_crashrecov

non_discover_targets: aoco_compression filerep_end_to_end fts


# GPDB-O2-TINC-Storage-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-Storage-no-scm/
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

storage: storage_vacuum_xidlimits aocoalter_catalog_loaders \
	storage_queryfinish_and_transactionmanagement \
	storage_persistent_filerep_accessmethods_and_vacuum \
	filerep_end_to_end_xlog_ctlog_cons

storage_vacuum_xidlimits:
	$(TESTER) $(DISCOVER) -t tincrepo \
	-s tincrepo/mpp/gpdb/tests/storage/vacuum \
	-q "class=XidlimitsTests"

aocoalter_catalog_loaders:
	$(TESTER) $(DISCOVER) -t tincrepo \
	-s tincrepo/mpp/gpdb/tests/storage/aoco_alter \
	-s tincrepo/mpp/gpdb/tests/catalog/basic \
	-s tincrepo/mpp/gpdb/tests/catalog/gpcheckcat \
	-s tincrepo/mpp/gpdb/tests/catalog/mpp24606 \
	-s tincrepo/mpp/gpdb/tests/catalog/mpp25160 \
	-s tincrepo/mpp/gpdb/tests/catalog/mpp25256 \
	-s tincrepo/mpp/gpdb/tests/catalog/oid_inconsistency \
	-s tincrepo/mpp/gpdb/tests/catalog/udf_exception_handling \
	-s tincrepo/mpp/gpdb/tests/catalog/upgrade \
	-s tincrepo/mpp/gpdb/tests/storage/loaders

storage_queryfinish_and_transactionmanagement:
	$(TESTER) $(DISCOVER) -t tincrepo \
	-s tincrepo/mpp/gpdb/tests/dispatch/queryfinish \
	-s tincrepo/mpp/gpdb/tests/storage/transaction_management \
	-s tincrepo/mpp/gpdb/tests/storage/basic

storage_persistent_filerep_accessmethods_and_vacuum:
	$(TESTER) $(DISCOVER) -t tincrepo \
	-s tincrepo/mpp/gpdb/tests/storage/persistent \
	-s tincrepo/mpp/gpdb/tests/storage/access_methods/ \
	-s tincrepo/mpp/gpdb/tests/storage/filerep \
	-s tincrepo/mpp/gpdb/tests/storage/vacuum \
	-q "class!=XidlimitsTests" \
	-q "class!=Mpp18816" -q "class!=FilerepMiscTestCase"

filerep_end_to_end_xlog_ctlog_cons:
	$(TESTER) $(DISCOVER) -t tincrepo \
	-s tincrepo/mpp/gpdb/tests/storage/filerep/mpp23631


# GPDB-O2-TINC-Persistent-Tables-no-scm 
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-Persistent-Tables-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

persistent_tables: crash_recovery_part_1 crash_recovery_part_2 \
	crash_recovery_part_3 crash_recovery_part_4 drop_db oom

crash_recovery_part_1:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-q "method=test_postmaster_reset*"

crash_recovery_part_2:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-q "method=test_periodic_failover*"

crash_recovery_part_3:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-q "method=test_crash_system*"

crash_recovery_part_4:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-q "method=test_kill_primaries*ORmethod=test_partition_table_scenario"

drop_db:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables \
	-p test_PT_DropDB.py -p test_PT_freeTID.py

oom:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/persistent_tables -p test_PT_OOM.py


# GPDB-O2-Walrepl-multinode-no-scm 
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-Walrepl-multinode-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

walrep_multinode:
	$(TESTER) $(DISCOVER) -s tincrepo/mpp/gpdb/tests/storage/walrepl


# GPDB-O2-UAO-regression-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-UAO-regression-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

uao_orca_both_regression:
	$(TESTER) $(DISCOVER) -s tincrepo/mpp/gpdb/tests/storage/uao


# GPDB-O2-TINC-aoco-compression-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-aoco-compression-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

aoco_compression: aoco_compression_large_01 aoco_compression_large_02 \
	aoco_compression_large_03 aoco_compression_large_04 \
	aoco_compression_small aoco_compression_small_serial

aoco_compression_large_01:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_01

aoco_compression_large_02:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_02

aoco_compression_large_03:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_03

aoco_compression_large_04:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_large_04

aoco_compression_small:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_small

aoco_compression_small_serial:
	$(TESTER) mpp.gpdb.tests.storage.aoco_compression.test_func_aococompression_small_serial


# GPDB-O2-TINC-Upgrade-no-scm 
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-Upgrade-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

upgrade:
	$(TESTER) $(DISCOVER) -s tincrepo/mpp/gpdb/tests/utilities/upgrade


# GPDB-O2-TINC-Filerep_end_to_end-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-Filerep_end_to_end-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

filerep_end_to_end: filerep_end_to_end_full_primary filerep_end_to_end_incr_primary \
	filerep_end_to_end_full_mirror filerep_end_to_end_incr_mirror

filerep_end_to_end_full_primary:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_full_primary

filerep_end_to_end_incr_primary:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_incr_primary

filerep_end_to_end_full_mirror:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_full_mirror

filerep_end_to_end_incr_mirror:
	$(TESTER) mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_incr_mirror


# GPDB-O2-TINC-PgTwophase-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-PgTwophase-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

pgtwophase: test_pg_twophase_01_10 test_pg_twophase_11_20 \
	test_pg_twophase_21_30 test_pg_twophase_31_40 test_pg_twophase_41_49 \
	test_switch_01_12 test_switch_13_24 test_switch_25_33

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


# GPDB-O2-TINC-FTS-no-scm 
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-FTS-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

fts: fts_transitions_part01 fts_transitions_part02 fts_transitions_part03

fts_transitions_part01:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_01

fts_transitions_part02:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_02

fts_transitions_part03:
	$(TESTER) mpp.gpdb.tests.storage.fts.fts_transitions.test_fts_transitions_03


# GPDB-O2-TINC-Sub-transaction-limit-removal-no-scm 
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-Sub-transaction-limit-removal-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

sub_transaction_limit_removal:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/sub_transaction_limit_removal \
	-p test*.py


# GPDB-O2-TINC-filerep-SchemaTopology-crashrecov-no-scm
# The following targets are pulled from:
# http://pulse-cloud.gopivotal.com/admin/projects/GPDB-O2-TINC-filerep-SchemaTopology-crashrecov-no-scm
#
# Refer to the properties of the pulse project and individual build
# stages for configuration requirements.

filerep_schematopology_crashrecov: crash_recovery_04_10 crash_recovery_11_20 \
	crash_recovery_21_30 crash_recovery_31_42 \
	crash_recovery_filerep_end_to_end crash_recovery_schema_topology

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

crash_recovery_filerep_end_to_end:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_crash_recovery_filerep.py

crash_recovery_schema_topology:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/crashrecovery \
	-p test_crash_recovery_schema_topology.py
