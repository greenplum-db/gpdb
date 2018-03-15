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

TESTER= \
	export PGDATABASE=gptest && \
	export TINCHOME=$(PWD) && \
	export TINCREPOHOME=$(PWD)/tincrepo && \
	export PYTHONPATH=$(PWD):$(PWD)/ext:$(PYTHONPATH):$(PWD)/tincrepo && \
	export PATH=$(PWD):$(PWD)/ext:$(PWD)/ext/unittest2:$(PWD)/sbin/:$(PATH) && \
	tinc.py

DISCOVER=discover

# cs-walrepl-multinode
walrep_1:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests/storage/walrepl \
	-s gpactivatestandby \
	-s gpinitstandby \
	-s gpstart \
	-s gpstate \
	-s gpstop \
	-s run

walrep_2:
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests/storage/walrepl \
	-s basebackup \
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

test_pg_twophase:
	$(TESTER) $(DISCOVER) \
	-s tincrepo/mpp/gpdb/tests/storage/pg_twophase \
	-p test_pg_twophase.py \
	-p test_switch.py

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

# gpexpand test
gpexpand_1:
	$(TESTER) $(DISCOVER) \
	-s mpp/gpdb/tests/utilities/gpexpand \
	-q tags=part1

gpexpand_2:
	$(TESTER) $(DISCOVER) \
	-s mpp/gpdb/tests/utilities/gpexpand \
	-q tags=part2

# mpp interconnect test
mpp_interconnect:
	make -C tincrepo/mpp/gpdb/tests/dispatch/interconnect/ickm all distribute
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests \
	-s dispatch/interconnect
