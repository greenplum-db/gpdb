#!/bin/sh

# This scripts scan for the tests that inject faults and comment them in the
# schedule files, so they will not run when gpdb is compiled without fault-injector.

# It's possible now that some tests depend on other tests that use fault-injector.
# But it's hard to tell which one depends, just let the pipeline complains.

set -e
[ "$enable_faultinjector" = "yes" ] && exit 0

fault_injection_tests=$(mktemp fault_injection_tests.XXX)
tempfile=$(mktemp temp_fault_inject.XXXX)
row_col_tests=$(mktemp row_col_tests.XXXX)

grep -ERIl '@orientation@' input \
| sed 's,^[^/]*/\(.*\)\.[^.]*$,\1,' \
| sort -u \
> $row_col_tests

# list the tests that inject faults
grep -ERIli '(\s+gp_inject_fault)|force_mirrors_to_catch_up' sql input \
| sed 's,^[^/]*/\(.*\)\.[^.]*$,\1,' \
| sort -u \
> $fault_injection_tests

# list the tests that inject faults
echo "comment out tests that use fault-injection ..."
comm -23 $fault_injection_tests $row_col_tests > $tempfile
comm -12 $fault_injection_tests $row_col_tests \
| sed 's,^\(.*\)$,\1_row \1_column,' \
| tr ' ' '\n' \
>> $tempfile
sort -u $tempfile > $fault_injection_tests

for schedule in *_schedule; do
	while read item
	do
		perl -i -p -e "s,^(test:\s+$item\s*)$,#\1," $schedule
	done < $fault_injection_tests
done

rm -f $fault_injection_tests $tempfile $row_col_tests

