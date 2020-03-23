#!/bin/sh

# This scripts scan for the tests that inject faults and are put into parallel
# testing groups.  These tests are flaky as the injected faults can be
# triggered by the tests in the same testing groups.  To make the testing
# results deterministic each of these tests must be put in a separate group.

set -e

fault_injection_tests=$(mktemp fault_injection_tests.XXX)
parallel_tests=$(mktemp parallel_tests.XXX)
retcode=0

# list the tests that inject faults
grep -ERIli '(select|perform)\s+gp_inject_fault' sql input \
| sed 's,^[^/]*/\(.*\)\.[^.]*$,\1,' \
| sort -u \
> $fault_injection_tests

echo "scanning for flaky fault-injection tests..."

for schedule in *_schedule; do
	# list the tests that are in parallel testing groups
	grep -E '^test:(\s+\S+){2,}' $schedule \
	| cut -d' ' -f2- \
	| tr ' ' '\n' \
	| sort -u \
	> $parallel_tests

	# find out the tests that are in both lists
	tests=$(comm -12 $fault_injection_tests $parallel_tests)
	if [ -n "$tests" ]; then
		echo "- $schedule:" $tests
		retcode=1
	fi
done

rm -f $fault_injection_tests $parallel_tests

if [ $retcode = 0 ]; then
	echo "done"
else
	cat <<EOF

ERROR: above tests are flaky as they inject faults and are put in parallel testing groups.
HINT: move each of them to a separate testing group.

EOF
fi

exit $retcode
