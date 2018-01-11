@backup_and_restore_restores
Feature: Validate command line arguments

    @nbuonly
    @nbusetup77
    Scenario: Setup to load NBU libraries
        Given the test suite is initialized for Netbackup "7.7"

    @ddonly
    @ddboostsetup
    Scenario: Setup DDBoost configuration
        Given the test suite is initialized for DDBoost

    Scenario: 1 Dirty table list check on recreating a table with same data and contents
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that plan file has latest timestamp for "public.ao_table"

    @nbuonly
    @nbupartI
    @nbupartII
    @nbupartIII
    Scenario: NetBackup dummy to absorb flakiness
        Given the backup test is initialized with database "bkdb_nbu"
        And there is a "ao" table "public.ao_table" in "bkdb_nbu" with data
        When the user runs "gpcrondump -a -x bkdb_nbu"
        #We don't care what happens because we expect it to fail

    @nbupartI
    @ddpartI
    Scenario: 2 Simple Incremental Backup
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that partitioned tables "ao_part_table, co_part_table, heap_part_table" in "bkdb2" have 6 partitions
        And verify that partitioned tables "ao_part_table_comp, co_part_table_comp" in "bkdb2" have 6 partitions
        And verify that partitioned tables "part_external" in "bkdb2" have 5 partitions in partition level "0"
        And verify that partitioned tables "ao_part_table, co_part_table_comp" in "bkdb2" has 0 empty partitions
        And verify that partitioned tables "co_part_table, ao_part_table_comp" in "bkdb2" has 0 empty partitions
        And verify that partitioned tables "heap_part_table" in "bkdb2" has 0 empty partitions
        And verify that there is a "heap" table "public.heap_table" in "bkdb2"
        And verify that there is a "heap" table "public.heap_index_table" in "bkdb2"
        And verify that there is partition "1" of "ao" partition table "ao_part_table" in "bkdb2" in "public"
        And verify that there is partition "1" of "co" partition table "co_part_table_comp" in "bkdb2" in "public"
        And verify that there is partition "1" of "heap" partition table "heap_part_table" in "bkdb2" in "public"
        And verify that there is partition "2" of "heap" partition table "heap_part_table" in "bkdb2" in "public"
        And verify that there is partition "3" of "heap" partition table "heap_part_table" in "bkdb2" in "public"
        And verify that there is partition "1" of mixed partition table "part_mixed_1" with storage_type "c"  in "bkdb2" in "public"
        And verify that there is partition "2" in partition level "0" of mixed partition table "part_external" with storage_type "x"  in "bkdb2" in "public"
        And verify that the data of the dirty tables under " " in "bkdb2" is validated after restore
        And verify that the distribution policy of all the tables in "bkdb2" are validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb2"

    Scenario: 4 gpdbrestore with -R for full dump
        Given the old timestamps are read from json
        Then the user runs gpdbrestore with "-R" option in path "/tmp/4"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb4" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb4"

    Scenario: 5 gpdbrestore with -R for incremental dump
        Given the old timestamps are read from json
        Then the user runs gpdbrestore with "-R" option in path "/tmp/5"
        And gpdbrestore should print "-R is not supported for restore with incremental timestamp" to stdout

    @nbupartI
    @ddpartI
    Scenario: 5a Full Backup and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb5a" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb5a" with data
        And verify that the "report" file in " " dir does not contain "ERROR"
        And verify that the "status" file in " " dir does not contain "ERROR"
        And verify that there is a constraint "check_constraint_no_domain" in "bkdb5a"
        And verify that there is a constraint "check_constraint_with_domain" in "bkdb5a"
        And verify that there is a constraint "unique_constraint" in "bkdb5a"
        And verify that there is a constraint "foreign_key" in "bkdb5a"
        And verify that there is a rule "myrule" in "bkdb5a"
        And verify that there is a trigger "mytrigger" in "bkdb5a"
        And verify that there is an index "my_unique_index" in "bkdb5a"

    @nbupartI
    @ddpartI
    Scenario: 6 Metadata-only restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-m"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb6"
        And the table names in "bkdb6" is stored
        And tables in "bkdb6" should not contain any data

    @nbupartI
    @ddpartI
    Scenario: 7 Metadata-only restore with global objects (-G)
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-m -G"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb7"
        And the table names in "bkdb7" is stored
        And tables in "bkdb7" should not contain any data
        And verify that a role "foo%userWITHCAPS" exists in database "bkdb7"
        And the user runs "psql -c 'DROP ROLE "foo%userWITHCAPS"' bkdb7"

    @ddpartI
    Scenario: 8 gpdbrestore -L with Full Backup
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-L"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Table public.ao_part_table" to stdout
        And gpdbrestore should print "Table public.heap_table" to stdout

    @nbupartI
    @ddpartI
    Scenario: 11 Backup and restore with -G only
        Given the old timestamps are read from json
        When the user runs gpdbrestore without -e with the stored timestamp and options "-G only"
        Then gpdbrestore should return a return code of 0
        And verify that a role "foo_user" exists in database "template1"
        And the user runs "psql -c 'DROP ROLE foo_user' template1"

    @valgrind
    Scenario: 12 Valgrind test of gp_restore for incremental backup
        Given the old timestamps are read from json
        Then the user runs valgrind with "gp_restore" and options "-i --gp-i --gp-l=p -d bkdb12 --gp-c"

    @valgrind
    Scenario: 13 Valgrind test of gp_restore_agent for incremental backup
        Given the old timestamps are read from json
        Then the user runs valgrind with "gp_restore_agent" and options "--gp-c /bin/gunzip -s --post-data-schema-only --target-dbid 1 -d bkdb13"

    @nbupartI
    @ddpartI
    @skip_for_gpdb_43
    Scenario: 14 Full Backup with option -t and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb14" with data
        And verify that there is no table "public.ao_part_table" in "bkdb14"

    @nbupartI
    @ddpartI
    @skip_for_gpdb_43
    Scenario: 15 Full Backup with option -T and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then verify that the "report" file in " " dir contains "Backup Type: Full"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb15" with data
        And verify that there is no table "public.heap_table" in "bkdb15"

    @nbupartI
    @ddpartI
    Scenario: 16 Full Backup with option --exclude-table-file and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "co" table "public.co_part_table" in "bkdb16" with data
        And verify that there is no table "public.ao_part_table" in "bkdb16"
        And verify that there is no table "public.heap_table" in "bkdb16"

    @nbupartI
    @ddpartI
    Scenario: 17 Full Backup with option --table-file and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb17" with data
        And verify that there is a "heap" table "public.heap_table" in "bkdb17" with data
        And verify that there is no table "public.co_part_table" in "bkdb17"

    Scenario: 19 Simple Plan File Test
        Given the old timestamps are read from json
        And the timestamp labels for scenario "19" are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file for scenario "19" is validated against "data/bar_plan1"

    @nbupartI
    @ddpartI
    Scenario: 20 No plan file generated
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then "plan" file should not be created under " "

    Scenario: 21 Schema only restore of incremental backup
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb21"
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And tables names in database "bkdb21" should be identical to stored table names in file "part_table_names"

    @nbupartI
    @ddpartI
    Scenario: 22 Simple Incremental Backup with AO/CO statistics w/ filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb22" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb22" for table "public.ao_table"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_table"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb22" for table "public.ao_index_table"
        And verify that there are "8760" tuples in "bkdb22" for table "public.ao_table"

    Scenario: 23 Simple Incremental Backup with TRUNCATE
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "21" tables in "bkdb23" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb23"

    Scenario: 24 Simple Incremental Backup to test ADD COLUMN
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "23" tables in "bkdb24" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb24"

    @nbupartI
    @ddpartI
    Scenario: 25 Non compressed incremental backup
        Given the old timestamps are read from json
        Then the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb25"
        And verify that the data of "11" tables in "bkdb25" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb25"
        And verify that the plan file is created for the latest timestamp

    Scenario: 26 Rollback Insert
        Given the old timestamps are read from json
        And the timestamp labels for scenario "26" are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the plan file for scenario "26" is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb26" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb26"

    Scenario: 27 Rollback Truncate Table
        Given the old timestamps are read from json
        And the timestamp labels for scenario "27" are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the plan file for scenario "27" is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb27" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb27"

    Scenario: 28 Rollback Alter table
        Given the old timestamps are read from json
        And the timestamp labels for scenario "28" are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the plan file for scenario "28" is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb28" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb28"

    @ddpartI
    Scenario: 29 Verify gpdbrestore -s option works with full backup
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb29 -a"
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb29" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb29"
        And verify that database "bkdb29-2" does not exist

    @ddpartI
    Scenario: 30 Verify gpdbrestore -s option works with incremental backup
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb30 -a"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb30" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb30"
        And verify that database "bkdb30-2" does not exist

    @nbupartI
    Scenario: 31 gpdbrestore -u option with full backup
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-u /tmp"
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb31" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb31"

    @nbupartI
    Scenario: 32 gpdbrestore -u option with incremental backup
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-u /tmp"
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "3" tables in "bkdb32" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb32"

    Scenario: 33 gpcrondump -x with multiple databases
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb33 -a"
        Then gpdbrestore should return a return code of 0
        And the user runs "gpdbrestore -e -s bkdb33-2 -a"
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb33" is validated after restore
        And verify that the data of "2" tables in "bkdb33-2" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb33"
        And verify that the tuple count of all appendonly tables are consistent in "bkdb33-2"

    @nbupartI
    @ddpartI
    Scenario: 34 gpdbrestore with --table-file option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file /tmp/table_file_foo"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb34" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb34"
        And verify that the restored table "public.ao_table" in database "bkdb34" is analyzed
        And verify that the restored table "public.co_table" in database "bkdb34" is analyzed
        Then the file "/tmp/table_file_foo" is removed from the system

    @nbupartI
    @ddpartI
    Scenario: 35 Incremental restore with extra full backup
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb35" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb35"

    Scenario: 36 gpcrondump should not track external tables
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "4" tables in "bkdb36" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb36"
        And verify that there is no "public.ext_tab" in the "dirty_list" file in " "
        And verify that there is no "public.ext_tab" in the "table_list" file in " "
        Then the file "/tmp/ext_tab" is removed from the system

    @nbupartI
    @ddpartI
    Scenario: 37 Full backup with -T option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_index_table"
        Then gpdbrestore should return a return code of 0
        Then verify that there is no table "public.ao_part_table" in "fullbkdb37"
        And verify that there is no table "public.heap_table" in "fullbkdb37"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb37" with data

    @nbupartI
    @ddpartI
    Scenario: 38 gpdbrestore with -T option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_index_table -a"
        Then gpdbrestore should return a return code of 0
        Then verify that there is no table "public.ao_part_table" in "bkdb38"
        And verify that there is no table "public.heap_table" in "bkdb38"
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb38" with data

    @nbupartI
    @ddpartI
    Scenario: 39 Full backup and restore with -T and --truncate
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb39"
        And there is a "ao" table "public.ao_index_table" in "bkdb39" with data
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table --truncate"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb39" with data
        And verify that the restored table "public.ao_index_table" in database "bkdb39" is analyzed
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_part_table"
        And the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_part_table_1_prt_p1_2_prt_1 --truncate"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb39" with data

    Scenario: 40 Full backup and restore with -T and --truncate with dropped table
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb40"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.heap_table --truncate"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Skipping truncate of bkdb40.public.heap_table because the relation does not exist" to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb40" with data

    @nbupartII
    @ddpartII
    Scenario: 41 Full backup -T with truncated table
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb41"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb41" with data
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "bkdb41"
        And the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_part_table_1_prt_p2_2_prt_3"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table_1_prt_p2_2_prt_3" in "bkdb41" with data
        And verify that the restored table "public.ao_part_table_1_prt_p2_2_prt_3" in database "bkdb41" is analyzed

    Scenario: 42 Full backup -T with no schema name supplied
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb42"
        When the user runs gpdbrestore -e with the stored timestamp and options "-T ao_index_table -a"
        Then gpdbrestore should return a return code of 2
        Then gpdbrestore should print "No schema name supplied" to stdout

    Scenario: 43 Full backup with gpdbrestore -T for DB with FUNCTION having DROP SQL
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb43"
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_index_table -a"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb43" with data

    @nbupartII
    @ddpartII
    Scenario: 44 Incremental restore with table filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_table -T public.co_table"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "bkdb44" have been restored

    Scenario: 45 Incremental restore with invalid table filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.heap_table -T public.invalid -q"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Tables \[\'public.invalid\'\] not found in backup" to stdout

    Scenario: 46 gpdbrestore -L with -u option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-L -u /tmp"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Table public.ao_part_table" to stdout
        And gpdbrestore should print "Table public.heap_table" to stdout

    Scenario: 47 gpdbrestore -b with -u option for Full timestamp
        Given the old timestamps are read from json
        Then the user runs gpdbrestore on dump date directory with options "-u /tmp/47"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb47" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb47"

    Scenario: 48 gpdbrestore with -s and -u options for full backup
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb48 -u /tmp -a"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb48" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb48"

    Scenario: 49 gpdbrestore with -s and -u options for incremental backup
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb49 -u /tmp -a"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "12" tables in "bkdb49" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb49"

    @ddpartII
    Scenario: 50 gpdbrestore -b option should display the timestamps in sorted order
        Given the old timestamps are read from json
        Then the user runs gpdbrestore -e with the date directory
        And the timestamps should be printed in sorted order

    Scenario: 51 gpdbrestore -R option should display the timestamps in sorted order
        Given the old timestamps are read from json
        Then the user runs gpdbrestore with "-R" option in path "/tmp"
        And the timestamps should be printed in sorted order

    Scenario: 54 Test gpcrondump and gpdbrestore verbose option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--verbose"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb54" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb54"

    Scenario: 55 Incremental table filter gpdbrestore with different schema for same tablenames
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_part_table -T testschema.ao_part_table"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_3" in "bkdb55"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb55"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_2" in "bkdb55"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb55"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_1" in "bkdb55"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_3" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_2" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_1" in "bkdb55"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb55"

    @nbupartII
    Scenario: 56 Incremental table filter gpdbrestore with noplan option
        Given the old timestamps are read from json
        And database "bkdb56" is dropped and recreated
        When the user runs gpdbrestore -e with the stored full timestamp and options "-m"
        Then gpdbrestore should return a return code of 0
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_part_table --noplan"
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_3, public.ao_part_table_1_prt_p2_2_prt_3" in "bkdb56" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_2, public.ao_part_table_1_prt_p2_2_prt_2" in "bkdb56" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_1, public.ao_part_table_1_prt_p2_2_prt_1" in "bkdb56" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_3, public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb56" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_2, public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb56" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_1, public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb56" has no rows

    @nbupartII
    @ddpartII
    Scenario: 57 gpdbrestore list_backup option
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb57"
        When the user runs gpdbrestore without -e with the stored timestamp and options "--list-backup"
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout
        Then "plan" file is removed under " "
        When the user runs gpdbrestore without -e with the stored timestamp and options "--list-backup -a"
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout

    Scenario: 58 gpdbrestore list_backup option with -T table filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.heap_table --list-backup"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Cannot specify -T and --list-backup together" to stdout

    @nbupartII
    @ddpartII
    Scenario: 59 gpdbrestore list_backup option with full timestamp
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb59"
        When the user runs gpdbrestore without -e with the stored timestamp and options "--list-backup"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "--list-backup is not supported for restore with full timestamps" to stdout

    Scenario: 60 Incremental Backup and Restore with named pipes
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb60"
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb60" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb60" with data
        Then table "public.ao_part_table" is assumed to be in dirty state in "bkdb60"
        When the named pipe script for the "restore" is run for the files under "/tmp/custom_timestamps"
        And all the data from "bkdb60" is saved for verification
        Then gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb60" is validated after restore
        When the named pipe script for the "restore" is run for the files under "/tmp/custom_timestamps"
        And the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_part_table -u /tmp/custom_timestamps"
        Then gpdbrestore should print "\[WARNING\]:-Skipping validation of tables in dump file due to the use of named pipes" to stdout
        And close all opened pipes

    @nbupartII
    @ddpartII
    Scenario: 61 Incremental Backup and Restore with -t filter for Full
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb61" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb61" with data
        And verify that there is no table "public.ao_part_table" in "bkdb61"

    Scenario: 62 Incremental Backup and Restore with -T filter for Full
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb62" with data
        And verify that there is no table "public.ao_part_table" in "bkdb62"
        And verify that there is no table "public.heap_table" in "bkdb62"

    Scenario: 63 Incremental Backup and Restore with --table-file filter for Full
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb63" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb63" with data
        And verify that there is no table "public.ao_part_table" in "bkdb63"

    Scenario: 64 Incremental Backup and Restore with --exclude-table-file filter for Full
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb64" with data
        And verify that there is no table "public.ao_part_table" in "bkdb64"
        And verify that there is no table "public.heap_table" in "bkdb64"

    @nbupartII
    @ddpartII
    Scenario: 65 Full Backup with option -T and non-existant table
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then verify that the "report" file in " " dir contains "Backup Type: Full"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb65" with data
        And verify that there is no table "public.heap_table" in "bkdb65"

    Scenario: 66 Negative test gpdbrestore -G with incremental timestamp
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-G"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Unable to locate global file" to stdout

    @nbupartII
    Scenario: 67 Dump and Restore metadata
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb67"
        When the user runs "gpdbrestore -a -t 20140101010101 -u /tmp/custom_timestamps"
        Then gpdbrestore should return a return code of 0
        And the user runs """psql -c "ALTER TABLE heap_table DISABLE TRIGGER before_heap_ins_trig;" bkdb67"""
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/check_metadata.sql bkdb67 > /tmp/check_metadata.out"
        And verify that the contents of the files "/tmp/check_metadata.out" and "test/behave/mgmt_utils/steps/data/check_metadata.ans" are identical
        And the directory "/tmp/check_metadata.out" is removed or does not exist

    Scenario: 68 Restore -T for incremental dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb68"
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata"
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb68"
        Then psql should print "2000" to stdout 4 times
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb68" with data
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb68 > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb68 > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/68_describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/68_describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/68_describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/68_describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    Scenario: 69 Restore -T for full dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb69"
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata"
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb69"
        Then psql should print "1000" to stdout 4 times
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb69" with data
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb69 > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb69 > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/69_describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/69_describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/69_describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/69_describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    @fails_on_mac
    Scenario: 70 Restore -T for full dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb70"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs "psql -c 'CREATE ROLE foo_user' bkdb70"
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp --noanalyze"
        Then gpdbrestore should return a return code of 0
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb70"
        Then psql should print "1000" to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "bkdb70" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "bkdb70" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb70 > /tmp/describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb70 > /tmp/privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb70 > /tmp/describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb70 > /tmp/privileges_heap_index_table_2_after"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb70 > /tmp/describe_multi_byte_char_after"
        Then verify that the contents of the files "/tmp/70_describe_heap_index_table_1_before" and "/tmp/describe_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/70_describe_heap_index_table_2_before" and "/tmp/describe_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/70_privileges_heap_index_table_1_before" and "/tmp/privileges_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/70_privileges_heap_index_table_2_before" and "/tmp/privileges_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/70_describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And the file "/tmp/70_describe_heap_index_table_1_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_1_after" is removed from the system
        And the file "/tmp/70_privileges_heap_index_table_1_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_1_after" is removed from the system
        And the file "/tmp/70_describe_heap_index_table_2_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_2_after" is removed from the system
        And the file "/tmp/70_privileges_heap_index_table_2_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_2_after" is removed from the system
        And the file "/tmp/70_describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system

    @fails_on_mac
    Scenario: 71 Restore -T for incremental dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb71"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        When the user runs "psql -c 'CREATE ROLE foo_user' bkdb71"
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp --noanalyze"
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb71"
        Then psql should print "2000" to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "bkdb71" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "bkdb71" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb71 > /tmp/71_describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb71 > /tmp/71_privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb71 > /tmp/71_describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb71 > /tmp/71_privileges_heap_index_table_2_after"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb71 > /tmp/71_describe_multi_byte_char_after"
        Then verify that the contents of the files "/tmp/71_describe_heap_index_table_1_before" and "/tmp/71_describe_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/71_describe_heap_index_table_2_before" and "/tmp/71_describe_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/71_privileges_heap_index_table_1_before" and "/tmp/71_privileges_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/71_privileges_heap_index_table_2_before" and "/tmp/71_privileges_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/71_describe_multi_byte_char_before" and "/tmp/71_describe_multi_byte_char_after" are identical
        And the file "/tmp/71_describe_heap_index_table_1_before" is removed from the system
        And the file "/tmp/71_describe_heap_index_table_1_after" is removed from the system
        And the file "/tmp/71_privileges_heap_index_table_1_before" is removed from the system
        And the file "/tmp/71_privileges_heap_index_table_1_after" is removed from the system
        And the file "/tmp/71_describe_heap_index_table_2_before" is removed from the system
        And the file "/tmp/71_describe_heap_index_table_2_after" is removed from the system
        And the file "/tmp/71_privileges_heap_index_table_2_before" is removed from the system
        And the file "/tmp/71_privileges_heap_index_table_2_after" is removed from the system
        And the file "/tmp/71_describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/71_describe_multi_byte_char_after" is removed from the system

    @nbupartII
    @ddpartII
    Scenario: 72 Redirected Restore Full Backup and Restore without -e option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb72-2"
        Then gpdbrestore should return a return code of 0
        And check that there is a "heap" table "public.heap_table" in "bkdb72-2" with same data from "bkdb72"
        And check that there is a "ao" table "public.ao_part_table" in "bkdb72-2" with same data from "bkdb72"

    @nbupartII
    @ddpartII
    Scenario: 73 Full Backup and Restore with -e option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb73-2"
        Then gpdbrestore should return a return code of 0
        And check that there is a "heap" table "public.heap_table" in "bkdb73-2" with same data from "bkdb73"
        And check that there is a "ao" table "public.ao_part_table" in "bkdb73-2" with same data from "bkdb73"

    @nbupartII
    @ddpartII
    Scenario: 74 Incremental Backup and Redirected Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb74-2"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb74-2" is validated after restore from "bkdb74"

    @nbupartII
    @ddpartII
    Scenario: 75 Full backup and redirected restore with -T
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_index_table --redirect=bkdb75-2"
        Then gpdbrestore should return a return code of 0
        And check that there is a "ao" table "public.ao_index_table" in "bkdb75-2" with same data from "bkdb75"

    @nbupartII
    @ddpartII
    Scenario: 76 Full backup and redirected restore with -T and --truncate
        Given the old timestamps are read from json
        And the database "bkdb76-2" does not exist
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table --redirect=bkdb76-2 --truncate"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Failure from truncating tables, FATAL:  database "bkdb76-2" does not exist" to stdout
        And there is a "ao" table "public.ao_index_table" in "bkdb76-2" with data
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table --redirect=bkdb76-2 --truncate"
        Then gpdbrestore should return a return code of 0
        And check that there is a "ao" table "public.ao_index_table" in "bkdb76-2" with same data from "bkdb76"

    @nbupartII
    @ddpartII
    Scenario: 77 Incremental redirected restore with table filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_table -T public.co_table --redirect=bkdb77-2"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "bkdb77-2" have been restored from "bkdb77"

    @nbupartII
    @ddpartII
    Scenario: 78 Full Backup and Redirected Restore with --prefix option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo --redirect=bkdb78-2"
        Then gpdbrestore should return a return code of 0
        And check that there is a "heap" table "public.heap_table" in "bkdb78-2" with same data from "bkdb78"
        And check that there is a "ao" table "public.ao_part_table" in "bkdb78-2" with same data from "bkdb78"

    @nbupartII
    @ddpartII
    Scenario: 79 Full Backup and Redirected Restore with --prefix option for multiple databases
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo --redirect=bkdb79-3"
        Then gpdbrestore should return a return code of 0
        And check that there is a "heap" table "public.heap_table" in "bkdb79-3" with same data from "bkdb79-2"
        And check that there is a "ao" table "public.ao_part_table" in "bkdb79-3" with same data from "bkdb79"

    Scenario: 80 Full Backup and Restore with the master dump file missing
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Unable to find .*. Skipping restore." to stdout

    Scenario: 81 Incremental Backup and Restore with the master dump file missing
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Unable to find .*. Skipping restore." to stdout

    Scenario: 82 Uppercase Database Name Full Backup and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "Issue with analyze of" to stdout
        And verify that there is a "heap" table "public.heap_table" in "82TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "82TESTING" with data

    Scenario: 83 Uppercase Database Name Full Backup and Restore using -s option with and without quotes
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -s 83TESTING -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "Issue with analyze of" to stdout
        And verify that there is a "heap" table "public.heap_table" in "83TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "83TESTING" with data
        And the user runs "gpdbrestore -s "83TESTING" -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "Issue with analyze of" to stdout
        And verify that there is a "heap" table "public.heap_table" in "83TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "83TESTING" with data

    Scenario: 84 Uppercase Database Name Incremental Backup and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "Issue with analyze of" to stdout
        And verify that the data of "11" tables in "84TESTING" is validated after restore

    Scenario: 85 Full backup and Restore should create the gp_toolkit schema with -e option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb85" is validated after restore
        And verify that the schema "gp_toolkit" exists in "bkdb85"

    Scenario: 86 Incremental backup and Restore should create the gp_toolkit schema with -e option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb86" is validated after restore
        And verify that the schema "gp_toolkit" exists in "bkdb86"

    Scenario: 87 Redirected Restore should create the gp_toolkit schema with or without -e option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb87-2"
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "10" tables in "bkdb87-2" is validated after restore from "bkdb87"
        And verify that the schema "gp_toolkit" exists in "bkdb87-2"
        And the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb87-2"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb87-2" is validated after restore from "bkdb87"
        And verify that the schema "gp_toolkit" exists in "bkdb87-2"

    Scenario: 88 gpdbrestore with noanalyze
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb88"
        When the user runs gpdbrestore -e with the stored timestamp and options "--noanalyze -a"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Analyze bypassed on request" to stdout
        And verify that the data of "10" tables in "bkdb88" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb88"

    Scenario: 89 gpdbrestore without noanalyze
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Commencing analyze of bkdb89 database" to stdout
        And gpdbrestore should print "Analyze of bkdb89 completed without error" to stdout
        And verify that the data of "10" tables in "bkdb89" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb89"

    @nbupartII
    @ddpartII
    Scenario: 90 Writable Report/Status Directory Full Backup and Restore without --report-status-dir option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "gp-r" to stdout
        And gpdbrestore should not print "status" to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb90" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb90" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"

    @nbupartII
    @ddpartII
    Scenario: 91 Writable Report/Status Directory Full Backup and Restore with --report-status-dir option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--report-status-dir=/tmp"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "gp-r" to stdout
        And gpdbrestore should print "status" to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb91" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb91" with data
        And verify that report file is generated in /tmp
        And verify that status file is generated in /tmp
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"

    Scenario: 92 Writable Report/Status Directory Full Backup and Restore with -u option
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -u /tmp/92_custom_timestamps -e -a -t 20150101010101"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "gp-r" to stdout
        And gpdbrestore should print "status" to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb92" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb92" with data
        And verify that report file is generated in /tmp/92_custom_timestamps/db_dumps/20150101
        And verify that status file is generated in /tmp/92_custom_timestamps/db_dumps/20150101

    Scenario: 93 Writable Report/Status Directory Full Backup and Restore with no write access -u option
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -u /tmp/custom_timestamps -e -a -t 20160101010101"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should not print "gp-r" to stdout
        And gpdbrestore should not print "--status=" to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb93" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb93" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And the user runs command "chmod -R 777 /tmp/custom_timestamps/db_dumps"

    @nbupartII
    @ddpartII
    Scenario: 94 Filtered Full Backup with Partition Table
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_part_table"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.heap_table" in "bkdb94"
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb94" with data
        And verify that the data of "9" tables in "bkdb94" is validated after restore

    @nbupartIII
    @ddpartII
    Scenario: 95 Filtered Incremental Backup with Partition Table
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.ao_part_table"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.heap_table" in "bkdb95"
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb95" with data
        And verify that the data of "9" tables in "bkdb95" is validated after restore

    @nbupartIII
    @ddpartII
    Scenario: 96 gpdbrestore runs ANALYZE on restored table only
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb96"
        And schema "testschema" exists in "bkdb96"
        And there is a "heap" table "public.heap_table" in "bkdb96" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb96" with data
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb96" with data
        And verify that the restored table "public.ao_index_table" in database "bkdb96" is analyzed
        And verify that the table "public.heap_table" in database "bkdb96" is not analyzed
        When the user runs gpdbrestore without -e with the stored timestamp and options "-S public"
        And verify that the restored table "public.heap_table" in database "bkdb96" is analyzed
        And verify that the table "testschema.heap_table" in database "bkdb96" is not analyzed

    @nbupartIII
    @ddpartII
    Scenario: 97 Full Backup with multiple -S option and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "bkdb97"
        And verify that there is no table "testschema.heap_table" in "bkdb97"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb97" with data

    @nbupartIII
    @ddpartII
    Scenario: 98 Full Backup with option -S and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "bkdb98"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb98" with data

    @nbupartIII
    @ddpartIII
    Scenario: 99 Full Backup with option -s and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb99" with data
        And verify that there is no table "schema_ao.ao_part_table" in "bkdb99"

    @nbupartIII
    @ddpartIII
    Scenario: 100 Full Backup with option --exclude-schema-file and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb100" with data
        And verify that there is no table "schema_ao.ao_part_table" in "bkdb100"
        And verify that there is no table "testschema.heap_table" in "bkdb100"

    @nbupartIII
    @ddpartIII
    Scenario: 101 Full Backup with option --schema-file and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb101" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb101" with data
        And verify that there is no table "testschema.heap_table" in "bkdb101"

    @nbupartIII
    @ddpartIII
    Scenario: 106 Full Backup and Restore with option --change-schema
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb106"
        And schema "schema_ao, schema_new" exists in "bkdb106"
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb106" with data
        When the user runs gpdbrestore without -e with the stored timestamp and options "--change-schema=schema_new --table-file 106_include_file"
        Then gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new.heap_table" of "heap" type in "bkdb106" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new.ao_part_table" of "ao" type in "bkdb106" with same data as table "schema_ao.ao_part_table"

    Scenario: 107 Incremental Backup and Restore with option --change-schema
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb107"
        And schema "schema_ao, schema_new" exists in "bkdb107"
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb107" with data
        When the user runs gpdbrestore without -e with the stored timestamp and options "--change-schema=schema_new --table-file 107_include_file"
        Then gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new.heap_table" of "heap" type in "bkdb107" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new.ao_part_table" of "ao" type in "bkdb107" with same data as table "schema_ao.ao_part_table"

    Scenario: 108 Full backup and restore with statistics
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--restore-stats"
        Then gpdbrestore should return a return code of 0
        And verify that the restored table "public.heap_table" in database "bkdb108" is analyzed
        And verify that the restored table "public.ao_part_table" in database "bkdb108" is analyzed
        And database "bkdb108" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb108" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb108" with data
        When the user runs gpdbrestore -e with the stored timestamp and options "--restore-stats only"
        Then gpdbrestore should return a return code of 2
        When the user runs gpdbrestore without -e with the stored timestamp and options "--restore-stats only"
        Then gpdbrestore should return a return code of 0
        And verify that the restored table "public.heap_table" in database "bkdb108" is analyzed
        And verify that the restored table "public.ao_part_table" in database "bkdb108" is analyzed

    @nbupartIII
    @ddpartIII
    Scenario: 109 Backup and restore with statistics and table filters
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.heap_index_table --noanalyze"
        Then gpdbrestore should return a return code of 0
        When the user runs gpdbrestore without -e with the stored timestamp and options "--restore-stats -T public.heap_table"
        Then gpdbrestore should return a return code of 0
        And verify that the table "public.heap_index_table" in database "bkdb109" is not analyzed
        And verify that the restored table "public.heap_table" in database "bkdb109" is analyzed

    Scenario: 110 Restoring a nonexistent table should fail with clear error message
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.heap_table2 -q"
        Then gpdbrestore should return a return code of 2
        Then gpdbrestore should print "Tables \[\'public.heap_table2\'\]" to stdout
        Then gpdbrestore should not print "Issue with 'ANALYZE' of restored table 'public.heap_table2' in 'bkdb110' database" to stdout

    @nbupartIII
    @ddpartIII
    Scenario: 111 Full Backup with option --schema-file with prefix option and Restore
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb111" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb111" with data
        And verify that there is no table "testschema.heap_table" in "bkdb111"

    Scenario: 112 Simple Full Backup with AO/CO statistics w/ filter
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb112" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb112" for table "public.ao_table"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_table"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb112" for table "public.ao_index_table"
        And verify that there are "4380" tuples in "bkdb112" for table "public.ao_table"

    @nbupartIII
    @ddpartIII
    Scenario: 113 Simple Full Backup with AO/CO statistics w/ filter schema
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_table"
        And verify that there are "0" tuples in "bkdb113" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "schema_ao.ao_part_table"
        And verify that there are "0" tuples in "bkdb113" for table "testschema.ao_foo"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-S schema_ao -S testschema"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_table"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_1"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_2"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_3"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_1"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_2"
        And verify that there are "730" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_3"
        And verify that there are "4380" tuples in "bkdb113" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "schema_ao.ao_part_table"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-S schema_ao -S testschema --truncate"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "public.ao_table"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_1"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_2"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p1_2_prt_3"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_1"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_2"
        And verify that there are "365" tuples in "bkdb113" for table "testschema.ao_foo_1_prt_p2_2_prt_3"
        And verify that there are "2190" tuples in "bkdb113" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb113" for table "schema_ao.ao_part_table"

    @nbupartIII
    @ddpartIII
    Scenario: 114 Restore with --redirect option should not rely on existance of dumped database
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect=bkdb114"
        Then gpdbrestore should return a return code of 0
        And the database "bkdb114" does not exist

    Scenario: 115 Database owner can be assigned to role containing special characters
        Given the old timestamps are read from json
        And the backup test is initialized with database "bkdb115"
        When the user runs "psql -c 'CREATE ROLE "Foo%user"' -d bkdb115"
        And the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the owner of "bkdb115" is "Foo%user"
        And database "bkdb115" is dropped and recreated
        When the user runs "psql -c 'DROP ROLE "Foo%user"' -d bkdb115"
        Then psql should return a return code of 0

    Scenario: 116 pg_temp should be ignored from gpcrondump --table_file option and -t option when given
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there are "2190" tuples in "bkdb116" for table "public.foo4"

    @nbupartIII
    @ddpartIII
    Scenario: 117 Schema level restore with gpdbrestore -S option for views, sequences, and functions
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-S s1"
        Then gpdbrestore should return a return code of 0
        And verify that sequence "id_seq" exists in schema "s1" and database "schema_level_test_db"
        And verify that view "v1" exists in schema "s1" and database "schema_level_test_db"
        And verify that function "increment(integer)" exists in schema "s1" and database "schema_level_test_db"
        And verify that table "apples" exists in schema "s1" and database "schema_level_test_db"
        And the user runs command "dropdb schema_level_test_db"

    Scenario: 118 Backup a database with database-specific configuration
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that "search_path=daisy" appears in the datconfig for database "bkdb118"
        And verify that "appendonly=true" appears in the datconfig for database "bkdb118"
        And verify that "blocksize=65536" appears in the datconfig for database "bkdb118"

    @ddpartIII
    @skip_for_gpdb_43
    Scenario: 119 Backup database grants
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that "user_grant=C/" appears in the datacl for database "bkdb119"
        And the user runs "psql -c 'DROP ROLE user_grant' bkdb11"

    Scenario: 120 Simple full backup and restore with special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        And the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 121 gpcrondump with -T option where table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "121_ao" that there is a "ao" table "$SP_CHAR_SCHEMA.$SP_CHAR_AO" in "$SP_CHAR_DB" with data
        And verify with backedup file "121_heap" that there is a "heap" table "$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" with data
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And the user runs command "dropdb "$SP_CHAR_DB""

    @nbupartIII
    @ddpartIII
    Scenario: 122 gpcrondump with --exclude-table-file option where table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "122_ao" that there is a "ao" table "$SP_CHAR_SCHEMA.$SP_CHAR_AO" in "$SP_CHAR_DB" with data
        And verify with backedup file "122_heap" that there is a "heap" table "$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" with data
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 123 gpcrondump with --table-file option where table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs gpdbrestore without -e with the stored timestamp and options " "
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "123_ao" that there is a "ao" table "$SP_CHAR_SCHEMA.$SP_CHAR_AO" in "$SP_CHAR_DB" with data
        And verify with backedup file "123_heap" that there is a "heap" table "$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" with data
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 124 gpcrondump with -t option where table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs gpdbrestore without -e with the stored timestamp and options " "
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "124_ao" that there is a "ao" table "$SP_CHAR_SCHEMA.$SP_CHAR_AO" in "$SP_CHAR_DB" with data
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And the user runs command "dropdb "$SP_CHAR_DB""

    @nbupartIII
    Scenario: 125 gpcrondump with --schema-file option when schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        And verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/125_special_table_data.ans" are identical
        When the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 126 gpcrondump with -s option when schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        And verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/126_special_table_data.ans" are identical
        When the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 127 gpcrondump with --exclude-schema-file option when schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        Then verify that there is no table "$SP_CHAR_AO" in "$SP_CHAR_DB"
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And verify that there is no table "$SP_CHAR_HEAP" in "$SP_CHAR_DB"
        When the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 128 gpcrondump with -S option when schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        Then verify that there is no table "$SP_CHAR_AO" in "$SP_CHAR_DB"
        And verify that there is no table "$SP_CHAR_CO" in "$SP_CHAR_DB"
        And verify that there is no table "$SP_CHAR_HEAP" in "$SP_CHAR_DB"
        When the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 130 Gpdbrestore with -T, --truncate, and --change-schema options when table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/add_schema.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T "$SP_CHAR_SCHEMA"."$SP_CHAR_AO" --change-schema="$SP_CHAR_SCHEMA" -S "$SP_CHAR_SCHEMA2""
        Then gpdbrestore should return a return code of 2
        And gpcrondump should print "-S option cannot be used with --change-schema option" to stdout
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T "$SP_CHAR_SCHEMA"."$SP_CHAR_AO" --change-schema="$SP_CHAR_SCHEMA2" --truncate"
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f  psql -c """select * from \"$SP_CHAR_SCHEMA2\".\"$SP_CHAR_AO\" order by 1""" -d "$SP_CHAR_DB"  > /tmp/table_data.out"
        And verify that the contents of the files "/tmp/130_table_data.ans" and "/tmp/table_data.out" are identical
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T "$SP_CHAR_SCHEMA"."$SP_CHAR_AO" --truncate"
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f  psql -c """select * from \"$SP_CHAR_SCHEMA\".\"$SP_CHAR_AO\" order by 1""" -d "$SP_CHAR_DB"  > /tmp/table_data.out"
        Then verify that the contents of the files "/tmp/130_table_data.ans" and "/tmp/table_data.out" are identical
        When the user runs command "dropdb "$SP_CHAR_DB""

    @nbupartIII
    @ddpartIII
    Scenario: 131 gpcrondump with --incremental option when table name, schema name and database name contains special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/131_special_table_data.ans" are identical
        When the user runs command "dropdb "$SP_CHAR_DB""

    @nbupartIII
    @ddpartIII
    Scenario: 132 gpdbrestore with --redirect option with special db name, and all table name, schema name and database name contain special character
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/drop_special_database.sql template1"
        When the user runs gpdbrestore without -e with the stored timestamp and options "--redirect "$SP_CHAR_DB""
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/132_special_table_data.ans" are identical
        When the user runs command "dropdb "$SP_CHAR_DB""

    Scenario: 133 gpdbrestore, -S option, -S truncate option schema level restore with special chars in schema name
        Given the old timestamps are read from json
        And the backup test is initialized for special characters
        When the user runs gpdbrestore -e with the stored timestamp and options "-S "$SP_CHAR_SCHEMA""
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/133_special_table_data.ans" are identical
        When the user runs gpdbrestore without -e with the stored timestamp and options "-S "$SP_CHAR_SCHEMA" --truncate"
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/133_special_table_data.ans" are identical

    @skip_for_gpdb_43
    Scenario: 136 Backup and restore CAST, with associated function in restored schema, base_file_name=dump_func_name
        Given the old timestamps are read from json
        # No filter
        When the user runs gpdbrestore -e with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that a cast exists in "bkdb136" in schema "public"
        # Table filter
        And the user runs gpdbrestore -e with the stored timestamp and options "-T public.heap_table"
        And gpdbrestore should return a return code of 0
        Then verify that a cast exists in "bkdb136" in schema "public"
        # Schema filter
        And the user runs gpdbrestore -e with the stored timestamp and options "-S public"
        And gpdbrestore should return a return code of 0
        Then verify that a cast exists in "bkdb136" in schema "public"
        # Change schema filter
        And database "bkdb136" is dropped and recreated
        And schema "newschema" exists in "bkdb136"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.heap_table --change-schema newschema"
        And gpdbrestore should return a return code of 0
        Then verify that a cast exists in "bkdb136" in schema "newschema"

    @skip_for_gpdb_43
    Scenario: 137 Backup and restore CAST, with associated function in non-restored schema
        Given the old timestamps are read from json
        # Table filter
        When the user runs gpdbrestore -e with the stored timestamp and options "-T testschema.heap_table"
        Then gpdbrestore should return a return code of 0
        Then verify that a cast does not exist in "bkdb137" in schema "testschema"
        # Schema filter
        And the user runs gpdbrestore -e with the stored timestamp and options "-S testschema"
        And gpdbrestore should return a return code of 0
        Then verify that a cast does not exist in "bkdb137" in schema "testschema"
        # Change schema filter
        And database "bkdb137" is dropped and recreated
        Given schema "newschema" exists in "bkdb137"
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T testschema.heap_table --change-schema newschema"
        And gpdbrestore should return a return code of 0
        Then verify that a cast does not exist in "bkdb137" in schema "newschema"

    Scenario: 138 Incremental backup and restore with restore to primary and mirror after failover
        Given the old timestamps are read from json
        When the user runs "gpdbrestore -e -s bkdb138 -a"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_table" in "bkdb138"
        And verify that there is a "heap" table "public.heap_table" in "bkdb138"
        And verify that the tuple count of all appendonly tables are consistent in "bkdb138"

        And user kills a primary postmaster process
        And user can start transactions
        When the user runs "gpdbrestore -e -s bkdb138-2 -a"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_table" in "bkdb138-2"
        And verify that there is a "heap" table "public.heap_table" in "bkdb138-2"
        And verify that the tuple count of all appendonly tables are consistent in "bkdb138-2"

        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0

    Scenario: 141 Backup with all GUC (system settings) set to defaults will succeed
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0

        # verify that gucs are set to previous defaults
        And verify that "appendonly=true" appears in the datconfig for database "bkdb141"
        And verify that "orientation=row" appears in the datconfig for database "bkdb141"
        And verify that "blocksize=65536" appears in the datconfig for database "bkdb141"
        And verify that "checksum=false" appears in the datconfig for database "bkdb141"
        And verify that "compresslevel=4" appears in the datconfig for database "bkdb141"
        And verify that "compresstype=none" appears in the datconfig for database "bkdb141"

        # verify that my_default table has default gucs
        When execute following sql in db "bkdb141" and store result in the context
            """
            select relstorage,
            reloptions,compresstype,columnstore,compresslevel,checksum from
            pg_class c , pg_appendonly a where c.oid=a.relid and
            c.relname='default_guc'
            """
        Then validate that following rows are in the stored rows
          | relstorage | reloptions                                       | compresstype | columnstore | compresslevel | checksum |
          | a          | {appendonly=true,blocksize=65536,checksum=false} |              | f           | 0             | f        |

        # verify that my_role_table has gucs from role
        When execute following sql in db "bkdb141" and store result in the context
            """
            select relstorage,
            reloptions,compresstype,columnstore,compresslevel,checksum from
            pg_class c , pg_appendonly a where c.oid=a.relid and
            c.relname='role_guc_table'
            """
        Then validate that following rows are in the stored rows
          | relstorage | reloptions                                             | compresstype | columnstore | compresslevel | checksum |
          | c          | {appendonly=true,compresstype=zlib,orientation=column} | zlib         | t           | 1             | t        |

        # verify that session_gucs has gucs from session
        When execute following sql in db "bkdb141" and store result in the context
            """
            select relstorage,
            reloptions,compresstype,columnstore,compresslevel,checksum from
            pg_class c , pg_appendonly a where c.oid=a.relid and
            c.relname='session_guc_table'
            """
        Then validate that following rows are in the stored rows
          | relstorage | reloptions                           | compresstype | columnstore | compresslevel | checksum |
          | c          | {appendonly=true,orientation=column} |              | t           | 0             | t        |


    Scenario: 142 gpcrondump -u option with include table filtering
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-u /tmp"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb142" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb142"

    Scenario: 143 gpcrondump -u option with exclude table filtering
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-u /tmp"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "1" tables in "bkdb143" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb143"

    Scenario: 144 gpdbrestore -u option with include table filtering
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "-u /tmp -T public.ao_table"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "1" tables in "bkdb144" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb144"

    @nbupartIII
    Scenario: 145 gpcrondump with -u and --prefix option
        Given the old timestamps are read from json
        When the user runs gpdbrestore -e with the stored timestamp and options "--prefix=foo --redirect=bkdb145-2 -u /tmp"
        Then gpdbrestore should return a return code of 0
        And there should be dump files under "/tmp" with prefix "foo"
        And check that there is a "heap" table "public.heap_table" in "bkdb145-2" with same data from "bkdb145"
        And check that there is a "ao" table "public.ao_part_table" in "bkdb145-2" with same data from "bkdb145"

    @ddpartIII
    @ddonly
    Scenario: 146 Full backup and restore with remote DataDomain replication using gpcrondump
        Given the old timestamps are read from json
        When the remote backup sets are restored to the local storage unit
        And the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb146" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb146"

    @ddpartIII
    @ddonly
    Scenario: 147 Full backup and restore with remote DataDomain replication using gp_mfr
        Given the old timestamps are read from json
        When the remote backup sets are restored to the local storage unit
        And the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb147" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb147"
        And the backup sets on the "remote" storage unit are deleted using gp_mfr

    @ddpartIII
    @ddonly
    Scenario: 148 Incremental backup and restore with remote DataDomain replication using gpcrondump
        Given the old timestamps are read from json
        When the remote backup sets are restored to the local storage unit
        And the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "3" tables in "bkdb148" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb148"

    @ddpartIII
    @ddonly
    Scenario: 149 Incremental backup and restore with remote DataDomain replication using gp_mfr
        Given the old timestamps are read from json
        When the remote backup sets are restored to the local storage unit
        And the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        Then verify that the data of "3" tables in "bkdb149" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb149"
        And the backup sets on the "remote" storage unit are deleted using gp_mfr

    @ddonly
    @ddboostsetup
    Scenario: 150 Cleanup DDBoost dump directories
        Given the DDBoost dump directory is deleted for the "local" storage unit
        Given the DDBoost dump directory is deleted for the "remote" storage unit
