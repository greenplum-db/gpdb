-- start_matchsubs
-- m/\(actual time=\d+\.\d+..\d+\.\d+ rows=\d+ loops=\d+\)/
-- s/\(actual time=\d+\.\d+..\d+\.\d+ rows=\d+ loops=\d+\)/(actual time=##.###..##.### rows=# loops=#)/
-- m/\(slice\d+\)    Executor memory: (\d+)\w bytes\./
-- s/Executor memory: (\d+)\w bytes\./Executor memory: (#####)K bytes./
-- m/\(slice\d+\)    Executor memory: (\d+)\w bytes avg x \d+ workers, \d+\w bytes max \(seg\d+\)\./
-- s/Executor memory: (\d+)\w bytes avg x \d+ workers, \d+\w bytes max \(seg\d+\)\./Executor memory: ####K bytes avg x #### workers, ####K bytes max (seg#)./
-- m/Work_mem: \d+\w bytes max\./
-- s/Work_mem: \d+\w bytes max\. */Work_mem: ###K bytes max./
-- m/Memory: \d+kB  Max Memory: \d+kB  Peak Memory: \d+kB  Avg Memory: \d+kB \(3 segments\)/
-- s/Memory: \d+kB  Max Memory: \d+kB  Peak Memory: \d+kB  Avg Memory: \d+kB \(3 segments\)/Memory: ###kB  Max Memory: ###kB  Peak Memory: ###kB  Avg Memory: ###kB \(3 segments\)/
-- m/work_mem: \d+kB  Segments: 3  Max: \d+kB \(segment \d+\)  Workfile: \(\d+ spilling\)/
-- s/work_mem: \d+kB  Segments: 3  Max: \d+kB \(segment \d+\)  Workfile: \(\d+ spilling\)/work_mem: ###kB  Segments: 3  Max: ###kB \(segment ##\)  Workfile: \(# spilling\)/
-- m/Execution Time: \d+\.\d+ ms/
-- s/Execution Time: \d+\.\d+ ms/Execution Time: ##.### ms/
-- m/Planning Time: \d+\.\d+ ms/
-- s/Planning Time: \d+\.\d+ ms/Planning Time: ##.### ms/
-- m/cost=\d+\.\d+\.\.\d+\.\d+ rows=\d+ width=\d+/
-- s/\(cost=\d+\.\d+\.\.\d+\.\d+ rows=\d+ width=\d+\)/(cost=##.###..##.### rows=### width=###)/
-- m/Memory used:  \d+\w?B/
-- s/Memory used:  \d+\w?B/Memory used: ###B/
-- m/Memory Usage: \d+\w?B/
-- s/Memory Usage: \d+\w?B/Memory Usage: ###B/
-- m/Peak Memory Usage: \d+/
-- s/Peak Memory Usage: \d+/Peak Memory Usage: ###/
-- m/Buckets: \d+/
-- s/Buckets: \d+/Buckets: ###/
-- m/Batches: \d+/
-- s/Batches: \d+/Batches: ###/
-- m/Hash chain length \d+\.\d+ avg, \d+ max, using \d+ of \d+ buckets./
-- s/Hash chain length \d+\.\d+ avg, \d+ max, using \d+ of \d+ buckets./Hash chain length ###, using ### of ### buckets./
-- end_matchsubs
--
-- DEFAULT syntax
CREATE TABLE apples(id int PRIMARY KEY, type text);
INSERT INTO apples(id) SELECT generate_series(1, 100000);
CREATE TABLE box_locations(id int PRIMARY KEY, address text);
CREATE TABLE boxes(id int PRIMARY KEY, apple_id int REFERENCES apples(id), location_id int REFERENCES box_locations(id));

--- Check Explain Text format output
-- explain_processing_off
EXPLAIN SELECT * from boxes LEFT JOIN apples ON apples.id = boxes.apple_id LEFT JOIN box_locations ON box_locations.id = boxes.location_id;
-- explain_processing_on

--- Check Explain Analyze Text output that include the slices information
-- explain_processing_off
EXPLAIN (ANALYZE) SELECT * from boxes LEFT JOIN apples ON apples.id = boxes.apple_id LEFT JOIN box_locations ON box_locations.id = boxes.location_id;
-- explain_processing_on

-- Unaligned output format is better for the YAML / XML / JSON outputs.
-- In aligned format, you have end-of-line markers at the end of each line,
-- and its position depends on the longest line. If the width changes, all
-- lines need to be adjusted for the moved end-of-line-marker.
\a

-- YAML Required replaces for costs and time changes
-- start_matchsubs
-- m/ Loops: \d+/
-- s/ Loops: \d+/ Loops: #/
-- m/ Cost: \d+\.\d+/
-- s/ Cost: \d+\.\d+/ Cost: ###.##/
-- m/ Rows: \d+/
-- s/ Rows: \d+/ Rows: #####/
-- m/ Plan Width: \d+/
-- s/ Plan Width: \d+/ Plan Width: ##/
-- m/ Time: \d+\.\d+/
-- s/ Time: \d+\.\d+/ Time: ##.###/
-- m/Execution Time: \d+\.\d+/
-- s/Execution Time: \d+\.\d+/Execution Time: ##.###/
-- m/Segments: \d+$/
-- s/Segments: \d+$/Segments: #/
-- m/Pivotal Optimizer \(GPORCA\) version \d+\.\d+\.\d+",?/
-- s/Pivotal Optimizer \(GPORCA\) version \d+\.\d+\.\d+",?/Pivotal Optimizer \(GPORCA\)"/
-- m/ Memory: \d+$/
-- s/ Memory: \d+$/ Memory: ###/
-- m/Maximum Memory Used: \d+/
-- s/Maximum Memory Used: \d+/Maximum Memory Used: ###/
-- m/Workers: \d+/
-- s/Workers: \d+/Workers: ##/
-- m/Average: \d+/
-- s/Average: \d+/Average: ##/
-- m/Total memory used across slices: \d+/
-- s/Total memory used across slices: \d+\s*/Total memory used across slices: ###/
-- m/Memory used: \d+/
-- s/Memory used: \d+/Memory used: ###/
-- end_matchsubs
-- Check Explain YAML output
EXPLAIN (FORMAT YAML) SELECT * from boxes LEFT JOIN apples ON apples.id = boxes.apple_id LEFT JOIN box_locations ON box_locations.id = boxes.location_id;
SET random_page_cost = 1;
SET cpu_index_tuple_cost = 0.1;
EXPLAIN (FORMAT YAML, VERBOSE) SELECT * from boxes;
EXPLAIN (FORMAT YAML, VERBOSE, SETTINGS ON) SELECT * from boxes;

--- Check Explain Analyze YAML output that include the slices information
-- explain_processing_off
EXPLAIN (ANALYZE, FORMAT YAML) SELECT * from boxes LEFT JOIN apples ON apples.id = boxes.apple_id LEFT JOIN box_locations ON box_locations.id = boxes.location_id;

-- start_matchsubs
-- m/Executor Memory: \d+kB  Segments: 3  Max: \d+kB \(segment \d\)/
-- s/Executor Memory: \d+kB  Segments: 3  Max: \d+kB \(segment \d\)/Executor Memory: ###kB  Segments: 3  Max: ##kB (segment #)/
-- end_matchsubs
--- Check explain analyze sort infomation in verbose mode
EXPLAIN (ANALYZE, VERBOSE) SELECT * from boxes ORDER BY apple_id;
RESET random_page_cost;
RESET cpu_index_tuple_cost;
-- explain_processing_on

--
-- Test a simple case with JSON and XML output, too.
--
-- This should be enough for those format. The only difference between JSON,
-- XML, and YAML is in the formatting, after all.

-- Check JSON format
--
-- start_matchsubs
-- m/Pivotal Optimizer \(GPORCA\) version \d+\.\d+\.\d+/
-- s/Pivotal Optimizer \(GPORCA\) version \d+\.\d+\.\d+/Pivotal Optimizer \(GPORCA\)/
-- end_matchsubs
-- explain_processing_off
EXPLAIN (FORMAT JSON, COSTS OFF) SELECT * FROM generate_series(1, 10);

EXPLAIN (FORMAT XML, COSTS OFF) SELECT * FROM generate_series(1, 10);

-- Test for an old bug in printing Sequence nodes in JSON/XML format
-- (https://github.com/greenplum-db/gpdb/issues/9410)
CREATE TABLE jsonexplaintest (i int4) PARTITION BY RANGE (i) (START(1) END(3) EVERY(1));
EXPLAIN (FORMAT JSON, COSTS OFF) SELECT * FROM jsonexplaintest WHERE i = 2;

-- explain_processing_on

-- Cleanup
DROP TABLE boxes;
DROP TABLE apples;
DROP TABLE box_locations;
DROP TABLE jsonexplaintest;
