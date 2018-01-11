@backup_and_restore_backups
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
        Given the backup test is initialized with database "bkdb1"
        And there is a "ao" table "public.ao_table" in "bkdb1" with data
        When the user runs "gpcrondump -a -x bkdb1"
        Then gpcrondump should return a return code of 0
        When the "public.ao_table" is recreated with same data in "bkdb1"
        And the user runs "gpcrondump -a --incremental -x bkdb1"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "public.ao_table" is marked as dirty in dirty_list file

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
        Given the backup test is initialized with database "bkdb2"
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a "ao" table "public.ao_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb2" with data
        And there is a "ao" table "public.ao_index_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb2" with data
        And there is a "ao" partition table "public.ao_part_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "co" table "public.co_table" in "bkdb2" with data
        And there is a "co" table "public.co_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "co" table "public.co_index_table" in "bkdb2" with data
        And there is a "co" table "public.co_index_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb2" with data
        And there is a "co" partition table "public.co_part_table_comp" with compression "zlib" in "bkdb2" with data
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb2" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb2" with data
        And there is a mixed storage partition table "part_mixed_1" in "bkdb2" with data
        Given the user runs "echo > /tmp/backup_gpfdist_dummy"
        And the user runs "gpfdist -p 8098 -d /tmp &"
        And there is a partition table "part_external" has external partitions of gpfdist with file "backup_gpfdist_dummy" on port "8098" in "bkdb2" with data
        Then data for partition table "part_mixed_1" with partition level "1" is distributed across all segments on "bkdb2"
        And data for partition table "part_external" with partition level "0" is distributed across all segments on "bkdb2"
        When the user runs "gpcrondump -a -x bkdb2"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print the correct disk space check message
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "last_operation" files are generated under " " for stored "full" timestamp
        When partition "1" of partition table "ao_part_table, co_part_table_comp, part_mixed_1" is assumed to be in dirty state in "bkdb2" in schema "public"
        And partition "1" in partition level "0" of partition table "part_external" is assumed to be in dirty state in "bkdb2" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb2"
        And the temp files "dirty_backup_list" are removed from the system
        And the user runs "gpcrondump -a --incremental -x bkdb2"
        Then gpcrondump should return a return code of 0
        And the temp files "dirty_backup_list" are not created in the system
        And gpcrondump should not print "Validating disk space" to stdout
        And the timestamp from gpcrondump is stored
        And verify that the incremental file has the stored timestamp
        And the state files are generated under " " for stored "incr" timestamp
        And the "last_operation" files are generated under " " for stored "incr" timestamp
        And the subdir from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb2" is saved for verification

    Scenario: 4 gpdbrestore with -R for full dump
        Given the backup test is initialized with database "bkdb4"
        And there is a "ao" table "public.ao_table" in "bkdb4" with data
        And there is a "heap" table "public.heap_table" in "bkdb4" with data
        When the user runs "gpcrondump -a -x bkdb4 -u /tmp/4"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And all the data from "bkdb4" is saved for verification
        And the subdir from gpcrondump is stored
        And database "bkdb4" is dropped and recreated
        And all the data from the remote segments in "bkdb4" are stored in path "/tmp/4" for "full"

    Scenario: 5 gpdbrestore with -R for incremental dump
        Given the backup test is initialized with database "bkdb5"
        And there is a "ao" table "public.ao_table" in "bkdb5" with data
        And there is a "heap" table "public.heap_table" in "bkdb5" with data
        When the user runs "gpcrondump -a -x bkdb5 -u /tmp/5"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        When table "public.ao_table" is assumed to be in dirty state in "bkdb5"
        And the user runs "gpcrondump -a --incremental -x bkdb5 -u /tmp/5"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that the "report" file in "/tmp/5" dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under "/tmp/5"
        And all the data from the remote segments in "bkdb5" are stored in path "/tmp/5" for "inc"
        And all files for full backup have been removed in path "/tmp/5"

    @nbupartI
    @ddpartI
    Scenario: 5a Full Backup and Restore
        Given the backup test is initialized with database "bkdb5a"
        And there is a "heap" table "public.heap_table" in "bkdb5a" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb5a" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb5a" exists for validation
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/add_rules_indexes_constraints_triggers.sql bkdb5a"
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb5a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartI
    @ddpartI
    Scenario: 6 Metadata-only restore
        Given the backup test is initialized with database "bkdb6"
        And schema "schema_heap" exists in "bkdb6"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb6" with data
        When the user runs "gpcrondump -a -x bkdb6"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the schemas "schema_heap" do not exist in "bkdb6"

    @nbupartI
    @ddpartI
    Scenario: 7 Metadata-only restore with global objects (-G)
        Given the backup test is initialized with database "bkdb7"
        And schema "schema_heap" exists in "bkdb7"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb7" with data
        And the user runs "psql -c 'CREATE ROLE "foo%userWITHCAPS"' bkdb7"
        When the user runs "gpcrondump -a -x bkdb7 -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -c 'DROP ROLE "foo%userWITHCAPS"' bkdb7"
        And the schemas "schema_heap" do not exist in "bkdb7"

    @ddpartI
    Scenario: 8 gpdbrestore -L with Full Backup
        Given the backup test is initialized with database "bkdb8"
        And there is a "heap" table "public.heap_table" in "bkdb8" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb8" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb8" exists for validation
        When the user runs "gpcrondump -a -x bkdb8"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartI
    @ddpartI
    Scenario: 11 Backup and restore with -G only
        Given the backup test is initialized with database "bkdb11"
        And the user runs "psql -c 'CREATE ROLE foo_user' bkdb11"
        And verify that a role "foo_user" exists in database "bkdb11"
        When the user runs "gpcrondump -a -x bkdb11 -G"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And the user runs "psql -c 'DROP ROLE foo_user' bkdb11"

    @valgrind
    Scenario: 12 Valgrind test of gp_restore for incremental backup
        Given the backup test is initialized with database "bkdb12"
        And there is a "heap" table "public.heap_table" in "bkdb12" with data
        And there is a "ao" table "public.ao_table" in "bkdb12" with data
        And a backup file of tables "public.heap_table, public.ao_table" in "bkdb12" exists for validation
        When the user runs "gpcrondump -a -x bkdb12"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb12"
        And the user runs "gpcrondump -a -x bkdb12 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @valgrind
    Scenario: 13 Valgrind test of gp_restore_agent for incremental backup
        Given the backup test is initialized with database "bkdb13"
        And there is a "heap" table "public.heap_table" in "bkdb13" with data
        And there is a "ao" table "public.ao_table" in "bkdb13" with data
        And a backup file of tables "public.heap_table, public.ao_table" in "bkdb13" exists for validation
        When the user runs "gpcrondump -a -x bkdb13"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb13"
        And the user runs "gpcrondump -a -x bkdb13 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartI
    @ddpartI
    @skip_for_gpdb_43
    Scenario: 14 Full Backup with option -t and Restore
        Given the backup test is initialized with database "bkdb14"
        And there is a "heap" table "public.heap_table" in "bkdb14" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb14" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb14" exists for validation
        And the temp files "include_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x bkdb14 -t public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "include_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And "table" file should be created under " "

    @nbupartI
    @ddpartI
    @skip_for_gpdb_43
    Scenario: 15 Full Backup with option -T and Restore
        Given the backup test is initialized with database "bkdb15"
        And there is a "heap" table "public.heap_table" in "bkdb15" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb15" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb15" exists for validation
        And the temp files "exclude_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x bkdb15 -T public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "exclude_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored
        And "table" file should be created under " "

    @nbupartI
    @ddpartI
    Scenario: 16 Full Backup with option --exclude-table-file and Restore
        Given the backup test is initialized with database "bkdb16"
        And there is a "heap" table "public.heap_table" in "bkdb16" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb16" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb16" with data
        And a backup file of tables "public.co_part_table" in "bkdb16" exists for validation
        And there is a file "exclude_file" with tables "public.heap_table|public.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb16 --exclude-table-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartI
    @ddpartI
    Scenario: 17 Full Backup with option --table-file and Restore
        Given the backup test is initialized with database "bkdb17"
        And there is a "heap" table "public.heap_table" in "bkdb17" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb17" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb17" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb17" exists for validation
        And there is a file "include_file" with tables "public.heap_table|public.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb17 --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    Scenario: 19 Simple Plan File Test
        Given the backup test is initialized with database "bkdb19"
        And there is a "heap" table "public.heap_table" in "bkdb19" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb19" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb19" with data
        And there is a "heap" partition table "public.heap_part_table_comp" with compression "zlib" in "bkdb19" with data
        And there is a "ao" table "public.ao_table" in "bkdb19" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb19" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb19" with data
        And there is a "ao" partition table "public.ao_part_table_comp" with compression "zlib" in "bkdb19" with data
        And there is a "co" table "public.co_table" in "bkdb19" with data
        And there is a "co" table "public.co_index_table" in "bkdb19" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb19" with data
        And there is a "co" partition table "public.co_part_table_comp" with compression "zlib" in "bkdb19" with data
        When the user runs "gpcrondump -a -x bkdb19"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "19" is labeled "ts0"
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb19" in schema "public"
        And the user runs "gpcrondump -a -x bkdb19 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp for scenario "19" is labeled "ts1"
        And "dirty_list" file should be created under " "
        And table "public.co_table" is assumed to be in dirty state in "bkdb19"
        And partition "1" of partition table "co_part_table_comp" is assumed to be in dirty state in "bkdb19" in schema "public"
        And the user runs "gpcrondump -a -x bkdb19 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp for scenario "19" is labeled "ts2"
        And "dirty_list" file should be created under " "

    @nbupartI
    @ddpartI
    Scenario: 20 No plan file generated
        Given the backup test is initialized with database "bkdb20"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb20" with data
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb20" in schema "public"
        When the user runs "gpcrondump -a -x bkdb20"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 21 Schema only restore of incremental backup
        Given the backup test is initialized with database "bkdb21"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb21" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb21" with data
        When the user runs "gpcrondump -a -x bkdb21"
        And gpcrondump should return a return code of 0
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb21" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb21"
        And the user runs "gpcrondump -a -x bkdb21 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartI
    @ddpartI
    Scenario: 22 Simple Incremental Backup with AO/CO statistics w/ filter
        Given the backup test is initialized with database "bkdb22"
        And there is a "ao" table "public.ao_table" in "bkdb22" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb22" with data
        When the user runs "gpcrondump -a -x bkdb22"
        Then gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb22"
        And the user runs "gpcrondump -a --incremental -x bkdb22"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 23 Simple Incremental Backup with TRUNCATE
        Given the backup test is initialized with database "bkdb23"
        And schema "testschema" exists in "bkdb23"
        And there is a "ao" table "testschema.ao_table" in "bkdb23" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb23" with data
        And there is a "co" table "testschema.co_table" in "bkdb23" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb23" with data
        And there is a list to store the incremental backup timestamps
        When the user truncates "testschema.ao_table" tables in "bkdb23"
        And the user truncates "testschema.co_table" tables in "bkdb23"
        And the numbers "1" to "10000" are inserted into "testschema.ao_table" tables in "bkdb23"
        And the numbers "1" to "10000" are inserted into "testschema.co_table" tables in "bkdb23"
        And the user runs "gpcrondump -a -x bkdb23"
        And gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the user truncates "testschema.ao_part_table" tables in "bkdb23"
        And the partition table "testschema.ao_part_table" in "bkdb23" is populated with similar data
        And the user truncates "testschema.co_table" tables in "bkdb23"
        And the numbers "1" to "10000" are inserted into "testschema.co_table" tables in "bkdb23"
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb23"
        And the user runs "gpcrondump -a --incremental -x bkdb23"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user truncates "testschema.ao_part_table" tables in "bkdb23"
        And the partition table "testschema.ao_part_table" in "bkdb23" is populated with similar data
        And the user runs "gpcrondump -a --incremental -x bkdb23"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Then verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb23" is saved for verification

    Scenario: 24 Simple Incremental Backup to test ADD COLUMN
        Given the backup test is initialized with database "bkdb24"
        And schema "testschema" exists in "bkdb24"
        And there is a "ao" table "testschema.ao_table" in "bkdb24" with data
        And there is a "ao" table "testschema.ao_index_table" in "bkdb24" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb24" with data
        And there is a "co" table "testschema.co_table" in "bkdb24" with data
        And there is a "co" table "testschema.co_index_table" in "bkdb24" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb24" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb24"
        And gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the user adds column "foo" with type "int" and default "0" to "testschema.ao_table" table in "bkdb24"
        And the user adds column "foo" with type "int" and default "0" to "testschema.co_table" table in "bkdb24"
        And the user adds column "foo" with type "int" and default "0" to "testschema.ao_part_table" table in "bkdb24"
        And the user adds column "foo" with type "int" and default "0" to "testschema.co_part_table" table in "bkdb24"
        And the user runs "gpcrondump -a --incremental -x bkdb24"
        And gpcrondump should return a return code of 0
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And all the data from "bkdb24" is saved for verification

    @nbupartI
    @ddpartI
    Scenario: 25 Non compressed incremental backup
        Given the backup test is initialized with database "bkdb25"
        And schema "testschema" exists in "bkdb25"
        And there is a "heap" table "testschema.heap_table" in "bkdb25" with data
        And there is a "ao" table "testschema.ao_table" in "bkdb25" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb25" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb25 -z"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "bkdb25" in schema "testschema"
        And table "testschema.heap_table" is dropped in "bkdb25"
        And the user runs "gpcrondump -a -x bkdb25 --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb25"
        And the user runs "gpcrondump -a -x bkdb25 --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb25" is saved for verification

    Scenario: 26 Rollback Insert
        Given the backup test is initialized with database "bkdb26"
        And schema "testschema" exists in "bkdb26"
        And there is a "ao" table "testschema.ao_table" in "bkdb26" with data
        And there is a "co" table "testschema.co_table" in "bkdb26" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb26"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "26" is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And an insert on "testschema.ao_table" in "bkdb26" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb26"
        And the user runs "gpcrondump -a --incremental -x bkdb26"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "26" is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb26" is saved for verification

    Scenario: 27 Rollback Truncate Table
        Given the backup test is initialized with database "bkdb27"
        And schema "testschema" exists in "bkdb27"
        And there is a "ao" table "testschema.ao_table" in "bkdb27" with data
        And there is a "co" table "testschema.co_table" in "bkdb27" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb27"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "27" is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And a truncate on "testschema.ao_table" in "bkdb27" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb27"
        And the user runs "gpcrondump -a --incremental -x bkdb27"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "27" is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb27" is saved for verification

    Scenario: 28 Rollback Alter table
        Given the backup test is initialized with database "bkdb28"
        And schema "testschema" exists in "bkdb28"
        And there is a "ao" table "testschema.ao_table" in "bkdb28" with data
        And there is a "co" table "testschema.co_table" in "bkdb28" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb28"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "28" is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And an alter on "testschema.ao_table" in "bkdb28" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb28"
        And the user runs "gpcrondump -a --incremental -x bkdb28"
        And gpcrondump should return a return code of 0
        And the timestamp for scenario "28" is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb28" is saved for verification

    @ddpartI
    Scenario: 29 Verify gpdbrestore -s option works with full backup
        Given the backup test is initialized with database "bkdb29"
        And database "bkdb29-2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb29" with data
        And there is a "co" table "public.co_table" in "bkdb29" with data
        And there is a "ao" table "public.ao_table" in "bkdb29-2" with data
        And there is a "co" table "public.co_table" in "bkdb29-2" with data
        When the user runs "gpcrondump -a -x bkdb29"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb29" is saved for verification
        And the user runs "gpcrondump -a -x bkdb29-2"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the database "bkdb29-2" does not exist

    @ddpartI
    Scenario: 30 Verify gpdbrestore -s option works with incremental backup
        Given the backup test is initialized with database "bkdb30"
        And database "bkdb30-2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb30" with data
        And there is a "co" table "public.co_table" in "bkdb30" with data
        And there is a "ao" table "public.ao_table" in "bkdb30-2" with data
        And there is a "co" table "public.co_table" in "bkdb30-2" with data
        When the user runs "gpcrondump -a -x bkdb30"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a -x bkdb30-2"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb30"
        And the user runs "gpcrondump -a -x bkdb30 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpcrondump -a -x bkdb30-2 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb30" is saved for verification
        And the database "bkdb30-2" does not exist

    @nbupartI
    Scenario: 31 gpdbrestore -u option with full backup
        Given the backup test is initialized with database "bkdb31"
        And there is a "ao" table "public.ao_table" in "bkdb31" with data
        And there is a "co" table "public.co_table" in "bkdb31" with data
        When the user runs "gpcrondump -a -x bkdb31 -u /tmp"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb31" is saved for verification

    @nbupartI
    Scenario: 32 gpdbrestore -u option with incremental backup
        Given the backup test is initialized with database "bkdb32"
        And there is a "ao" table "public.ao_table" in "bkdb32" with data
        And there is a "co" table "public.co_table" in "bkdb32" with data
        When the user runs "gpcrondump -a -x bkdb32 -u /tmp"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb32"
        When the user runs "gpcrondump -a -x bkdb32 -u /tmp --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb32" is saved for verification

    Scenario: 33 gpcrondump -x with multiple databases
        Given the backup test is initialized with database "bkdb33"
        And database "bkdb33-2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb33" with data
        And there is a "co" table "public.co_table" in "bkdb33" with data
        And there is a "ao" table "public.ao_table" in "bkdb33-2" with data
        And there is a "co" table "public.co_table" in "bkdb33-2" with data
        When the user runs "gpcrondump -a -x bkdb33 -x bkdb33-2"
        And the timestamp for database dumps "bkdb33, bkdb33-2" are stored
        And gpcrondump should return a return code of 0
        And all the data from "bkdb33" is saved for verification
        And all the data from "bkdb33-2" is saved for verification
        Then the dump timestamp for "bkdb33, bkdb33-2" are different

    @nbupartI
    @ddpartI
    Scenario: 34 gpdbrestore with --table-file option
        Given the backup test is initialized with database "bkdb34"
        And there is a "ao" table "public.ao_table" in "bkdb34" with data
        And there is a "co" table "public.co_table" in "bkdb34" with data
        And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
        When the user runs "gpcrondump -a -x bkdb34"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb34" is saved for verification

    @nbupartI
    @ddpartI
    Scenario: 35 Incremental restore with extra full backup
        Given the backup test is initialized with database "bkdb35"
        And there is a "heap" table "public.heap_table" in "bkdb35" with data
        And there is a "ao" table "public.ao_table" in "bkdb35" with data
        When the user runs "gpcrondump -a -x bkdb35"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb35"
        And the user runs "gpcrondump -a --incremental -x bkdb35"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb35" is saved for verification
        And the numbers "1" to "100" are inserted into "ao_table" tables in "bkdb35"
        When the user runs "gpcrondump -a -x bkdb35"
        And gpcrondump should return a return code of 0

    Scenario: 36 gpcrondump should not track external tables
        Given the backup test is initialized with database "bkdb36"
        And there is a "ao" table "public.ao_table" in "bkdb36" with data
        And there is a "co" table "public.co_table" in "bkdb36" with data
        And there is an external table "ext_tab" in "bkdb36" with data for file "/tmp/ext_tab"
        When the user runs "gpcrondump -a -x bkdb36"
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb36 --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb36" is saved for verification

    @nbupartI
    @ddpartI
    Scenario: 37 Full backup with -T option
        Given the database is running
        And the database "fullbkdb37" does not exist
        And database "fullbkdb37" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "fullbkdb37" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "fullbkdb37" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "fullbkdb37" with data
        When the user runs "gpcrondump -a -x fullbkdb37"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb37" is saved for verification

    @nbupartI
    @ddpartI
    Scenario: 38 gpdbrestore with -T option
        Given the backup test is initialized with database "bkdb38"
        And there is a "heap" table "public.heap_table" in "bkdb38" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb38" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb38" with data
        When the user runs "gpcrondump -a -x bkdb38"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb38" is saved for verification

    @nbupartI
    @ddpartI
    Scenario: 39 Full backup and restore with -T and --truncate
        Given the backup test is initialized with database "bkdb39"
        And there is a "heap" table "public.heap_table" in "bkdb39" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb39" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb39" with data
        When the user runs "gpcrondump -a -x bkdb39"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb39" is saved for verification

    Scenario: 40 Full backup and restore with -T and --truncate with dropped table
        Given the backup test is initialized with database "bkdb40"
        And there is a "heap" table "public.heap_table" in "bkdb40" with data
        When the user runs "gpcrondump -a -x bkdb40"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb40" is saved for verification
        And table "public.heap_table" is dropped in "bkdb40"

    @nbupartII
    @ddpartII
    Scenario: 41 Full backup -T with truncated table
        Given the backup test is initialized with database "bkdb41"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb41" with data
        When the user runs "gpcrondump -a -x bkdb41"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb41" is saved for verification

    Scenario: 42 Full backup -T with no schema name supplied
        Given the backup test is initialized with database "bkdb42"
        And there is a "ao" table "public.ao_index_table" in "bkdb42" with data
        When the user runs "gpcrondump -a -x bkdb42"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user truncates "public.ao_index_table" tables in "bkdb42"

    Scenario: 43 Full backup with gpdbrestore -T for DB with FUNCTION having DROP SQL
        Given the backup test is initialized with database "bkdb43"
        And there is a "ao" table "public.ao_index_table" in "bkdb43" with data
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/create_function_with_drop_table.sql bkdb43"
        When the user runs "gpcrondump -a -x bkdb43"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb43" is saved for verification
        When table "public.ao_index_table" is dropped in "bkdb43"

    @nbupartII
    @ddpartII
    Scenario: 44 Incremental restore with table filter
        Given the backup test is initialized with database "bkdb44"
        And there is a "heap" table "public.heap_table" in "bkdb44" with data
        And there is a "ao" table "public.ao_table" in "bkdb44" with data
        And there is a "co" table "public.co_table" in "bkdb44" with data
        When the user runs "gpcrondump -a -x bkdb44"
        And gpcrondump should return a return code of 0
        And table "ao_table" is assumed to be in dirty state in "bkdb44"
        And the user runs "gpcrondump -a --incremental -x bkdb44"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb44" is saved for verification

    Scenario: 45 Incremental restore with invalid table filter
        Given the backup test is initialized with database "bkdb45"
        And there is a "heap" table "public.heap_table" in "bkdb45" with data
        When the user runs "gpcrondump -a -x bkdb45"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb45"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb45" is saved for verification

    Scenario: 46 gpdbrestore -L with -u option
        Given the backup test is initialized with database "bkdb46"
        And there is a "heap" table "public.heap_table" in "bkdb46" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb46" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb46" exists for validation
        When the user runs "gpcrondump -a -x bkdb46 -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in "/tmp" dir contains "Backup Type: Full"

    Scenario: 47 gpdbrestore -b with -u option for Full timestamp
        Given the backup test is initialized with database "bkdb47"
        And there is a "heap" table "public.heap_table" in "bkdb47" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb47" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb47" with data
        When the user runs "gpcrondump -a -x bkdb47 -u /tmp/47"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb47" is saved for verification

    Scenario: 48 gpdbrestore with -s and -u options for full backup
        Given the backup test is initialized with database "bkdb48"
        And there is a "heap" table "public.heap_table" in "bkdb48" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb48" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb48" with data
        When the user runs "gpcrondump -a -x bkdb48 -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb48" is saved for verification

    Scenario: 49 gpdbrestore with -s and -u options for incremental backup
        Given the backup test is initialized with database "bkdb49"
        And there is a "heap" table "public.heap_table" in "bkdb49" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb49" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb49" with data
        When the user runs "gpcrondump -a -x bkdb49 -u /tmp"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb49"
        When the user runs "gpcrondump -a -x bkdb49 -u /tmp --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb49" is saved for verification

    @ddpartII
    Scenario: 50 gpdbrestore -b option should display the timestamps in sorted order
        Given the backup test is initialized with database "bkdb50"
        And there is a "heap" table "public.heap_table" in "bkdb50" with data
        When the user runs "gpcrondump -a -x bkdb50"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -x bkdb50 --incremental -a"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -x bkdb50 --incremental -a"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb50" is saved for verification

    Scenario: 51 gpdbrestore -R option should display the timestamps in sorted order
        Given the backup test is initialized with database "bkdb51"
        And there is a "heap" table "public.heap_table" in "bkdb51" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb51" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb51" with data
        When the user runs "gpcrondump -a -x bkdb51 -u /tmp"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "bkdb51"
        And the user runs "gpcrondump -x bkdb51 --incremental -u /tmp -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "bkdb51"
        And the user runs "gpcrondump -x bkdb51 --incremental -u /tmp -a"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        And all the data from "bkdb51" is saved for verification

    Scenario: 54 Test gpcrondump and gpdbrestore verbose option
        Given the backup test is initialized with database "bkdb54"
        And there is a "ao" table "public.ao_table" in "bkdb54" with data
        When the user runs "gpcrondump -a -x bkdb54 --verbose"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb54"
        And the user runs "gpcrondump -a -x bkdb54 --incremental --verbose"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb54" is saved for verification

    Scenario: 55 Incremental table filter gpdbrestore with different schema for same tablenames
        Given the backup test is initialized with database "bkdb55"
        And schema "testschema" exists in "bkdb55"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb55" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb55" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb55" with data
        And there is a "ao" partition table "testschema.ao_part_table1" in "bkdb55" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb55"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb55"
        Then data for partition table "testschema.ao_part_table" with partition level "1" is distributed across all segments on "bkdb55"
        Then data for partition table "testschema.ao_part_table1" with partition level "1" is distributed across all segments on "bkdb55"
        When the user runs "gpcrondump -a -x bkdb55"
        Then gpcrondump should return a return code of 0
        When table "public.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb55"
        And table "public.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb55"
        And table "testschema.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb55"
        And table "testschema.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb55"
        And all the data from "bkdb55" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb55"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    @ddpartII
    Scenario: 56 Incremental table filter gpdbrestore with noplan option
        Given the backup test is initialized with database "bkdb56"
        And there is a list to store the incremental backup timestamps
        And there is a "ao" partition table "public.ao_part_table" in "bkdb56" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb56" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb56"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb56"
        And verify that partitioned tables "ao_part_table" in "bkdb56" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "bkdb56" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb56"
        Then gpcrondump should return a return code of 0
        When all the data from "bkdb56" is saved for verification
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -a --incremental -x bkdb56"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list

    @nbupartII
    @ddpartII
    Scenario: 57 gpdbrestore list_backup option
        Given the backup test is initialized with database "bkdb57"
        And there is a "heap" table "public.heap_table" in "bkdb57" with data
        And there is a "ao" table "public.ao_table" in "bkdb57" with data
        And there is a "co" table "public.co_table" in "bkdb57" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb57"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_table" is assumed to be in dirty state in "bkdb57"
        When the user runs "gpcrondump -a --incremental -x bkdb57"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list

    Scenario: 58 gpdbrestore list_backup option with -T table filter
        Given the backup test is initialized with database "bkdb58"
        And there is a "heap" table "public.heap_table" in "bkdb58" with data
        When the user runs "gpcrondump -a -x bkdb58"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a --incremental -x bkdb58"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    @ddpartII
    Scenario: 59 gpdbrestore list_backup option with full timestamp
        Given the backup test is initialized with database "bkdb59"
        And there is a "heap" table "public.heap_table" in "bkdb59" with data
        When the user runs "gpcrondump -a -x bkdb59"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 60 Incremental Backup and Restore with named pipes
        Given the backup test is initialized with database "bkdb60"
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb60" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb60" with data
        And there is a list to store the incremental backup timestamps
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb60" exists for validation
        And the directory "/tmp/custom_timestamps" exists
        When the user runs "gpcrondump -a -x bkdb60 --list-backup-files -K 20120101010101 -u /tmp/custom_timestamps"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Added the list of pipe names to the file" to stdout
        And gpcrondump should print "Added the list of file names to the file" to stdout
        And gpcrondump should print "Successfully listed the names of backup files and pipes" to stdout
        And the timestamp key "20120101010101" for gpcrondump is stored
        And "pipes" file should be created under "/tmp/custom_timestamps"
        And "regular_files" file should be created under "/tmp/custom_timestamps"
        And the "pipes" file under "/tmp/custom_timestamps" with options " " is validated after dump operation
        And the "regular_files" file under "/tmp/custom_timestamps" with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20120101010101" under "/tmp/custom_timestamps"
        And the named pipes are validated against the timestamp "20120101010101" under "/tmp/custom_timestamps"
        And the named pipe script for the "dump" is run for the files under "/tmp/custom_timestamps"
        When the user runs "gpcrondump -a -x bkdb60 -K 20120101010101 -u /tmp/custom_timestamps"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a -x bkdb60 --list-backup-files -K 20130101010101 --incremental -u /tmp/custom_timestamps"
        Then gpcrondump should return a return code of 0
        And the timestamp key "20130101010101" for gpcrondump is stored
        And "pipes" file should be created under "/tmp/custom_timestamps"
        And "regular_files" file should be created under "/tmp/custom_timestamps"
        And the "pipes" file under "/tmp/custom_timestamps" with options " " is validated after dump operation
        And the "regular_files" file under "/tmp/custom_timestamps" with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20130101010101" under "/tmp/custom_timestamps"
        And the named pipes are validated against the timestamp "20130101010101" under "/tmp/custom_timestamps"
        And the named pipe script for the "dump" is run for the files under "/tmp/custom_timestamps"
        And table "public.ao_part_table" is assumed to be in dirty state in "bkdb60"
        When the user runs "gpcrondump -a -x bkdb60 -K 20130101010101 --incremental -u /tmp/custom_timestamps"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list

    @nbupartII
    @ddpartII
    Scenario: 61 Incremental Backup and Restore with -t filter for Full
        Given the backup test is initialized with database "bkdb61"
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb61" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb61" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb61" with data
        When the user runs "gpcrondump -a -x bkdb61 --prefix=foo -t public.ao_index_table -t public.heap_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb61"
        When the user runs "gpcrondump -x bkdb61 --prefix=foo --incremental  < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Filtering tables using:" to stdout
        And gpcrondump should print "Prefix                        = foo" to stdout
        And gpcrondump should print "Full dump timestamp           = [0-9]{14}" to stdout
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb61 --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print "Filtering bkdb61 for the following tables:" to stdout
        And gpcrondump should print "public.ao_index_table" to stdout
        And gpcrondump should print "public.heap_table" to stdout
        And all the data from "bkdb61" is saved for verification

    Scenario: 62 Incremental Backup and Restore with -T filter for Full
        Given the backup test is initialized with database "bkdb62"
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb62" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb62" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb62" with data
        When the user runs "gpcrondump -a -x bkdb62 --prefix=foo -T public.ao_part_table -T public.heap_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb62"
        When the user runs "gpcrondump -a -x bkdb62 --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb62 --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print "Filtering bkdb62 for the following tables:" to stdout
        And gpcrondump should print "public.ao_index_table" to stdout
        And all the data from "bkdb62" is saved for verification

    Scenario: 63 Incremental Backup and Restore with --table-file filter for Full
        Given the backup test is initialized with database "bkdb63"
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb63" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb63" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb63" with data
        And there is a table-file "/tmp/table_file_1" with tables "public.ao_index_table, public.heap_table"
        When the user runs "gpcrondump -a -x bkdb63 --prefix=foo --table-file /tmp/table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb63"
        When the temp files "table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x bkdb63 --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb63 --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print "Filtering bkdb63 for the following tables:" to stdout
        And gpcrondump should print "public.ao_index_table" to stdout
        And gpcrondump should print "public.heap_table" to stdout
        And all the data from "bkdb63" is saved for verification

    Scenario: 64 Incremental Backup and Restore with --exclude-table-file filter for Full
        Given the backup test is initialized with database "bkdb64"
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb64" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb64" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb64" with data
        And there is a table-file "/tmp/exclude_table_file_1" with tables "public.ao_part_table, public.heap_table"
        When the user runs "gpcrondump -a -x bkdb64 --prefix=foo --exclude-table-file /tmp/exclude_table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb64"
        When the temp files "exclude_table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x bkdb64 --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb64 --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print "Filtering bkdb64 for the following tables:" to stdout
        And gpcrondump should print "public.ao_index_table" to stdout
        And all the data from "bkdb64" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 65 Full Backup with option -T and non-existant table
        Given the backup test is initialized with database "bkdb65"
        And there is a "heap" table "public.heap_table" in "bkdb65" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb65" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb65" exists for validation
        When the user runs "gpcrondump -a -x bkdb65 -T public.heap_table -T cool.dude"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "does not exist in" to stdout
        And the timestamp from gpcrondump is stored

    Scenario: 66 Negative test gpdbrestore -G with incremental timestamp
        Given the backup test is initialized with database "bkdb66"
        And there is a "heap" table "public.heap_table" in "bkdb66" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb66" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb66" with data
        When the user runs "gpcrondump -a -x bkdb66 "
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb66 --incremental -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "global" file should be created under " "
        And table "public.ao_part_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "bkdb66"
        When the user runs "gpcrondump -a -x bkdb66 --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    Scenario: 67 Dump and Restore metadata
        Given the backup test is initialized with database "bkdb67"
        And the directory "/tmp/custom_timestamps" exists
        When the user runs "psql -f test/behave/mgmt_utils/steps/data/create_metadata.sql bkdb67"
        And the user runs "gpcrondump -a -x bkdb67 -K 20140101010101 -u /tmp/custom_timestamps"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And all the data from the remote segments in "bkdb67" are stored in path "/tmp/custom_timestamps" for "full"

    Scenario: 68 Restore -T for incremental dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the backup test is initialized with database "bkdb68"
        And schema "schema_heap" exists in "bkdb68"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb68" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb68" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb68"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb68"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb68"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb68"
        When the user runs "gpcrondump -a -x bkdb68"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb68"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql bkdb68"
        When the user runs "gpcrondump --incremental -a -x bkdb68"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb68 > /tmp/68_describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb68 > /tmp/68_describe_ao_index_table_before"
        When a backup file of tables "public.ao_index_table" in "bkdb68" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb68"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb68"

    Scenario: 69 Restore -T for full dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the backup test is initialized with database "bkdb69"
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb69" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb69"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb69"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb69"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb69"
        When the user runs "gpcrondump -a -x bkdb69"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify the metadata dump file syntax under " " for comments and types
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb69 > /tmp/69_describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb69 > /tmp/69_describe_ao_index_table_before"
        When a backup file of tables "public.ao_index_table, public.co_index_table, public.heap_index_table" in "bkdb69" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb69"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb69"

    @fails_on_mac
    Scenario: 70 Restore -T for full dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the backup test is initialized with database "bkdb70"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb70"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb70"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb70"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql bkdb70"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb70"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" bkdb70"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" bkdb70"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "bkdb70" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" bkdb70"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" bkdb70"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "bkdb70" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" bkdb70"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" bkdb70"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" bkdb70"""
        And there is a "heap" table "customer.heap_index_table_3" with index "heap_index_3" compression "None" in "bkdb70" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_3 owner to customer" bkdb70"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_3 TO customer;" bkdb70"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_3 TO test_group;" bkdb70"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_3 TO select_group;" bkdb70"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" bkdb70"""
        And the user runs "psql -c '\d customer.heap_index_table_1' bkdb70 > /tmp/70_describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb70 > /tmp/70_privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb70 > /tmp/70_describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb70 > /tmp/70_privileges_heap_index_table_2_before"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb70 > /tmp/70_describe_multi_byte_char_before"
        When the user runs "gpcrondump -x bkdb70 -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When a backup file of tables "customer.heap_index_table_1, customer.heap_index_table_2, customer.heap_index_table_3" in "bkdb70" exists for validation
        And table "customer.heap_index_table_1" is dropped in "bkdb70"
        And table "customer.heap_index_table_2" is dropped in "bkdb70"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb70"
        And the user runs "psql -c 'DROP ROLE foo_user' bkdb70"

    @fails_on_mac
    Scenario: 71 Restore -T for incremental dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the backup test is initialized with database "bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql bkdb71"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb71"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" bkdb71"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" bkdb71"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "bkdb71" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" bkdb71"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" bkdb71"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "bkdb71" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" bkdb71"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" bkdb71"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" bkdb71"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" bkdb71"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" bkdb71"""
        And the user runs "psql -c '\d customer.heap_index_table_1' bkdb71 > /tmp/71_describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb71 > /tmp/71_privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb71 > /tmp/71_describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb71 > /tmp/71_privileges_heap_index_table_2_before"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb71 > /tmp/71_describe_multi_byte_char_before"
        When the user runs "gpcrondump -x bkdb71 -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And table "customer.heap_index_table_1" is assumed to be in dirty state in "bkdb71"
        And table "customer.heap_index_table_2" is assumed to be in dirty state in "bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql bkdb71"
        When the user runs "gpcrondump --incremental -x bkdb71 -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When a backup file of tables "customer.heap_index_table_1, customer.heap_index_table_2" in "bkdb71" exists for validation
        And table "customer.heap_index_table_1" is dropped in "bkdb71"
        And table "customer.heap_index_table_2" is dropped in "bkdb71"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb71"
        And the user runs "psql -c 'DROP ROLE foo_user' bkdb71"

    @nbupartII
    @ddpartII
    Scenario: 72 Redirected Restore Full Backup and Restore without -e option
        Given the backup test is initialized with database "bkdb72"
        And the database "bkdb72-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb72" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb72" with data
        And all the data from "bkdb72" is saved for verification
        When the user runs "gpcrondump -x bkdb72 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    @ddpartII
    Scenario: 73 Full Backup and Restore with -e option
        Given the backup test is initialized with database "bkdb73"
        And the database "bkdb73-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb73" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb73" with data
        And all the data from "bkdb73" is saved for verification
        When the user runs "gpcrondump -x bkdb73 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    @ddpartII
    Scenario: 74 Incremental Backup and Redirected Restore
        Given the backup test is initialized with database "bkdb74"
        And the database "bkdb74-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb74" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb74" with data
        When the user runs "gpcrondump -x bkdb74 -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table" is assumed to be in dirty state in "bkdb74"
        When the user runs "gpcrondump -x bkdb74 -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb74" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 75 Full backup and redirected restore with -T
        Given the backup test is initialized with database "bkdb75"
        And the database "bkdb75-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb75" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb75" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb75" with data
        When the user runs "gpcrondump -a -x bkdb75"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb75" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 76 Full backup and redirected restore with -T and --truncate
        Given the backup test is initialized with database "bkdb76"
        And there is a "ao" table "public.ao_index_table" in "bkdb76" with data
        When the user runs "gpcrondump -a -x bkdb76"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb76" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 77 Incremental redirected restore with table filter
        Given the backup test is initialized with database "bkdb77"
        And the database "bkdb77-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb77" with data
        And there is a "ao" table "public.ao_table" in "bkdb77" with data
        And there is a "co" table "public.co_table" in "bkdb77" with data
        When the user runs "gpcrondump -a -x bkdb77"
        And gpcrondump should return a return code of 0
        And table "ao_table" is assumed to be in dirty state in "bkdb77"
        And table "co_table" is assumed to be in dirty state in "bkdb77"
        And the user runs "gpcrondump -a --incremental -x bkdb77"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb77" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 78 Full Backup and Redirected Restore with --prefix option
        Given the backup test is initialized with database "bkdb78"
        And the prefix "foo" is stored
        And the database "bkdb78-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb78" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb78" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb78" exists for validation
        When the user runs "gpcrondump -a -x bkdb78 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And there should be dump files under " " with prefix "foo"

    @nbupartII
    @ddpartII
    Scenario: 79 Full Backup and Redirected Restore with --prefix option for multiple databases
        Given the backup test is initialized with database "bkdb79"
        And the prefix "foo" is stored
        And database "bkdb79-2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb79" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb79" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb79" exists for validation
        And there is a "heap" table "public.heap_table" in "bkdb79-2" with data
        And a backup file of tables "public.heap_table" in "bkdb79-2" exists for validation
        When the user runs "gpcrondump -a -x bkdb79 -x bkdb79-2 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And there should be dump files under " " with prefix "foo"

    Scenario: 80 Full Backup and Restore with the master dump file missing
        Given the backup test is initialized with database "bkdb80"
        And there is a "heap" table "public.heap_table" in "bkdb80" with data
        When the user runs "gpcrondump -x bkdb80 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "dump" file is removed under " "
        And all the data from "bkdb80" is saved for verification

    Scenario: 81 Incremental Backup and Restore with the master dump file missing
        Given the backup test is initialized with database "bkdb81"
        And there is a "heap" table "public.heap_table" in "bkdb81" with data
        When the user runs "gpcrondump -x bkdb81 -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x bkdb81 -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "dump" file is removed under " "
        And all the data from "bkdb81" is saved for verification

    Scenario: 82 Uppercase Database Name Full Backup and Restore
        Given the backup test is initialized with database "bkdb82"
        And database "82TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "82TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "82TESTING" with data
        And all the data from "82TESTING" is saved for verification
        When the user runs "gpcrondump -x 82TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 83 Uppercase Database Name Full Backup and Restore using -s option with and without quotes
        Given the backup test is initialized with database "bkdb83"
        And database "83TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "83TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "83TESTING" with data
        And all the data from "83TESTING" is saved for verification
        When the user runs "gpcrondump -x 83TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 84 Uppercase Database Name Incremental Backup and Restore
        Given the backup test is initialized with database "bkdb84"
        And database "84TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "84TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "84TESTING" with data
        When the user runs "gpcrondump -x 84TESTING -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table" is assumed to be in dirty state in "84TESTING"
        When the user runs "gpcrondump -x 84TESTING -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "84TESTING" is saved for verification

    Scenario: 85 Full backup and Restore should create the gp_toolkit schema with -e option
        Given the backup test is initialized with database "bkdb85"
        And there is a "heap" table "public.heap_table" in "bkdb85" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb85" with data
        And all the data from "bkdb85" is saved for verification
        When the user runs "gpcrondump -x bkdb85 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 86 Incremental backup and Restore should create the gp_toolkit schema with -e option
        Given the backup test is initialized with database "bkdb86"
        And there is a "heap" table "public.heap_table" in "bkdb86" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb86" with data
        And all the data from "bkdb86" is saved for verification
        When the user runs "gpcrondump -x bkdb86 -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x bkdb86 --incremental -a"
        THen gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 87 Redirected Restore should create the gp_toolkit schema with or without -e option
        Given the backup test is initialized with database "bkdb87"
        And the database "bkdb87-2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb87" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb87" with data
        And all the data from "bkdb87" is saved for verification
        When the user runs "gpcrondump -x bkdb87 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 88 gpdbrestore with noanalyze
        Given the backup test is initialized with database "bkdb88"
        And there is a "heap" table "public.heap_table" in "bkdb88" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb88" with data
        When the user runs "gpcrondump -a -x bkdb88"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb88" is saved for verification
        And database "bkdb88" is dropped and recreated

    Scenario: 89 gpdbrestore without noanalyze
        Given the backup test is initialized with database "bkdb89"
        And there is a "heap" table "public.heap_table" in "bkdb89" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb89" with data
        When the user runs "gpcrondump -a -x bkdb89"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb89" is saved for verification

    @nbupartII
    @ddpartII
    Scenario: 90 Writable Report/Status Directory Full Backup and Restore without --report-status-dir option
        Given the backup test is initialized with database "bkdb90"
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And there is a "heap" table "public.heap_table" in "bkdb90" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb90" with data
        And all the data from "bkdb90" is saved for verification
        When the user runs "gpcrondump -x bkdb90 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartII
    @ddpartII
    Scenario: 91 Writable Report/Status Directory Full Backup and Restore with --report-status-dir option
        Given the backup test is initialized with database "bkdb91"
        And there is a "heap" table "public.heap_table" in "bkdb91" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb91" with data
        And all the data from "bkdb91" is saved for verification
        When the user runs "gpcrondump -x bkdb91 -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 92 Writable Report/Status Directory Full Backup and Restore with -u option
        Given the backup test is initialized with database "bkdb92"
        And there is a "heap" table "public.heap_table" in "bkdb92" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb92" with data
        And all the data from "bkdb92" is saved for verification
        And the directory "/tmp/92_custom_timestamps" exists
        When the user runs "gpcrondump -x bkdb92 -a -u /tmp/92_custom_timestamps -K 20150101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 93 Writable Report/Status Directory Full Backup and Restore with no write access -u option
        Given the backup test is initialized with database "bkdb93"
        And there is a "heap" table "public.heap_table" in "bkdb93" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb93" with data
        And all the data from "bkdb93" is saved for verification
        And the directory "/tmp/custom_timestamps" exists
        When the user runs "gpcrondump -x bkdb93 -a -u /tmp/custom_timestamps -K 20160101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "chmod -R 555 /tmp/custom_timestamps/db_dumps"

    @nbupartII
    @ddpartII
    Scenario: 94 Filtered Full Backup with Partition Table
        Given the backup test is initialized with database "bkdb94"
        And there is a "heap" table "public.heap_table" in "bkdb94" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb94" with data
        When the user runs "gpcrondump -a -x bkdb94"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb94" is saved for verification

    @nbupartIII
    @ddpartII
    Scenario: 95 Filtered Incremental Backup with Partition Table
        Given the backup test is initialized with database "bkdb95"
        And there is a "heap" table "public.heap_table" in "bkdb95" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb95" with data
        When the user runs "gpcrondump -a -x bkdb95"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb95 --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb95" is saved for verification

    @nbupartIII
    @ddpartII
    Scenario: 96 gpdbrestore runs ANALYZE on restored table only
        Given the backup test is initialized with database "bkdb96"
        And there is a "heap" table "public.heap_table" in "bkdb96" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb96" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb96" with data
        And the database "bkdb96" is analyzed
        When the user runs "gpcrondump -a -x bkdb96"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb96" is saved for verification
        And the user truncates "public.ao_index_table" tables in "bkdb96"
        And the user deletes rows from the table "heap_table" of database "bkdb96" where "column1" is "1088"

    @nbupartIII
    @ddpartII
    Scenario: 97 Full Backup with multiple -S option and Restore
        Given the backup test is initialized with database "bkdb97"
        And schema "schema_heap, schema_ao, testschema" exists in "bkdb97"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb97" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb97" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb97" with data
        And a backup file of tables "testschema.heap_table, schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb97" exists for validation
        When the user runs "gpcrondump -a -x bkdb97 -S schema_heap -S testschema"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartIII
    @ddpartII
    Scenario: 98 Full Backup with option -S and Restore
        Given the backup test is initialized with database "bkdb98"
        And schema "schema_heap, schema_ao" exists in "bkdb98"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb98" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb98" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb98" exists for validation
        When the user runs "gpcrondump -a -x bkdb98 -S schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartIII
    @ddpartIII
    Scenario: 99 Full Backup with option -s and Restore
        Given the backup test is initialized with database "bkdb99"
        And schema "schema_heap, schema_ao" exists in "bkdb99"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb99" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb99" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb99" exists for validation
        When the user runs "gpcrondump -a -x bkdb99 -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartIII
    @ddpartIII
    Scenario: 100 Full Backup with option --exclude-schema-file and Restore
        Given the backup test is initialized with database "bkdb100"
        And schema "schema_heap, schema_ao, testschema" exists in "bkdb100"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb100" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb100" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb100" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb100" exists for validation
        And there is a file "exclude_file" with tables "testschema|schema_ao"
        When the user runs "gpcrondump -a -x bkdb100 --exclude-schema-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartIII
    @ddpartIII
    Scenario: 101 Full Backup with option --schema-file and Restore
        Given the backup test is initialized with database "bkdb101"
        And schema "schema_heap, schema_ao, testschema" exists in "bkdb101"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb101" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb101" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb101" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb101" exists for validation
        And there is a file "include_file" with tables "schema_heap|schema_ao"
        When the user runs "gpcrondump -a -x bkdb101 --schema-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    @nbupartIII
    @ddpartIII
    Scenario: 106 Full Backup and Restore with option --change-schema
        Given the backup test is initialized with database "bkdb106"
        And schema "schema_heap, schema_ao, schema_new" exists in "bkdb106"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb106" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb106" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb106" exists for validation
        And there is a file "106_include_file" with tables "schema_heap.heap_table|schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb106 --table-file 106_include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb106" exists for validation

    Scenario: 107 Incremental Backup and Restore with option --change-schema
        Given the backup test is initialized with database "bkdb107"
        And schema "schema_heap, schema_ao, schema_new" exists in "bkdb107"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb107" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb107" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb107" exists for validation
        And there is a file "107_include_file" with tables "schema_heap.heap_table|schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb107 --table-file 107_include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And table "schema_ao.ao_part_table" is assumed to be in dirty state in "bkdb107"
        And the user runs "gpcrondump -a --incremental -x bkdb107"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb107" exists for validation

    Scenario: 108 Full backup and restore with statistics
        Given the backup test is initialized with database "bkdb108"
        And there is a "heap" table "public.heap_table" in "bkdb108" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb108" with data
        And the database "bkdb108" is analyzed
        When the user runs "gpcrondump -a -x bkdb108 --dump-stats"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "statistics" file should be created under " "

    @nbupartIII
    @ddpartIII
    Scenario: 109 Backup and restore with statistics and table filters
        Given the backup test is initialized with database "bkdb109"
        And there is a "heap" table "public.heap_table" in "bkdb109" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb109" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb109" with data
        And the database "bkdb109" is analyzed
        When the user runs "gpcrondump -a -x bkdb109 --dump-stats -t public.heap_table -t public.heap_index_table"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "statistics" file should be created under " "
        And verify that the "statistics" file in " " dir does not contain "Schema: public, Table: ao_part_table"
        And database "bkdb109" is dropped and recreated

    Scenario: 110 Restoring a nonexistent table should fail with clear error message
        Given the backup test is initialized with database "bkdb110"
        And there is a "heap" table "heap_table" in "bkdb110" with data
        When the user runs "gpcrondump -a -x bkdb110"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 111 Full Backup with option --schema-file with prefix option and Restore
        Given the backup test is initialized with database "bkdb111"
        And the prefix "foo" is stored
        And schema "schema_heap, schema_ao, testschema" exists in "bkdb111"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb111" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb111" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb111" with data
        And a backup file of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb111" exists for validation
        And there is a file "include_file" with tables "schema_heap|schema_ao"
        When the user runs "gpcrondump -a -x bkdb111 --schema-file include_file --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"

    Scenario: 112 Simple Full Backup with AO/CO statistics w/ filter
        Given the backup test is initialized with database "bkdb112"
        And there is a "ao" table "public.ao_table" in "bkdb112" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb112" with data
        When the user runs "gpcrondump -a -x bkdb112"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 113 Simple Full Backup with AO/CO statistics w/ filter schema
        Given the backup test is initialized with database "bkdb113"
        And schema "schema_ao, testschema" exists in "bkdb113"
        And there is a "ao" table "public.ao_table" in "bkdb113" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb113" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb113" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb113" with data
        And there is a "ao" partition table "testschema.ao_foo" in "bkdb113" with data
        When the user runs "gpcrondump -a -x bkdb113"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 114 Restore with --redirect option should not rely on existance of dumped database
        Given the backup test is initialized with database "bkdb114"
        When the user runs "gpcrondump -a -x bkdb114"
        And the timestamp from gpcrondump is stored
        And the database "bkdb114" does not exist
        And database "bkdb114" is dropped and recreated

    Scenario: 115 Database owner can be assigned to role containing special characters
        Given the backup test is initialized with database "bkdb115"
        When the user runs "psql -c 'DROP ROLE IF EXISTS "Foo%user"' -d bkdb115"
        Then psql should return a return code of 0
        When the user runs "psql -c 'CREATE ROLE "Foo%user"' -d bkdb115"
        Then psql should return a return code of 0
        When the user runs "psql -c 'ALTER DATABASE bkdb115 OWNER TO "Foo%user"' -d bkdb115"
        Then psql should return a return code of 0
        And there is a "ao" table "public.ao_table" in "bkdb115" with data
        When the user runs "gpcrondump -a -x bkdb115"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "cdatabase" file in " " dir contains "OWNER = "Foo%user""

    Scenario: 116 pg_temp should be ignored from gpcrondump --table_file option and -t option when given
        Given the backup test is initialized with database "bkdb116"
        And there is a "ao" table "public.foo4" in "bkdb116" with data
        # NOTE: pg_temp does not exist in the database at all. We are just valiating that we can still
        # do a backup given that the user tries to backup a temporary table
        # --table-file option ignore pg_temp
        And there is a table-file "/tmp/table_file_foo4" with tables "public.foo4, pg_temp_1337.foo4"
        When the user runs "gpcrondump -a --table-file /tmp/table_file_foo4 -x bkdb116"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 117 Schema level restore with gpdbrestore -S option for views, sequences, and functions
        Given the user runs "psql -f test/behave/mgmt_utils/steps/data/schema_level_test_workload.sql template1"
        When the user runs "gpcrondump -a -x schema_level_test_db"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 118 Backup a database with database-specific configuration
        Given the backup test is initialized with database "bkdb118"
        When the user runs "psql -d bkdb118 -c 'alter database bkdb118 set search_path=''daisy'';'"
        Then psql should return a return code of 0
        When the user runs "psql -d bkdb118 -c "alter database bkdb118 set gp_default_storage_options='appendonly=true,blocksize=65536';""
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb118"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @ddpartIII
    @skip_for_gpdb_43
    Scenario: 119 Backup database grants
        Given the backup test is initialized with database "bkdb119"
        And the user runs """psql -c "DROP ROLE IF EXISTS user_grant" bkdb119"""
        And the user runs """psql -c "CREATE ROLE user_grant LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb119"""
        Then psql should return a return code of 0
        When the user runs "psql -d bkdb119 -c "GRANT CREATE ON DATABASE bkdb119 to user_grant ;""
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb119"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 120 Simple full backup and restore with special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB""
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        When the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/120_special_table_data.ans"

    Scenario: 121 gpcrondump with -T option where table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        And a list of files "121_ao,121_heap" of tables "$SP_CHAR_SCHEMA.$SP_CHAR_AO,$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" exists for validation
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" -T "$SP_CHAR_SCHEMA"."$SP_CHAR_CO""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 122 gpcrondump with --exclude-table-file option where table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        And a list of files "122_ao,122_heap" of tables "$SP_CHAR_SCHEMA.$SP_CHAR_AO,$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" exists for validation
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --exclude-table-file test/behave/mgmt_utils/steps/data/special_chars/exclude-table-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 123 gpcrondump with --table-file option where table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        And a list of files "123_ao,123_heap" of tables "$SP_CHAR_SCHEMA.$SP_CHAR_AO,$SP_CHAR_SCHEMA.$SP_CHAR_HEAP" in "$SP_CHAR_DB" exists for validation
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --table-file test/behave/mgmt_utils/steps/data/special_chars/table-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 124 gpcrondump with -t option where table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        And a list of files "124_ao" of tables "$SP_CHAR_SCHEMA.$SP_CHAR_AO" in "$SP_CHAR_DB" exists for validation
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" -t "$SP_CHAR_SCHEMA"."$SP_CHAR_AO""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 125 gpcrondump with --schema-file option when schema name and database name contains special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --schema-file test/behave/mgmt_utils/steps/data/special_chars/schema-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/125_special_table_data.ans"

    Scenario: 126 gpcrondump with -s option when schema name and database name contains special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" -s "$SP_CHAR_SCHEMA""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/126_special_table_data.ans"

    Scenario: 127 gpcrondump with --exclude-schema-file option when schema name and database name contains special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --exclude-schema-file test/behave/mgmt_utils/steps/data/special_chars/schema-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 128 gpcrondump with -S option when schema name and database name contains special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" -S "$SP_CHAR_SCHEMA""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 130 Gpdbrestore with -T, --truncate, and --change-schema options when table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        And the user runs command "psql -f  psql -c """select * from \"$SP_CHAR_SCHEMA\".\"$SP_CHAR_AO\" order by 1""" -d "$SP_CHAR_DB"  > /tmp/130_table_data.ans"
        And the user runs "gpcrondump -a -x "$SP_CHAR_DB""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @nbupartIII
    @ddpartIII
    Scenario: 131 gpcrondump with --incremental option when table name, schema name and database name contains special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB""
        Then gpcrondump should return a return code of 0
        Given the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/131_special_table_data.ans"

    @nbupartIII
    @ddpartIII
    Scenario: 132 gpdbrestore with --redirect option with special db name, and all table name, schema name and database name contain special character
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/132_special_table_data.ans"

    Scenario: 133 gpdbrestore, -S option, -S truncate option schema level restore with special chars in schema name
        Given the backup test is initialized for special characters
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql "$SP_CHAR_DB" > /tmp/133_special_table_data.ans"

    @skip_for_gpdb_43
    Scenario: 136 Backup and restore CAST, with associated function in restored schema, base_file_name=dump_func_name
        Given the backup test is initialized with database "bkdb136"
        And there is a "heap" table "public.heap_table" in "bkdb136" with data
        And schema "testschema" exists in "bkdb136"
        And there is a "heap" table "testschema.heap_table" in "bkdb136" with data
        And a cast is created in "bkdb136"
        Then verify that a cast exists in "bkdb136" in schema "public"
        When the user runs "gpcrondump -a -x bkdb136"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @skip_for_gpdb_43
    Scenario: 137 Backup and restore CAST, with associated function in non-restored schema
        Given the backup test is initialized with database "bkdb137"
        And there is a "heap" table "public.heap_table" in "bkdb137" with data
        And schema "testschema" exists in "bkdb137"
        And there is a "heap" table "testschema.heap_table" in "bkdb137" with data
        And a cast is created in "bkdb137"
        Then verify that a cast exists in "bkdb137" in schema "public"
        When the user runs "gpcrondump -a -x bkdb137"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 138 Incremental backup and restore with restore to primary and mirror after failover
        Given the backup test is initialized with database "bkdb138"
        Given the backup test is initialized with database "bkdb138-2"
        And all the segments are running
        And the segments are synchronized
        And there is a "ao" table "public.ao_table" in "bkdb138" with data
        And there is a "heap" table "public.heap_table" in "bkdb138" with data
        And there is a "ao" table "public.ao_table" in "bkdb138-2" with data
        And there is a "heap" table "public.heap_table" in "bkdb138-2" with data

        When the user runs "gpcrondump -a -x bkdb138"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And table "public.ao_table" is assumed to be in dirty state in "bkdb138"
        And the user runs "gpcrondump -a --incremental -x bkdb138"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

        When the user runs "gpcrondump -a -x bkdb138-2"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored

        And user kills a primary postmaster process
        And user can start transactions
        And the user runs "gpcrondump -a --incremental -x bkdb138"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

        And table "public.ao_table" is assumed to be in dirty state in "bkdb138-2"
        And the user runs "gpcrondump -a --incremental -x bkdb138-2"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb138-2" is saved for verification

        When the user runs "gprecoverseg -Fa"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0

        And the user runs "gpcrondump -a --incremental -x bkdb138"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb138" is saved for verification

    Scenario: 141 Backup with all GUC (system settings) set to defaults will succeed
        Given the backup test is initialized with database "bkdb141"
        # set guc defaults
        And the user runs "psql bkdb141 -c "Alter database bkdb141 set gp_default_storage_options='appendonly=true, orientation=row, blocksize=65536, checksum=false, compresslevel=4, compresstype=none'""
        Then psql should return a return code of 0
        And there is a "heap" table "public.default_guc" in "bkdb141" with data
        # create a role that has different gucs
        And a role "dsp_role" is created
        And the user runs "psql bkdb141 -c "Alter role dsp_role set gp_default_storage_options='appendonly=true, orientation=column, compresstype=zlib'""
        Then psql should return a return code of 0
        # connect as different role create a table with the role's gucs
        And the user runs command "export PGPASSWORD=dsprolepwd && psql bkdb141 dsp_role -f test/behave/mgmt_utils/steps/data/gpcrondump/guc_role_create_table.sql"
        Then export should return a return code of 0
        # change guc to non-default for just current session and create session_guc_table
        And the user runs "psql bkdb141 -f test/behave/mgmt_utils/steps/data/gpcrondump/guc_session_only.sql"
        # backup and restore
        When the user runs "gpcrondump -a -x bkdb141"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    Scenario: 142 gpcrondump -u option with include table filtering
        Given the backup test is initialized with database "bkdb142"
        And there is a "ao" table "public.ao_table" in "bkdb142" with data
        And there is a "co" table "public.co_table" in "bkdb142" with data
        And there is a "heap" table "public.heap_table" in "bkdb142" with data
        And there is a file "include_file" with tables "public.ao_table|public.heap_table"
        When the user runs "gpcrondump -a -x bkdb142 --table-file include_file -u /tmp"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb142" is saved for verification

    Scenario: 143 gpcrondump -u option with exclude table filtering
        Given the backup test is initialized with database "bkdb143"
        And there is a "ao" table "public.ao_table" in "bkdb143" with data
        And there is a "co" table "public.co_table" in "bkdb143" with data
        And there is a "heap" table "public.heap_table" in "bkdb143" with data
        And there is a file "exclude_file" with tables "public.ao_table|public.heap_table"
        When the user runs "gpcrondump -a -x bkdb143 --exclude-table-file exclude_file -u /tmp"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb143" is saved for verification

    Scenario: 144 gpdbrestore -u option with include table filtering
        Given the backup test is initialized with database "bkdb144"
        And there is a "ao" table "public.ao_table" in "bkdb144" with data
        And there is a "co" table "public.co_table" in "bkdb144" with data
        When the user runs "gpcrondump -a -x bkdb144 -u /tmp"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb144" is saved for verification

    @nbupartIII
    Scenario: 145 gpcrondump with -u and --prefix option
        Given the backup test is initialized with database "bkdb145"
        And the prefix "foo" is stored
        And the database "bkdb1452" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb145" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb145" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb145" exists for validation
        When the user runs "gpcrondump -a -x bkdb145 --prefix=foo -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored

    @ddpartIII
    @ddonly
    Scenario: 146 Full backup and restore with remote DataDomain replication using gpcrondump
        Given the backup test is initialized with database "bkdb146"
        And there is a "heap" table "public.heap_table" in "bkdb146" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb146" with data
        And a backup file of tables "public.heap_table, public.ao_index_table" in "bkdb146" exists for validation
        And all the data from "bkdb146" is saved for verification
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb146 --replicate --max-streams=15"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the files in the backup sets are validated for the "local" storage unit
        And the files in the backup sets are validated for the "remote" storage unit
        And the backup sets on the "local" storage unit are deleted using gp_mfr

    @ddpartIII
    @ddonly
    Scenario: 147 Full backup and restore with remote DataDomain replication using gp_mfr
        Given the backup test is initialized with database "bkdb147"
        And there is a "heap" table "public.heap_table" in "bkdb147" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb147" with data
        And a backup file of tables "public.heap_table, public.ao_index_table" in "bkdb147" exists for validation
        And all the data from "bkdb147" is saved for verification
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb147"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the backup sets are replicated to the remote DataDomain
        And the files in the backup sets are validated for the "local" storage unit
        And the files in the backup sets are validated for the "remote" storage unit
        And the backup sets on the "local" storage unit are deleted using gp_mfr

    @ddpartIII
    @ddonly
    Scenario: 148 Incremental backup and restore with remote DataDomain replication using gpcrondump
        Given the backup test is initialized with database "bkdb148"
        And there is a "heap" table "public.heap_table" in "bkdb148" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb148" with data
        And a backup file of tables "public.heap_table, public.ao_index_table" in "bkdb148" exists for validation
        And all the data from "bkdb148" is saved for verification
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb148 --replicate --max-streams=15"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb148"
        When the user runs "gpcrondump -a -x bkdb148 --incremental --replicate --max-streams=15"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the files in the backup sets are validated for the "local" storage unit
        And the files in the backup sets are validated for the "remote" storage unit
        And the backup sets on the "local" storage unit are deleted using gp_mfr

    @ddpartIII
    @ddonly
    Scenario: 149 Incremental backup and restore with remote DataDomain replication using gp_mfr
        Given the backup test is initialized with database "bkdb149"
        And there is a "heap" table "public.heap_table" in "bkdb149" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb149" with data
        And a backup file of tables "public.heap_table, public.ao_index_table" in "bkdb149" exists for validation
        And all the data from "bkdb149" is saved for verification
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb149"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb149"
        When the user runs "gpcrondump -a -x bkdb149 --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the backup sets are replicated to the remote DataDomain
        And the files in the backup sets are validated for the "local" storage unit
        And the files in the backup sets are validated for the "remote" storage unit
        And the backup sets on the "local" storage unit are deleted using gp_mfr
