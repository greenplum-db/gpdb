.PHONY: check clean unit_tests integration_tests


unit_tests:
	make -C unit check


integration_tests:
	make -C integration check


clean_unit_tests:
	make -C unit clean


clean_integration_tests:
	make -C integration clean


check: unit_tests integration_tests
clean: clean_unit_tests clean_integration_tests