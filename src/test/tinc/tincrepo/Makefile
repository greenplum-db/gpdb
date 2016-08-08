TESTER=$(TINCHOME)/ext/unittest2/unit2

all:

check: unit regress

unit:
	$(TESTER) discover -s mpp/lib -p "test_*.py" -v 
	$(TESTER) discover -s mpp/models -p "test_*.py" -v 
	$(TESTER) discover -s mpp/gpdb/lib/models -p "test_*.py" -v 
	#$(TESTER) discover -s mpp/hawq/lib/models -p "test_*.py" -v 

regress:
	$(TESTER) discover -s mpp/models -p "regress_*.py" -v 
	$(TESTER) discover -s mpp/gpdb/lib/models -p "regress_*.py" -v 
	#no modles in hawq currently
	#$(TESTER) discover -s mpp/hawq/lib/models -p "regress_*.py"
