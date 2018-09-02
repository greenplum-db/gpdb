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
	$(TESTER) $(DISCOVER) -t tincrepo/mpp/gpdb/tests \
	-s dispatch/interconnect
