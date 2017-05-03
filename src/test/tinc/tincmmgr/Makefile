TINCHOME=$(shell cd .. && pwd)
TESTER=$(TINCHOME)/ext/unittest2/unit2

all:

check: unit regress

unit:
	$(TESTER) discover -s . -p "test_*.py" -v 

regress:
	$(TESTER) discover -s . -p "regress_*.py" -v
