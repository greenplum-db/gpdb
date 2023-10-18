1. use `python upgrade_el8_locale_check.py precheck-index` to list affected indexes.
2. use `python upgrade_el8_locale_check.py precheck-table` to list affected partitioned tables.
3. use `python upgrade_el8_locale_check.py postfix` to run the reindex and alter partition table commands.

(Note: For easier reading, some example output is omitted with ellipses.)

```
$ python upgrade_el8_locale_check.py --help
usage: upgrade_el8_locale_check [-h] [--host HOST] [--port PORT]
                                [--dbname DBNAME] [--user USER]
                                {precheck-index,precheck-table,postfix} ...

positional arguments:
  {precheck-index,precheck-table,postfix}
                        sub-command help
    precheck-index      list affected index
    precheck-table      list affected tables
    postfix             run the reindex and the rebuild partition commands

optional arguments:
  -h, --help            show this help message and exit
  --host HOST           Greenplum Database hostname
  --port PORT           Greenplum Database port
  --dbname DBNAME       Greenplum Database database name
  --user USER           Greenplum Database user name
```
```
$ python upgrade_el8_locale_check.py precheck-index --help
usage: upgrade_el8_locale_check precheck-index [-h] --out OUT

optional arguments:
  -h, --help  show this help message and exit

required arguments:
  --out OUT   outfile path for the reindex commands

Example usage:

$ python upgrade_el8_locale_check.py precheck-index --out index.out
2023-10-18 11:04:13,944 - INFO - There are 2 catalog indexes that needs reindex when doing in-place upgrade from EL7->EL8.
2023-10-18 11:04:14,001 - INFO - There are 7 user indexes in database test that needs reindex when doing in-place upgrade from EL7->EL8.

$ cat index.out
-- DB name:  postgres
-- catalog indexrelid: 3597 | index name: pg_seclabel_object_index | table name: pg_seclabel | collname: default | indexdef:  CREATE UNIQUE INDEX pg_seclabel_object_index ON pg_catalog.pg_seclabel USING btree (objoid, classoid, objsubid, provider)
reindex index pg_seclabel_object_index;

-- catalog indexrelid: 3593 | index name: pg_shseclabel_object_index | table name: pg_shseclabel | collname: default | indexdef:  CREATE UNIQUE INDEX pg_shseclabel_object_index ON pg_catalog.pg_shseclabel USING btree (objoid, classoid, provider)
reindex index pg_shseclabel_object_index;

-- DB name:  test
-- indexrelid: 16512 | index name: testupgrade.hash_idx1 | table name: testupgrade.hash_test1 | collname: default | indexdef:  CREATE INDEX hash_idx1 ON testupgrade.hash_test1 USING btree (content)
reindex index testupgrade.hash_idx1;
...
```
```
$ python upgrade_el8_locale_check.py precheck-table --help
usage: upgrade_el8_locale_check precheck-table [-h] --out OUT
                                               [--order_size_ascend]
                                               [--nthread NTHREAD]

optional arguments:
  -h, --help           show this help message and exit
  --order_size_ascend  sort the tables by size in ascending order
  --nthread NTHREAD    the concurrent threads to check partition tables

required arguments:
  --out OUT            outfile path for the rebuild partition commands

Example usage:
$ python upgrade_el8_locale_check.py precheck-table --out table.out
2023-10-16 04:12:19,064 - WARNING - There are 2 tables in database test that the distribution key is using custom operator class, should be checked when doing in-place upgrade from EL7->EL8.
---------------------------------------------
tablename | distclass
('testdiskey', 16397)
('testupgrade.test_citext', 16454)
---------------------------------------------
2023-10-16 04:12:19,064 - INFO - There are 6 partitioned tables in database testupgrade that should be checked when doing in-place upgrade from EL7->EL8.
2023-10-16 04:12:19,066 - INFO - worker[0]: begin:
2023-10-16 04:12:19,066 - INFO - worker[0]: connect to <testupgrade> ...
2023-10-16 04:12:19,110 - INFO - start checking table testupgrade.partition_range_test_3_1_prt_mar ...
2023-10-16 04:12:19,162 - INFO - check table testupgrade.partition_range_test_3_1_prt_mar OK.
2023-10-16 04:12:19,162 - INFO - start checking table testupgrade.partition_range_test_3_1_prt_feb ...
2023-10-16 04:12:19,574 - INFO - check table testupgrade.partition_range_test_3_1_prt_feb error out: ERROR:  trying to insert row into wrong partition  (seg1 10.0.138.96:20001 pid=3975)
DETAIL:  Expected partition: partition_range_test_3_1_prt_mar, provided partition: partition_range_test_3_1_prt_feb.

2023-10-16 04:12:19,575 - INFO - start checking table testupgrade.partition_range_test_3_1_prt_jan ...
2023-10-16 04:12:19,762 - INFO - check table testupgrade.partition_range_test_3_1_prt_jan error out: ERROR:  trying to insert row into wrong partition  (seg1 10.0.138.96:20001 pid=3975)
DETAIL:  Expected partition: partition_range_test_3_1_prt_feb, provided partition: partition_range_test_3_1_prt_jan.

2023-10-16 04:12:19,804 - WARNING - no default partition for testupgrade.partition_range_test_3
...
2023-10-16 04:12:22,058 - INFO - Current progress: have 0 remaining, 2.99126505852 seconds passed.
2023-10-16 04:12:22,058 - INFO - worker[0]: finish.
---------------------------------------------
total table size (in GBytes) : 0.000396907329559
total partition tables       : 6
total leaf partitions        : 19
---------------------------------------------

Example Usage for using nthreads (check passed example):
$ python upgrade_el8_locale_check.py precheck-table --out table.out --nthread 3
2023-10-18 11:19:11,717 - INFO - There are 4 partitioned tables in database test that should be checked when doing in-place upgrade from EL7->EL8.
2023-10-18 11:19:11,718 - INFO - worker[0]: begin:
2023-10-18 11:19:11,718 - INFO - worker[0]: connect to <test> ...
2023-10-18 11:19:11,718 - INFO - worker[1]: begin:
2023-10-18 11:19:11,719 - INFO - worker[1]: connect to <test> ...
2023-10-18 11:19:11,718 - INFO - worker[2]: begin:
2023-10-18 11:19:11,719 - INFO - worker[2]: connect to <test> ...
2023-10-18 11:19:11,744 - INFO - start checking table testupgrade.partition_range_test_1_1_prt_mar ...
2023-10-18 11:19:11,745 - INFO - start checking table testupgrade.partition_range_test_ao_1_prt_mar ...
2023-10-18 11:19:11,746 - INFO - start checking table testupgrade.partition_range_test_2_1_prt_mar ...
2023-10-18 11:19:11,749 - INFO - check table testupgrade.partition_range_test_1_1_prt_mar OK.
2023-10-18 11:19:11,749 - INFO - start checking table testupgrade.partition_range_test_1_1_prt_feb ...
2023-10-18 11:19:11,751 - INFO - check table testupgrade.partition_range_test_ao_1_prt_mar OK.
2023-10-18 11:19:11,751 - INFO - start checking table testupgrade.partition_range_test_ao_1_prt_feb ...
2023-10-18 11:19:11,751 - INFO - check table testupgrade.partition_range_test_2_1_prt_mar OK.
2023-10-18 11:19:11,751 - INFO - start checking table testupgrade.partition_range_test_2_1_prt_feb ...
2023-10-18 11:19:11,752 - INFO - check table testupgrade.partition_range_test_1_1_prt_feb OK.
2023-10-18 11:19:11,752 - INFO - start checking table testupgrade.partition_range_test_1_1_prt_others ...
2023-10-18 11:19:11,754 - INFO - check table testupgrade.partition_range_test_2_1_prt_feb OK.
2023-10-18 11:19:11,754 - INFO - start checking table testupgrade.partition_range_test_2_1_prt_jan ...
2023-10-18 11:19:11,755 - INFO - check table testupgrade.partition_range_test_1_1_prt_others OK.
2023-10-18 11:19:11,755 - INFO - check table testupgrade.partition_range_test_ao_1_prt_feb OK.
2023-10-18 11:19:11,755 - INFO - start checking table testupgrade.partition_range_test_ao_1_prt_jan ...
2023-10-18 11:19:11,756 - INFO - Current progress: have 1 remaining, 0.0373229980469 seconds passed.
2023-10-18 11:19:11,757 - INFO - check table testupgrade.partition_range_test_2_1_prt_jan OK.
2023-10-18 11:19:11,758 - INFO - Current progress: have 0 remaining, 0.0390849113464 seconds passed.
2023-10-18 11:19:11,758 - INFO - worker[2]: finish.
2023-10-18 11:19:11,761 - INFO - check table testupgrade.partition_range_test_ao_1_prt_jan OK.
2023-10-18 11:19:11,761 - INFO - Current progress: have 0 remaining, 0.0425379276276 seconds passed.
2023-10-18 11:19:11,761 - INFO - worker[1]: finish.
2023-10-18 11:19:11,763 - INFO - start checking table testupgrade.root_1_prt_mar ...
2023-10-18 11:19:11,766 - INFO - check table testupgrade.root_1_prt_mar OK.
2023-10-18 11:19:11,767 - INFO - start checking table testupgrade.root_1_prt_feb ...
2023-10-18 11:19:11,769 - INFO - check table testupgrade.root_1_prt_feb OK.
2023-10-18 11:19:11,770 - INFO - start checking table testupgrade.root_1_prt_jan ...
2023-10-18 11:19:11,772 - INFO - check table testupgrade.root_1_prt_jan OK.
2023-10-18 11:19:11,773 - INFO - Current progress: have 0 remaining, 0.0547292232513 seconds passed.
2023-10-18 11:19:11,773 - INFO - worker[0]: finish.
---------------------------------------------
total table size (in GBytes) : 0.0
total partition tables       : 0
total leaf partitions        : 0
---------------------------------------------

$ cat table.out
-- order table by size in descending order
-- DB name:  testupgrade

-- parrelid: 16649 | coll: 100 | attname: date | msg: partition table, 3 leafs, size 98304
begin; create temp table "testupgrade.partition_range_test_3_bak" as select * from testupgrade.partition_range_test_3; truncate testupgrade.partition_range_test_3; insert into testupgrade.partition_range_test_3 select * from "testupgrade.partition_range_test_3_bak"; commit;
...

```
```
$ python upgrade_el8_locale_check.py postfix --help
usage: upgrade_el8_locale_check postfix [-h] --input INPUT

optional arguments:
  -h, --help     show this help message and exit

required arguments:
  --input INPUT  the file contains reindex or rebuild partition commands

Example usage for postfix index:
$ python upgrade_el8_locale_check.py postfix --input index.out
2023-10-16 04:12:02,461 - INFO - db: testupgrade, total have 7 commands to execute
2023-10-16 04:12:02,467 - INFO - db: testupgrade, executing command: reindex index testupgrade.test_id1;
2023-10-16 04:12:02,541 - INFO - db: testupgrade, executing command: reindex index testupgrade.test_id2;
2023-10-16 04:12:02,566 - INFO - db: testupgrade, executing command: reindex index testupgrade.test_id3;
2023-10-16 04:12:02,592 - INFO - db: testupgrade, executing command: reindex index testupgrade.test_citext_pkey;
2023-10-16 04:12:02,623 - INFO - db: testupgrade, executing command: reindex index testupgrade.test_idx_citext;
2023-10-16 04:12:02,647 - INFO - db: testupgrade, executing command: reindex index testupgrade.hash_idx1;
2023-10-16 04:12:02,673 - INFO - db: testupgrade, executing command: reindex index testupgrade.idx_projecttag;
2023-10-16 04:12:02,692 - INFO - db: postgres, total have 2 commands to execute
2023-10-16 04:12:02,698 - INFO - db: postgres, executing command: reindex index pg_seclabel_object_index;
2023-10-16 04:12:02,730 - INFO - db: postgres, executing command: reindex index pg_shseclabel_object_index;
2023-10-16 04:12:02,754 - INFO - All done

Example usage for postfix tables:
$ python upgrade_el8_locale_check.py postfix --input table.out
2023-10-16 04:14:17,003 - INFO - db: testupgrade, total have 6 commands to execute
2023-10-16 04:14:17,009 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.partition_range_test_3_bak" as select * from testupgrade.partition_range_test_3; truncate testupgrade.partition_range_test_3; insert into testupgrade.partition_range_test_3 select * from "testupgrade.partition_range_test_3_bak"; commit;
2023-10-16 04:14:17,175 - INFO - db: testupgrade, executing analyze command: analyze testupgrade.partition_range_test_3;;
2023-10-16 04:14:17,201 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.partition_range_test_2_bak" as select * from testupgrade.partition_range_test_2; truncate testupgrade.partition_range_test_2; insert into testupgrade.partition_range_test_2 select * from "testupgrade.partition_range_test_2_bak"; commit;
2023-10-16 04:14:17,490 - ERROR - ERROR:  no partition for partitioning key  (seg1 10.0.138.96:20001 pid=4028)

2023-10-16 04:14:17,497 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.partition_range_test_4_bak" as select * from testupgrade.partition_range_test_4; truncate testupgrade.partition_range_test_4; insert into testupgrade.partition_range_test_4 select * from "testupgrade.partition_range_test_4_bak"; commit;
2023-10-16 04:14:17,628 - INFO - db: testupgrade, executing analyze command: analyze testupgrade.partition_range_test_4;;
2023-10-16 04:14:17,660 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.partition_range_test_1_bak" as select * from testupgrade.partition_range_test_1; truncate testupgrade.partition_range_test_1; insert into testupgrade.partition_range_test_1 select * from "testupgrade.partition_range_test_1_bak"; commit;
2023-10-16 04:14:17,784 - INFO - db: testupgrade, executing analyze command: analyze testupgrade.partition_range_test_1;;
2023-10-16 04:14:17,808 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.root_bak" as select * from testupgrade.root; truncate testupgrade.root; insert into testupgrade.root select * from "testupgrade.root_bak"; commit;
2023-10-16 04:14:17,928 - INFO - db: testupgrade, executing analyze command: analyze testupgrade.root;;
2023-10-16 04:14:17,952 - INFO - db: testupgrade, executing command: begin; create temp table "testupgrade.partition_range_test_ao_bak" as select * from testupgrade.partition_range_test_ao; truncate testupgrade.partition_range_test_ao; insert into testupgrade.partition_range_test_ao select * from "testupgrade.partition_range_test_ao_bak"; commit;
2023-10-16 04:14:18,276 - ERROR - ERROR:  no partition for partitioning key  (seg1 10.0.138.96:20001 pid=4060)

2023-10-16 04:14:18,277 - INFO - All done
```

