#! /usr/bin/env python3

'''
Parse the results of the performance test into a CSV for loading into
a results table or spreadsheet.
'''
import sys
import re

results_file = sys.argv[1]
num_copies = sys.argv[2]

## Get number of rows
num_rows = 100 * int(num_copies)

## Open the perf results file
with open(results_file, 'r') as perf_results:
    perf_results_lines = perf_results.readlines()

## Trim the results until we get to the actual tests
start = perf_results_lines.index('============== running regression test queries        ==============\n')
del perf_results_lines[0:int(start)+1]

## Create the CSV output
counter = 1
rows = []
parallel_group_tests = 0
parallel_group_num_rows = 0
parallel_group_duration = 0.0
for line in perf_results_lines:
    if 'test ' in line:
        m = re.match(r'test (\w+)\s+... \w+ \((\d+.\d+) sec\)', line)
        rows.append('|'.join([str(counter), m.group(1), str(num_rows), m.group(2)]))
        counter += 1
    elif 'parallel group ' in line:
        m = re.match(r'parallel group \((\d+)\s', line)
        parallel_group_tests = int(m.group(1))
    elif parallel_group_tests > 0:
        parallel_group_num_rows += num_rows
        m = re.match(r'\s+(\w+)\s+... \w+ \((\d+.\d+) sec\)', line)

        if float(m.group(2)) > parallel_group_duration:
            parallel_group_duration = float(m.group(2))

        if parallel_group_tests == 1:
            rows.append('|'.join([str(counter), 'parallel_' + m.group(1), str(parallel_group_num_rows), str(parallel_group_duration)]))
            parallel_group_tests = 0
            parallel_group_num_rows = 0
            parallel_group_duration = 0.0
            counter += 1
        else:
            parallel_group_tests -= 1

## Output the CSV output into a CSV file
with open('perf_results.csv', 'w') as csv_write:
    for row in rows:
        csv_write.write('%s\n' % row)
