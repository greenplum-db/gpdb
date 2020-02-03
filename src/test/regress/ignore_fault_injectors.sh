#!/bin/sh

# This scripts scan for the tests that inject faults and comment them in the
# schedule files, so they will not run when gpdb is compiled without fault-injector.

# It's possible now that some tests depend on other tests that use fault-injector.
# But it's hard to tell which one depends, just let the pipeline complains.

set -e
[[ "$enable_faultinjector" == "yes" ]] && exit 0

fault_injection_tests=$(mktemp fault_injection_tests.XXX)

# list the tests that inject faults
grep -ERIli '\s+gp_inject_fault' sql input \
| sed 's,^[^/]*/\(.*\)\.[^.]*$,\1,' \
| sort -u \
> $fault_injection_tests

echo "scanning for flaky fault-injection tests..."

for schedule in *_schedule; do
	while read item
	do
		sed -i "s,^\(test:\s\+$item\s*\)$,#\1," $schedule
	done < $fault_injection_tests

done

rm -f $fault_injection_tests

