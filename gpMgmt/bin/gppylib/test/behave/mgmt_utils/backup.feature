@backup
Feature: Validate command line arguments

    @backupfire
    Scenario: Entering an invalid argument
        When the user runs "gpcrondump -X"
        Then gpcrondump should print no such option: -X error message
        And gpcrondump should return a return code of 2

    @backupfire
    Scenario: Valid option combinations for incremental backup
        Given the test is initialized
        When the user runs "gpcrondump -a --incremental -t public.bkdb -x bkdb"
        Then gpcrondump should print include table list can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -T public.bkdb -x bkdb"
        And gpcrondump should print exclude table list can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental --exclude-table-file /tmp/foo -x bkdb"
        And gpcrondump should print exclude table file can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental --table-file /tmp/foo -x bkdb"
        And gpcrondump should print include table file can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -s foo -x bkdb"
        And gpcrondump should print -s option can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -c -x bkdb"
        And gpcrondump should print -c option can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -f 10 -x bkdb"
        And gpcrondump should print -f option cannot be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -o -x bkdb"
        And gpcrondump should print -o option cannot be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump --incremental"
        And gpcrondump should print Must supply -x <database name> with incremental option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump --list-backup-files"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Must supply -K option when listing backup files to stdout
        When the user runs "gpcrondump -K 20140101010101 -x bkdb -x fulldb -a"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print multi-database backup is not supported with -K option to stdout
        When the user runs "gpcrondump -a --incremental -x bkdb -x fulldb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print multi-database backup is not supported with incremental backup to stdout
        When the user runs "gpcrondump -a --list-backup-files -K 20130101010101 --ddboost -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print list backup files not supported with ddboost option to stdout
        When the user runs "gpcrondump -a --list-filter-tables -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print list filter tables option requires --prefix and --incremental to stdout
        When the user runs "gpcrondump -a --list-filter-tables -x bkdb --incremental"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print list filter tables option requires --prefix and --incremental to stdout
        When the user runs "gpdbrestore --help"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print noanalyze to stdout

    @backupfire
    Scenario: Valid option combinations for schema level backup
        Given the test is initialized
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -s schema_heap -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with -s option to stdout
        When the user runs "gpcrondump -a -S schema_heap -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with -S option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a -s schema_heap -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with -s option to stdout
        When the user runs "gpcrondump -a -S schema_heap -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with -S option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -t and -T can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a -s schema_heap --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with -s option to stdout
        When the user runs "gpcrondump -a -S schema_heap --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with -S option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a -s schema_heap --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with -s option to stdout
        When the user runs "gpcrondump -a -S schema_heap --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with -S option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --table-file and --exclude-table-file can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --schema-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --exclude-schema-file can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -s schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -s can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -s schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -s can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -S schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -S can not be selected with --exclude-schema-file option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -S schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -S can not be selected with --schema-file option to stdout
        When the user runs "gpcrondump -a -s schema_heap -S schema_heap -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -s can not be selected with -S option to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --exclude-schema-file /tmp/bar -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print --exclude-schema-file can not be selected with --schema-file option to stdout
        And the user runs "psql -c 'drop schema schema_heap cascade;' bkdb"

    @backupfire
    Scenario: Valid option combinations for gpdbrestore
        When the user runs "gpdbrestore -t 20140101010101 --truncate -a"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print --truncate can be specified only with -S, -T, or --table-file option to stdout
        When the user runs "gpdbrestore -t 20140101010101 --truncate -e -T public.foo -a"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify --truncate and -e together to stdout
        And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
        And the user runs "gpdbrestore -t 20140101010101 -T public.ao_table --table-file /tmp/table_file_foo"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify -T and --table-file together to stdout
        Then the file "/tmp/table_file_foo" is removed from the system
        When the user runs "gpdbrestore -u /tmp --ddboost -s bkdb"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print -u cannot be used with DDBoost parameters to stdout

    @backupsmoke
    Scenario: Negative test for Incremental Backup
        Given the test is initialized
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Negative test for Incremental Backup - Incremental with -u after a full backup to default directory
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Negative test for Incremental Backup - Incremental to default directory after a full backup with -u option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Negative test for Incremental Backup - Incremental after a full backup of different database
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb2"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Dirty table list check on recreating a table with same data and contents
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When the "public.ao_table" is recreated with same data in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "public.ao_table" is marked as dirty in dirty_list file
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that plan file has latest timestamp for "public.ao_table"

    Scenario: Negative test for missing state file
        Given the test is initialized
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "ao" state file under " " is saved for the "full" timestamp
        And the saved state file is deleted
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print state file does not exist to stdout

    Scenario: Negative test for invalid state file format
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "ao" state file under " " is saved for the "full" timestamp
        And the saved state file is corrupted
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Invalid state file format to stdout

    @backupsmoke
    Scenario: Simple Incremental Backup
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "co" table "public.co_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "co" table "public.co_index_table" in "bkdb" with data
        And there is a "co" table "public.co_index_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb" with data
        And there is a mixed storage partition table "part_mixed_1" in "bkdb" with data
        Given the user runs "echo > /tmp/backup_gpfdist_dummy"
        And the user runs "gpfdist -p 8098 -d /tmp &"
        And there is a partition table "part_external" has external partitions of gpfdist with file "backup_gpfdist_dummy" on port "8098" in "bkdb" with data
        Then data for partition table "part_mixed_1" with partition level "1" is distributed across all segments on "bkdb"
        And data for partition table "part_external" with partition level "0" is distributed across all segments on "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Validating disk space to stdout
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "last_operation" files are generated under " " for stored "full" timestamp
        When partition "1" of partition table "ao_part_table, co_part_table_comp, part_mixed_1" is assumed to be in dirty state in "bkdb" in schema "public"
        And partition "1" in partition level "0" of partition table "part_external" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the temp files "dirty_backup_list" are removed from the system
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the temp files "dirty_backup_list" are not created in the system
        And gpcrondump should not print Validating disk space to stdout
        And the timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "incr" timestamp
        And the "last_operation" files are generated under " " for stored "incr" timestamp
        And the subdir from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "-L"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Table public.ao_index_table to stdout
        And gpdbrestore should print Table public.ao_index_table_comp to stdout
        And gpdbrestore should print Table public.ao_part_table to stdout
        And gpdbrestore should print Table public.ao_part_table_comp to stdout
        And gpdbrestore should print Table public.part_external to stdout
        And gpdbrestore should print Table public.ao_table to stdout
        And gpdbrestore should print Table public.ao_table_comp to stdout
        And gpdbrestore should print Table public.co_index_table to stdout
        And gpdbrestore should print Table public.co_index_table_comp to stdout
        And gpdbrestore should print Table public.co_part_table to stdout
        And gpdbrestore should print Table public.co_part_table_comp to stdout
        And gpdbrestore should print Table public.co_table to stdout
        And gpdbrestore should print Table public.co_table_comp to stdout
        And gpdbrestore should print Table public.heap_index_table to stdout
        And gpdbrestore should print Table public.heap_part_table to stdout
        And gpdbrestore should print Table public.heap_table to stdout
        And gpdbrestore should print Table public.part_mixed_1 to stdout
        And database "bkdb" is dropped and recreated
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that partitioned tables "ao_part_table, co_part_table, heap_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table_comp, co_part_table_comp" in "bkdb" have 6 partitions
        And verify that partitioned tables "part_external" in "bkdb" have 5 partitions in partition level "0"
        And verify that partitioned tables "ao_part_table, co_part_table_comp" in "bkdb" has 0 empty partitions
        And verify that partitioned tables "co_part_table, ao_part_table_comp" in "bkdb" has 0 empty partitions
        And verify that partitioned tables "heap_part_table" in "bkdb" has 0 empty partitions
        And verify that there is a "heap" table "public.heap_table" in "bkdb"
        And verify that there is a "heap" table "public.heap_index_table" in "bkdb"
        And verify that there is partition "1" of "ao" partition table "ao_part_table" in "bkdb" in "public"
        And verify that there is partition "1" of "co" partition table "co_part_table_comp" in "bkdb" in "public"
        And verify that there is partition "1" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "2" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "3" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "1" of mixed partition table "part_mixed_1" with storage_type "c"  in "bkdb" in "public"
        And verify that there is partition "2" in partition level "0" of mixed partition table "part_external" with storage_type "x"  in "bkdb" in "public"
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore
        And verify that the distribution policy of all the tables in "bkdb" are validated after restore
        And verify that the incremental file has the stored timestamp
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Increments File Check With Complicated Scenario
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb2"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        And verify that the incremental file has all the stored timestamps

    Scenario: Incremental File Check With Different Directory
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under "/tmp"
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under "/tmp"
        And verify that the incremental file in "/tmp" has all the stored timestamps

    @backupsmoke
    Scenario: Incremental Backup with -u option
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in "/tmp" dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under "/tmp"
        And the subdir from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And database "bkdb" is dropped and recreated
        And the user runs gp_restore with the stored timestamp and subdir in "bkdb" and backup_dir "/tmp"
        And gp_restore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_table" in "bkdb"
        And verify that the data of the dirty tables under "/tmp" in "bkdb" is validated after restore
        And verify that the distribution policy of all the tables in "bkdb" are validated after restore

    Scenario: gpdbrestore with -R for full dump
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the subdir from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        And all the data from the remote segments in "bkdb" are stored in path "/tmp" for "full"
        And the user runs gpdbrestore with "-R" option in path "/tmp"
        And gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: gpdbrestore with -R for incremental dump
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        When table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And all files for full backup have been removed in path "/tmp"
        And all the data from the remote segments in "bkdb" are stored in path "/tmp" for "inc"
        And the user runs gpdbrestore with "-R" option in path "/tmp"
        And gpdbrestore should print -R is not supported for restore with incremental timestamp to stdout

    @backupsmoke
    Scenario: Full Backup and Restore
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/add_rules_indexes_constraints_triggers.sql bkdb"
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And verify that the "status" file in " " dir contains "reading schemas"
        And verify that the "status" file in " " dir contains "reading user-defined functions"
        And verify that the "status" file in " " dir contains "reading user-defined types"
        And verify that the "status" file in " " dir contains "reading type storage options"
        And verify that the "status" file in " " dir contains "reading procedural languages"
        And verify that the "status" file in " " dir contains "reading user-defined aggregate functions"
        And verify that the "status" file in " " dir contains "reading user-defined operators"
        And verify that the "status" file in " " dir contains "reading user-defined external protocols"
        And verify that the "status" file in " " dir contains "reading user-defined operator classes"
        And verify that the "status" file in " " dir contains "reading user-defined conversions"
        And verify that the "status" file in " " dir contains "reading user-defined tables"
        And verify that the "status" file in " " dir contains "reading table inheritance information"
        And verify that the "status" file in " " dir contains "reading rewrite rules"
        And verify that the "status" file in " " dir contains "reading type casts"
        And verify that the "status" file in " " dir contains "finding inheritance relationships"
        And verify that the "status" file in " " dir contains "reading column info for interesting tables"
        And verify that the "status" file in " " dir contains "flagging inherited columns in subtables"
        And verify that the "status" file in " " dir contains "reading indexes"
        And verify that the "status" file in " " dir contains "reading constraints"
        And verify that the "status" file in " " dir contains "reading triggers"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that the "report" file in " " dir does not contain "ERROR"
        And verify that the "status" file in " " dir does not contain "ERROR"
        And verify that there is a constraint "check_constraint_no_domain" in "bkdb"
        And verify that there is a constraint "check_constraint_with_domain" in "bkdb"
        And verify that there is a constraint "unique_constraint" in "bkdb"
        And verify that there is a constraint "foreign_key" in "bkdb"
        And verify that there is a rule "myrule" in "bkdb"
        And verify that there is a trigger "mytrigger" in "bkdb"
        And verify that there is an index "my_unique_index" in "bkdb"

    Scenario: Metadata-only restore
        Given the test is initialized
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the schemas "schema_heap" do not exist in "bkdb"
        And the user runs gpdbrestore with the stored timestamp and options "-m"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb"
        And the table names in "bkdb" is stored
        And tables in "bkdb" should not contain any data

    Scenario: Metadata-only restore with global objects (-G)
        Given the test is initialized
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And the user runs "psql -c 'CREATE ROLE "foo%userWITHCAPS"' bkdb"
        When the user runs "gpcrondump -a -x bkdb -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -c 'DROP ROLE "foo%userWITHCAPS"' bkdb"
        And the schemas "schema_heap" do not exist in "bkdb"
        And the user runs gpdbrestore with the stored timestamp and options "-m -G"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb"
        And the table names in "bkdb" is stored
        And tables in "bkdb" should not contain any data
        And verify that a role "foo%userWITHCAPS" exists in database "bkdb"
        And the user runs "psql -c 'DROP ROLE "foo%userWITHCAPS"' bkdb"

    Scenario: gpdbrestore -L with Full Backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        When the user runs gpdbrestore with the stored timestamp and options "-L"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Table public.ao_part_table to stdout
        And gpdbrestore should print Table public.heap_table to stdout

    @backupfire
    Scenario: gpcrondump -b with Full and Incremental backup
        Given the test is initialized
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -b"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Bypassing disk space check to stdout
        And gpcrondump should not print Validating disk space to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -b --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Bypassing disk space checks for incremental backup to stdout
        And gpcrondump should not print Validating disk space to stdout

    @backupfire
    Scenario: gpdbrestore -b with Full timestamp
        Given the test is initialized
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp and options "-b"
        Then gpdbrestore should return a return code of 0

    Scenario: Output info gpdbrestore
        Given the test is initialized
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Full Database to stdout
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Table Restore to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Incremental Restore to stdout
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Incremental Table Restore to stdout

    Scenario: Output info gpcrondump
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Dump type                                = Full to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Dump type                                = Incremental to stdout

    Scenario: gpcrondump -G with Full timestamp
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -G"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -G --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "

    Scenario: Backup and restore with -G only
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the user runs "psql -c 'CREATE ROLE foo_user' bkdb"
        And verify that a role "foo_user" exists in database "bkdb"
        When the user runs "gpcrondump -a -x bkdb -G"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And the user runs "psql -c 'DROP ROLE foo_user' bkdb"
        And the user runs gpdbrestore with the stored timestamp and options "-G only"
        And gpdbrestore should return a return code of 0
        And verify that a role "foo_user" exists in database "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"
        And the user runs "psql -c 'DROP ROLE foo_user' bkdb"

    @valgrind
    Scenario: Valgrind test of gp_dump incremental
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental bkdb" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump incremental with table file
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump full with table file
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental with table file
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only bkdb --incremental --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent full with table file
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only bkdb --incremental" and options " "

    @valgrind
    Scenario: Valgrind test of gp_restore for incremental backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs valgrind with "gp_restore" and options "-i --gp-i --gp-l=p -d bkdb --gp-c"

    @valgrind
    Scenario: Valgrind test of gp_restore_agent for incremental backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs valgrind with "gp_restore_agent" and options "--gp-c /bin/gunzip -s --post-data-schema-only --target-dbid 1 -d bkdb"

    @backupfire
    Scenario: Full Backup with option -t and Restore
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And the temp files "include_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x bkdb -t public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "include_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"

    @backupfire
    Scenario: Full Backup with option -T and Restore
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And the temp files "exclude_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x bkdb -T public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "exclude_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that there is no table "public.heap_table" in "bkdb"

    @backupfire
    Scenario: Full Backup with option --exclude-table-file and Restore
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.co_part_table" in "bkdb" exists for validation
        And there is a file "exclude_file" with tables "public.heap_table|public.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb --exclude-table-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "co" table "public.co_part_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"

    @backupfire
    Scenario: Full Backup with option --table-file and Restore
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "public.heap_table|public.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is no table "public.co_part_table" in "bkdb"

    Scenario: plan file creation in directory
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "

    Scenario: Simple Plan File Test
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table_comp" with compression "zlib" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "co" table "public.co_index_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table_comp" with compression "zlib" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts1"
        And "dirty_list" file should be created under " "
        And table "public.co_table" is assumed to be in dirty state in "bkdb"
        And partition "1" of partition table "co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts2"
        And "dirty_list" file should be created under " "
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file is validated against "data/bar_plan1"

    Scenario: No plan file generated
        Given the test is initialized
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should not be created under " "

    Scenario: Schema only restore of incremental backup
        Given the test is initialized
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the table names in "bkdb" is stored
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And tables names should be identical to stored table names in "bkdb"

    Scenario: Simple Incremental Backup with AO/CO statistics w/ filter
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "public.ao_table"
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "8760" tuples in "bkdb" for table "public.ao_table"

    @backupfire
    Scenario: pg_stat_last_operation registers truncate for regular tables
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user truncates "ao_table, co_table" tables in "bkdb"
        Then pg_stat_last_operation registers the truncate for tables "ao_table, co_table" in "bkdb" in schema "public"

    @backupfire
    Scenario: pg_stat_last_operation registers truncate for partition tables
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        When the user truncates "testschema.ao_part_table" tables in "bkdb"
        When the user truncates "testschema.co_part_table_1_prt_p1_2_prt_2" tables in "bkdb"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_1" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_2" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_3" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_1" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_2" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_3" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p1_2_prt_1" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation registers the truncate for tables "co_part_table_1_prt_p1_2_prt_2" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p1_2_prt_3" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_1" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_2" in "bkdb" in schema "testschema"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_3" in "bkdb" in schema "testschema"

    Scenario: Simple Incremental Backup with TRUNCATE
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user truncates "testschema.ao_table" tables in "bkdb"
        And the user truncates "testschema.co_table" tables in "bkdb"
        And the numbers "1" to "10000" are inserted into "testschema.ao_table" tables in "bkdb"
        And the numbers "1" to "10000" are inserted into "testschema.co_table" tables in "bkdb"
        And the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And the user truncates "testschema.ao_part_table" tables in "bkdb"
        And the partition table "testschema.ao_part_table" in "bkdb" is populated with similar data
        And the user truncates "testschema.co_table" tables in "bkdb"
        And the numbers "1" to "10000" are inserted into "testschema.co_table" tables in "bkdb"
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        And the timestamp from gpcrondump is stored
        And the user truncates "testschema.ao_part_table" tables in "bkdb"
        And the partition table "testschema.ao_part_table" in "bkdb" is populated with similar data
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts2"
        And the timestamp from gpcrondump is stored
        Then verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "21" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Simple Incremental Backup to test ADD COLUMN
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "ao" table "testschema.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a "co" table "testschema.co_index_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And the user adds column "foo" with type "int" and default "0" to "testschema.ao_table" table in "bkdb"
        And the user adds column "foo" with type "int" and default "0" to "testschema.co_table" table in "bkdb"
        And the user adds column "foo" with type "int" and default "0" to "testschema.ao_part_table" table in "bkdb"
        And the user adds column "foo" with type "int" and default "0" to "testschema.co_part_table" table in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "23" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupfire
    Scenario: Non compressed incremental backup
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb -z"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "bkdb" in schema "testschema"
        And table "testschema.heap_table" is dropped in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that the data of "11" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the plan file is created for the latest timestamp

    Scenario: Rollback Insert
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And an insert on "testschema.ao_table" in "bkdb" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Rollback Truncate Table
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And a truncate on "testschema.ao_table" in "bkdb" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Rollback Alter table
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored
        And an alter on "testschema.ao_table" in "bkdb" is rolled back
        And table "testschema.co_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/bar_plan2"
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupsmoke
    Scenario: Out of Sync timestamp
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        And there is a fake timestamp for "30160101010101"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print There is a future dated backup on the system preventing new backups to stdout

    Scenario: Test for a quadrillion rows
        Given the test is initialized
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "bkdb"
        And the row "1, 0, 1000000000000000, 1000000000000000, 0, 0" is inserted into "public.tuple_count_table" in "bkdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "bkdb" for ao table "testschema.t1"
        Then an exception should be raised with "Exceeded backup max tuple count of 1 quadrillion rows per table for:"

    Scenario: Test for a quadrillion rows in 2 files
        Given the test is initialized
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "bkdb"
        And the row "1, 0, 999999999999999, 999999999999999, 0, 0" is inserted into "public.tuple_count_table" in "bkdb"
        And the row "2, 0, 1, 1, 0, 0" is inserted into "public.tuple_count_table" in "bkdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "bkdb" for ao table "testschema.t1"
        Then an exception should be raised with "Exceeded backup max tuple count of 1 quadrillion rows per table for:"

    Scenario: Test for less than a quadrillion rows in 2 files
        Given the test is initialized
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "bkdb"
        And the row "1, 0, 999999999999998, 999999999999998, 0, 0" is inserted into "public.tuple_count_table" in "bkdb"
        And the row "2, 0, 1, 1, 0, 0" is inserted into "public.tuple_count_table" in "bkdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "bkdb" for ao table "testschema.t1"
        Then the get_partition_state result should contain "testschema, t1, 999999999999999"

    Scenario: Test gpcrondump dump deletion only (-o option)
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the user runs "gpcrondump -a -x bkdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o --cleanup-date=20130101"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And older backup directories "20130101" exists
        And older backup directories "20130102" exists
        When the user runs "gpcrondump -o --cleanup-total=2"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directories "20130102" should not exist
        And the dump directory for the stored timestamp should exist

    Scenario: Negative test gpcrondump dump deletion only (-o option)
        Given the test is initialized
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And the user runs "gpcrondump -a -x bkdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -o"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print No old backup sets to remove to stdout
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o --cleanup-date=20130100"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Timestamp dump 20130100 does not exist. to stdout
        When the user runs "gpcrondump -o --cleanup-total=2"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Unable to delete 2 backups.  Only have 1 backups. to stdout
        When the user runs "gpcrondump --cleanup-date=20130100"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Must supply -c or -o with --cleanup-date option to stdout
        When the user runs "gpcrondump --cleanup-total=2"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Must supply -c or -o with --cleanup-total option to stdout

    @backupfire
    Scenario: Test gpcrondump dump deletion (-c option)
        Given the test is initialized
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And the user runs "gpcrondump -a -x bkdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -a -x bkdb -c"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist
        And older backup directories "20130101" exists
        And older backup directories "20130102" exists
        When the user runs "gpcrondump -a -x bkdb -c --cleanup-total=2"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directories "20130102" should not exist
        And the dump directory for the stored timestamp should exist
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -a -x bkdb -c --cleanup-date=20130101"
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist

    Scenario: Verify the gpcrondump -g option works with full backup
        Given the test is initialized
        When the user runs "gpcrondump -a -x bkdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    @backupfire
    Scenario: Verify the gpcrondump -g option works with incremental backup
        Given the test is initialized
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb -g --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    Scenario: Verify the gpcrondump history table works by default with full and incremental backups
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that there is a "heap" table "gpcrondump_history" in "bkdb"
        And verify that the table "gpcrondump_history" in "bkdb" has dump info for the stored timestamp
        When the user runs "gpcrondump -a -x bkdb --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that there is a "heap" table "gpcrondump_history" in "bkdb"
        And verify that table "gpcrondump_history" in "bkdb" has "2" rows
        And verify that the table "gpcrondump_history" in "bkdb" has dump info for the stored timestamp

    Scenario: Verify the gpcrondump -H option should not create history table
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -H"
        Then gpcrondump should return a return code of 0
        Then verify that there is no table "public.gpcrondump_history" in "bkdb"

    Scenario: Verify the gpcrondump -H and -h option can not be specified together with DDBoost
        Given the test is initialized
        When the user runs "gpcrondump -a -x bkdb -H -h --ddboost"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -H option cannot be selected with -h option to stdout

    @backupfire
    Scenario: Verify gpdbrestore -s option works with full backup
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a "co" table "public.co_table" in "bkdb2" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a -x bkdb2"
        And gpcrondump should return a return code of 0
        And the database "bkdb2" does not exist
        And the user runs "gpdbrestore -e -s bkdb -a"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that database "bkdb2" does not exist

    @backupfire
    Scenario: Verify gpdbrestore -s option works with incremental backup
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a "co" table "public.co_table" in "bkdb2" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a -x bkdb2"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a -x bkdb2 --incremental"
        And gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And the database "bkdb2" does not exist
        And the user runs "gpdbrestore -e -s bkdb -a"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that database "bkdb2" does not exist

    @backupfire
    Scenario: gpdbrestore -u option with full backup
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And there are no backup files
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupsmoke
    Scenario: gpdbrestore -u option with incremental backup
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -u /tmp --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And there are no backup files
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: gpcrondump -x with multiple databases
        Given the test is initialized
        And database "bkdb2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a "co" table "public.co_table" in "bkdb2" with data
        When the user runs "gpcrondump -a -x bkdb -x bkdb2"
        And the timestamp for database dumps "bkdb, bkdb2" are stored
        And gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And all the data from "bkdb2" is saved for verification
        And the user runs "gpdbrestore -e -s bkdb -a"
        And gpdbrestore should return a return code of 0
        And the user runs "gpdbrestore -e -s bkdb2 -a"
        And gpdbrestore should return a return code of 0
        Then verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the data of "2" tables in "bkdb2" is validated after restore
        And the dump timestamp for "bkdb, bkdb2" are different
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the tuple count of all appendonly tables are consistent in "bkdb2"

    Scenario: gpdbrestore with --table-file option
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--table-file /tmp/table_file_foo"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the restored table "public.ao_table" in database "bkdb" is analyzed
        And verify that the restored table "public.co_table" in database "bkdb" is analyzed
        Then the file "/tmp/table_file_foo" is removed from the system

    @backupsmoke
    Scenario: Incremental restore with extra full backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the numbers "1" to "100" are inserted into "ao_table" tables in "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupfire
    Scenario: gpcrondump should not track external tables
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is an external table "ext_tab" in "bkdb" with data for file "/tmp/ext_tab"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "4" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that there is no "public.ext_tab" in the "dirty_list" file in " "
        And verify that there is no "public.ext_tab" in the "table_list" file in " "
        Then the file "/tmp/ext_tab" is removed from the system

    Scenario: Full backup with -T option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "public.ao_part_table" in "fullbkdb"
        And verify that there is no table "public.heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    @backupfire
    Scenario: gpdbrestore with -T option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_index_table -a"
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "public.ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data

    @backupfire
    Scenario: Full backup and restore with -T and --truncate
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs "gpdbrestore -T public.ao_index_table -a --truncate" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that the restored table "public.ao_index_table" in database "bkdb" is analyzed
        And the user runs "gpdbrestore -T public.ao_part_table_1_prt_p1_2_prt_1 -a --truncate" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data

    Scenario: Full backup and restore with -T and --truncate with dropped table
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And table "public.heap_table" is dropped in "bkdb"
        And the user runs "gpdbrestore -T public.heap_table -a --truncate" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbrestore should print Skipping truncate of bkdb.public.heap_table because the relation does not exist to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data

    Scenario: Full backup -T with truncated table
        Given the test is initialized
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "bkdb"
        And the user runs "gpdbrestore -T public.ao_part_table_1_prt_p2_2_prt_3 -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table_1_prt_p2_2_prt_3" in "bkdb" with data
        And verify that the restored table "public.ao_part_table_1_prt_p2_2_prt_3" in database "bkdb" is analyzed

    Scenario: Full backup -T with no schema name supplied
        Given the test is initialized
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user truncates "public.ao_index_table" tables in "bkdb"
        And the user runs "gpdbrestore -T ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        Then gpdbrestore should print No schema name supplied to stdout

    Scenario: Full backup with gpdbrestore -T for DB with FUNCTION having DROP SQL
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_function_with_drop_table.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When table "public.ao_index_table" is dropped in "bkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data

    Scenario: Incremental restore with table filter
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And table "ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table -T public.co_table"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "bkdb" have been restored

    Scenario: Incremental restore with invalid table filter
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.heap_table -T public.invalid -q"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Tables \[\'public.invalid\'\] not found in backup to stdout

    @backupfire
    Scenario: gpdbrestore -L with -u option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in "/tmp" dir contains "Backup Type: Full"
        When the user runs gpdbrestore with the stored timestamp and options "-L -u /tmp"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Table public.ao_part_table to stdout
        And gpdbrestore should print Table public.heap_table to stdout

    @backupfire
    Scenario: gpdbrestore -b with -u option for Full timestamp
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore on dump date directory with options "-u /tmp"
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupfire
    Scenario: gpdbrestore with -s and -u options for full backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And the user runs "gpdbrestore -e -s bkdb -u /tmp -a"
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupfire
    Scenario: gpdbrestore with -s and -u options for incremental backup
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -u /tmp --incremental"
        Then gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And the user runs "gpdbrestore -e -s bkdb -u /tmp -a"
        And gpdbrestore should return a return code of 0
        And verify that the data of "12" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: gpdbrestore -b option should display the timestamps in sorted order
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -x bkdb --incremental -a"
        And gpcrondump should return a return code of 0
        And the user runs "gpcrondump -x bkdb --incremental -a"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp and options "-b"
        And the timestamps should be printed in sorted order

    Scenario: gpdbrestore -R option should display the timestamps in sorted order
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -x bkdb --incremental -u /tmp -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -x bkdb --incremental -u /tmp -a"
        And gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        Then the user runs gpdbrestore with "-R" option in path "/tmp"
        And the timestamps should be printed in sorted order

    @scale
    Scenario: Dirty File Scale Test
        Given the test is initialized
        And there are "240" "heap" tables "public.heap_table" with data in "bkdb"
        And there are "10" "ao" tables "public.ao_table" with data in "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_table_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb"
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_3, public.ao_table_4, public.ao_table_5, public.ao_table_6" in "bkdb" has no rows
        And verify that tables "public.ao_table_7, public.ao_table_8, public.ao_table_9, public.ao_table_10" in "bkdb" has no rows
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore

    @scale
    Scenario: Dirty File Scale Test for partitions
        Given the test is initialized
        And there are "240" "heap" tables "public.heap_table" with data in "bkdb"
        And there is a "ao" partition table "public.ao_table" in "bkdb" with data
        Then data for partition table "ao_table" with partition level "1" is distributed across all segments on "bkdb"
        And verify that partitioned tables "ao_table" in "bkdb" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb"
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_1_prt_p1_2_prt_3, public.ao_table_1_prt_p2_2_prt_1" in "bkdb" has no rows
        And verify that tables "public.ao_table_1_prt_p2_2_prt_2, public.ao_table_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore

    Scenario: Config files have the same timestamp as the backup set
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that the config files are backed up with the stored timestamp

    Scenario Outline: Incremental Backup With column-inserts, inserts and oids options
        Given the test is initialized
        When the user runs "gpcrondump -a --incremental -x bkdb <options>"
        Then gpcrondump should print --inserts, --column-inserts, --oids cannot be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        Examples:
        | options         |
        | --inserts        |
        | --oids          |
        | --column-inserts |

    Scenario: Test gpcrondump and gpdbrestore verbose option
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --verbose"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental --verbose"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--verbose"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @backupfire
    Scenario: Incremental table filter gpdbrestore with different schema for same tablenames
        Given the test is initialized
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table1" in "bkdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "testschema.ao_part_table" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "testschema.ao_part_table1" with partition level "1" is distributed across all segments on "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb"
        And table "testschema.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "testschema.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -e -T public.ao_part_table -T testschema.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_3" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_2" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_1" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_3" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_2" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p1_2_prt_1" in "bkdb"
        And verify that there is no table "testschema.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb"

    @backupfire
    Scenario: Incremental table filter gpdbrestore with noplan option
        Given the test is initialized
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb"
        And verify that partitioned tables "ao_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "bkdb" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb"
        Then gp_restore should return a return code of 0
        And the user runs "gpdbrestore -T public.ao_part_table -a --noplan" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_3, public.ao_part_table_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_2, public.ao_part_table_1_prt_p2_2_prt_2" in "bkdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_1, public.ao_part_table_1_prt_p2_2_prt_1" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_3, public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_2, public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_1, public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb" has no rows

    @backupsmoke
    Scenario: gpdbrestore list_backup option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options " "
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout
        Then "plan" file is removed under " "
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options "-a"
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout

    @backupsmoke
    Scenario: gpdbrestore list_backup option with -e
        Given the test is initialized
        When the user runs "gpdbrestore -a -e --list-backup -t 20160101010101"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify --list-backup and -e together to stdout

    @backupfire
    Scenario: gpdbrestore list_backup option with -T table filter
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options "-T public.heap_table"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify -T and --list-backup together to stdout

    @backupfire
    Scenario: gpdbrestore list_backup option with full timestamp
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options " "
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print --list-backup is not supported for restore with full timestamps to stdout

    Scenario: Full and Incremental Backup with -g option using named pipes
        Given the test is initialized
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --list-backup-files -K 20130101010101 -g"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored
        And "pipes" file should be created under " "
        And "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20130101010101" under " "
        And the named pipes are validated against the timestamp "20130101010101" under " "
        And the named pipe script for the "dump" is run for the files under " "
        When the user runs "gpcrondump -a -x bkdb -K 20130101010101 -g"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb -K 20130101020101 -g --incremental"
        Then gpcrondump should return a return code of 0

    Scenario: Incremental Backup and Restore with named pipes
        Given the test is initialized
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --list-backup-files -K 20130101010101"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored
        And "pipes" file should be created under " "
        And "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20130101010101" under " "
        And the named pipes are validated against the timestamp "20130101010101" under " "
        And the named pipe script for the "dump" is run for the files under " "
        When the user runs "gpcrondump -a -x bkdb -K 20130101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a -x bkdb --list-backup-files -K 20140101010101 --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp key "20140101010101" for gpcrondump is stored
        And "pipes" file should be created under " "
        And "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20140101010101" under " "
        And the named pipes are validated against the timestamp "20140101010101" under " "
        And the named pipe script for the "dump" is run for the files under " "
        And table "public.ao_part_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -K 20140101010101 --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the named pipe script for the "restore" is run for the files under " "
        And all the data from "bkdb" is saved for verification
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb" is validated after restore
        When the named pipe script for the "restore" is run for the files under " "
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_part_table"
        Then gpdbrestore should print \[WARNING\]:-Skipping validation of tables in dump file due to the use of named pipes to stdout
        And close all opened pipes

    Scenario: Incremental Backup and Restore with -t filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -t public.ao_index_table -t public.heap_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -x bkdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Filtering tables using: to stdout
        And gpcrondump should print Prefix                        = foo to stdout
        And gpcrondump should print Full dump timestamp           = [0-9]{14} to stdout
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering bkdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table to stdout
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"

    Scenario: Incremental Backup and Restore with -T filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -T public.ao_part_table -T public.heap_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering bkdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"

    Scenario: Incremental Backup and Restore with --table-file filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        And there is a table-file "/tmp/table_file_1" with tables "public.ao_index_table, public.heap_table"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --table-file /tmp/table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the temp files "table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering bkdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table to stdout
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"

    Scenario: Incremental Backup and Restore with --exclude-table-file filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        And there is a table-file "/tmp/exclude_table_file_1" with tables "public.ao_part_table, public.heap_table"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --exclude-table-file /tmp/exclude_table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the temp files "exclude_table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x bkdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering bkdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"

    Scenario: Full Backup with option -t and non-existant table
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -t cool.dude -t public.heap_table"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print does not exist in to stdout

    Scenario: Full Backup with option -T and non-existant table
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -T public.heap_table -T cool.dude"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print does not exist in to stdout
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that there is no table "public.heap_table" in "bkdb"

    Scenario: Negative test gpdbrestore -G with incremental timestamp
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb "
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb --incremental -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "global" file should be created under " "
        And table "public.ao_part_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "-G"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to locate global file to stdout

    Scenario: Full Backup and Restore using gp_dump with no-lock
        Given the test is initialized
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data and 1000000 rows
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs the "gp_dump --gp-s=p --gp-c --no-lock bkdb" in a worker pool "w1"
        And this test sleeps for "2" seconds
        And the worker pool "w1" is cleaned up
        Then gp_dump should return a return code of 0

    Scenario: Dump and Restore metadata
        Given the test is initialized
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_metadata.sql bkdb"
        And the user runs "gpcrondump -a -x bkdb -K 30160101010101 -u /tmp"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And all the data from the remote segments in "bkdb" are stored in path "/tmp" for "full"
        And verify that the file "/tmp/db_dumps/30160101/gp_dump_status_0_2_30160101010101" does not contain "reading indexes"
        And verify that the file "/tmp/db_dumps/30160101/gp_dump_status_1_1_30160101010101" contains "reading indexes"
        Given database "bkdb" is dropped and recreated
        When the user runs "gpdbrestore -a -t 30160101010101 -u /tmp"
        Then gpdbrestore should return a return code of 0
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/check_metadata.sql bkdb > /tmp/check_metadata.out"
        And verify that the contents of the files "/tmp/check_metadata.out" and "gppylib/test/behave/mgmt_utils/steps/data/check_metadata.ans" are identical
        And the directory "/tmp/db_dumps" is removed or does not exist
        And the directory "/tmp/check_metadata.out" is removed or does not exist

    Scenario: Restore -T for incremental dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "bkdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql bkdb"
        When the user runs "gpcrondump --incremental -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "ao_index_table, co_index_table, heap_index_table" in "bkdb" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        When the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 2000 to stdout 4 times
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is a "co" table "public.co_index_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_index_table" in "bkdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    Scenario: Restore -T for full dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "bkdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify the metadata dump file syntax under " " for comments and types
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "public.ao_index_table, public.co_index_table, public.heap_index_table" in "bkdb" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        When the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 1000 to stdout 4 times
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is a "co" table "public.co_index_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_index_table" in "bkdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    Scenario: Restore -T for full dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql bkdb"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" bkdb"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_3" with index "heap_index_3" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_3 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_3 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_3 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_3 TO select_group;" bkdb"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" bkdb"""
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb > /tmp/describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb > /tmp/privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb > /tmp/describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb > /tmp/privileges_heap_index_table_2_before"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        When the user runs "gpcrondump -x bkdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When there is a backupfile of tables "customer.heap_index_table_1, customer.heap_index_table_2, customer.heap_index_table_3" in "bkdb" exists for validation
        And table "customer.heap_index_table_1" is dropped in "bkdb"
        And table "customer.heap_index_table_2" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        And the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 1000 to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "bkdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "bkdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_3" in "bkdb" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb > /tmp/describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb > /tmp/privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb > /tmp/describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb > /tmp/privileges_heap_index_table_2_after"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        Then verify that the contents of the files "/tmp/describe_heap_index_table_1_before" and "/tmp/describe_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/describe_heap_index_table_2_before" and "/tmp/describe_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/privileges_heap_index_table_1_before" and "/tmp/privileges_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/privileges_heap_index_table_2_before" and "/tmp/privileges_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And the file "/tmp/describe_heap_index_table_1_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_1_after" is removed from the system
        And the file "/tmp/privileges_heap_index_table_1_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_1_after" is removed from the system
        And the file "/tmp/describe_heap_index_table_2_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_2_after" is removed from the system
        And the file "/tmp/privileges_heap_index_table_2_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_2_after" is removed from the system
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system

    Scenario: Restore -T for incremental dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql bkdb"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" bkdb"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" bkdb"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" bkdb"""
        And there is a "heap" table "customer.heap_index_table_3" with index "heap_index_3" compression "None" in "bkdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_3 owner to customer" bkdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_3 TO customer;" bkdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_3 TO test_group;" bkdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_3 TO select_group;" bkdb"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" bkdb"""
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb > /tmp/describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb > /tmp/privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb > /tmp/describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb > /tmp/privileges_heap_index_table_2_before"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        When the user runs "gpcrondump -x bkdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And table "customer.heap_index_table_1" is assumed to be in dirty state in "bkdb"
        And table "customer.heap_index_table_2" is assumed to be in dirty state in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql bkdb"
        When the user runs "gpcrondump --incremental -x bkdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When there is a backupfile of tables "customer.heap_index_table_1, customer.heap_index_table_2, customer.heap_index_table_3" in "bkdb" exists for validation
        And table "customer.heap_index_table_1" is dropped in "bkdb"
        And table "customer.heap_index_table_2" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        And the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 2000 to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "bkdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "bkdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_3" in "bkdb" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' bkdb > /tmp/describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' bkdb > /tmp/privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' bkdb > /tmp/describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' bkdb > /tmp/privileges_heap_index_table_2_after"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        Then verify that the contents of the files "/tmp/describe_heap_index_table_1_before" and "/tmp/describe_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/describe_heap_index_table_2_before" and "/tmp/describe_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/privileges_heap_index_table_1_before" and "/tmp/privileges_heap_index_table_1_after" are identical
        And verify that the contents of the files "/tmp/privileges_heap_index_table_2_before" and "/tmp/privileges_heap_index_table_2_after" are identical
        And verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And the file "/tmp/describe_heap_index_table_1_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_1_after" is removed from the system
        And the file "/tmp/privileges_heap_index_table_1_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_1_after" is removed from the system
        And the file "/tmp/describe_heap_index_table_2_before" is removed from the system
        And the file "/tmp/describe_heap_index_table_2_after" is removed from the system
        And the file "/tmp/privileges_heap_index_table_2_before" is removed from the system
        And the file "/tmp/privileges_heap_index_table_2_after" is removed from the system
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system

    Scenario: Redirected Restore Full Backup and Restore without -e option
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=bkdb2 -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb2" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb2" with data

    Scenario: Full Backup and Restore with -e option
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=bkdb2 -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb2" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb2" with data

    Scenario: Incremental Backup and Redirected Restore
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -x bkdb -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs "gpdbrestore --redirect=bkdb2 -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb2" is validated after restore

    Scenario: Full backup and redirected restore with -T
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user truncates "public.ao_index_table" tables in "bkdb"
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=bkdb2 -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb2" with data

    Scenario: Full backup and redirected restore with -T and --truncate
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=bkdb2 --truncate -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        And gpdbrestore should print Failure from truncating tables, FATAL:  database "bkdb2" does not exist to stdout
        And there is a "ao" table "public.ao_index_table" in "bkdb2" with data
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=bkdb2 --truncate -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb2" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data

    Scenario: Incremental redirected restore with table filter
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And table "ao_table" is assumed to be in dirty state in "bkdb"
        And table "co_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table -T public.co_table --redirect=bkdb2"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "bkdb2" have been restored

    Scenario: Full Backup and Redirected Restore with --prefix option
        Given the test is initialized
        And the prefix "foo" is stored
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --redirect=bkdb2"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "public.heap_table" in "bkdb2" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb2" with data

    Scenario: Full Backup and Redirected Restore with --prefix option for multiple databases
        Given the test is initialized
        And the prefix "foo" is stored
        And database "bkdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And there is a backupfile of tables "public.heap_table" in "bkdb2" exists for validation
        When the user runs "gpcrondump -a -x bkdb -x bkdb2 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --redirect=bkdb3"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "public.heap_table" in "bkdb3" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb3" with data

    Scenario: Full Backup and Restore with the master dump file missing
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "rm $MASTER_DATA_DIRECTORY/db_dumps/*/gp_dump_1_1*"
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to find .* or .*. Skipping restore. to stdout

    Scenario: Incremental Backup and Restore with the master dump file missing
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x bkdb -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "rm $MASTER_DATA_DIRECTORY/db_dumps/*/gp_dump_1_1*"
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to find .* or .*. Skipping restore. to stdout

    Scenario: Uppercase Database Name Full Backup and Restore
        Given the test is initialized
        And database "TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    Scenario: Uppercase Database Name Full Backup and Restore using -s option with and without quotes
        Given the test is initialized
        And database "TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore -s TESTING -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data
        And the user runs "gpdbrestore -s "TESTING" -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    Scenario: Uppercase Database Name Incremental Backup and Restore
        Given the test is initialized
        And database "TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "TESTING" with data
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table" is assumed to be in dirty state in "TESTING"
        When the user runs "gpcrondump -x TESTING -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "TESTING" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that the data of "11" tables in "TESTING" is validated after restore

    Scenario: Full backup and Restore should create the gp_toolkit schema with -e option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        And the gp_toolkit schema for "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb" is validated after restore
        And the gp_toolkit schema for "bkdb" is verified after restore

    Scenario: Incremental backup and Restore should create the gp_toolkit schema with -e option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        And the gp_toolkit schema for "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x bkdb --incremental -a"
        THen gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "bkdb" is validated after restore
        And the gp_toolkit schema for "bkdb" is verified after restore

    Scenario: Redirected Restore should create the gp_toolkit schema with or without -e option
        Given the test is initialized
        And the database "bkdb2" does not exist
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        And the gp_toolkit schema for "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=bkdb2 -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb2" is validated after restore
        And the gp_toolkit schema for "bkdb2" is verified after restore
        And the user runs "gpdbrestore --redirect=bkdb2 -e -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "bkdb2" is validated after restore
        And the gp_toolkit schema for "bkdb2" is verified after restore

    Scenario: gpdbrestore with noanalyze
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And database "bkdb" is dropped and recreated
        And the user runs "gpdbrestore --noanalyze -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbestore should print Analyze bypassed on request to stdout
        And verify that the data of "10" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: gpdbrestore without noanalyze
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbestore should print Commencing analyze of bkdb database to stdout
        And gpdbestore should print Analyze of bkdb completed without error to stdout
        And verify that the data of "10" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Writable Report/Status Directory Full Backup and Restore without --report-status-dir option
        Given the test is initialized
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print gp-r to stdout
        And gpdbestore should not print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"

    Scenario: Writable Report/Status Directory Full Backup and Restore with --report-status-dir option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --report-status-dir=/tmp -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should print gp-r to stdout
        And gpdbestore should print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that report file is generated in /tmp
        And verify that status file is generated in /tmp
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"

    Scenario: Writable Report/Status Directory Full Backup and Restore with -u option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a -u /tmp -K 20160101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore -u /tmp -e -a -t 20160101010101"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should print gp-r to stdout
        And gpdbestore should print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that report file is generated in /tmp/db_dumps/20160101
        And verify that status file is generated in /tmp/db_dumps/20160101
        And the backup files in "/tmp" are deleted

    Scenario: Writable Report/Status Directory Full Backup and Restore with no write access -u option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a -u /tmp -K 20160101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "chmod -R 555 /tmp/db_dumps"
        And the user runs "gpdbrestore -u /tmp -e -a -t 20160101010101"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print gp-r to stdout
        And gpdbestore should not print --status= to stdout
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And the user runs command "chmod -R 777 /tmp/db_dumps"
        And the backup files in "/tmp" are deleted

    @backupfire
    Scenario: Filtered Full Backup with Partition Table
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that the data of "9" tables in "bkdb" is validated after restore

    @backupfire
    Scenario: Filtered Incremental Backup with Partition Table
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data
        And verify that the data of "9" tables in "bkdb" is validated after restore

    Scenario: gpdbrestore runs ANALYZE on restored table only
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And the database "bkdb" is analyzed
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user truncates "public.ao_index_table" tables in "bkdb"
        And the user deletes rows from the table "heap_table" of database "bkdb" where "column1" is "1088"
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that the restored table "public.ao_index_table" in database "bkdb" is analyzed
        And verify that the table "public.heap_table" in database "bkdb" is not analyzed

    Scenario: Gpcrondump with --email-file option
        Given the test is initialized
        And database "testdb1" is dropped and recreated
        And database "testdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "testdb1" with data
        And there is a "heap" table "public.heap_table" in "testdb2" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "gppylib/test/behave/mgmt_utils/steps/data/test_email_details.yaml" stores email details is in proper format
        When the user runs "gpcrondump -a -x testdb1 -x testdb2 --email-file gppylib/test/behave/mgmt_utils/steps/data/test_email_details.yaml --verbose"
        Then gpcrondump should return a return code of 0
        And verify that emails are sent to the given contacts with appropriate messages after backup of "testdb1,testdb2"
        And the mail_contacts file does not exist

    Scenario: Gpcrondump without mail_contacts
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the mail_contacts file does not exist
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print unable to send dump email notification to stdout as warning

    Scenario: Negative case for gpcrondump with --email-file option
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "gppylib/test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml" stores email details is not in proper format
        When the user runs "gpcrondump -a -x bkdb --email-file gppylib/test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml --verbose"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print file is not formatted properly to stdout
        And the mail_contacts file does not exist

    @backupfire
    Scenario: Full Backup with multiple -S option and Restore
        Given the test is initialized
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "testschema.heap_table, schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -S schema_heap -S testschema"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "bkdb"
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data

    @backupfire
    Scenario: Full Backup with option -S and Restore
        Given the test is initialized
        And there is schema "schema_heap, schema_ao" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -S schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data

    @backupfire
    Scenario: Full Backup with option -s and Restore
        Given the test is initialized
        And there is schema "schema_heap, schema_ao" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "bkdb"

    @backupfire
    Scenario: Full Backup with option --exclude-schema-file and Restore
        Given the test is initialized
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb" exists for validation
        And there is a file "exclude_file" with tables "testschema|schema_ao"
        When the user runs "gpcrondump -a -x bkdb --exclude-schema-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "bkdb"
        And verify that there is no table "testschema.heap_table" in "bkdb"

    @backupfire
    Scenario: Full Backup with option --schema-file and Restore
        Given the test is initialized
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap|schema_ao"
        When the user runs "gpcrondump -a -x bkdb --schema-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data
        And verify that there is no table "testschema.heap_table" in "bkdb"

    @wip
    Scenario: Incremental Backup and Restore with -s filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -s schema_ao -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_schema" file should be created under " "
        And verify that the "schema" file in " " dir contains "schema_ao"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data

    @wip
    Scenario: Incremental Backup and Restore with --schema-file filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "schema_ao,schema_heap"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --schema-file /tmp/schema_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_schema" file should be created under " "
        And verify that the "schema" file in " " dir contains "schema_ao"
        And partition "3" is added to partition table "schema_ao.ao_part_table" in "bkdb"
        And partition "2" is dropped from partition table "schema_ao.ao_part_table" in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data

    @wip
    Scenario: Incremental Backup and Restore with --exclude-schema-file filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "testschema,schema_heap"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --exclude-schema-file /tmp/schema_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_schema" file should be created under " "
        And verify that the "schema" file in " " dir contains "schema_ao"
        And table "schema_ao.ao_index_table" is dropped in "bkdb"
        And partition "3" is added to partition table "schema_ao.ao_part_table" in "bkdb"
        And partition "2" is dropped from partition table "schema_ao.ao_part_table" in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that there is no table "schema_ao.ao_index_table" in "bkdb"
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "bkdb"

    @wip
    Scenario: Incremental Backup and Restore with -S filter for Full
        Given the test is initialized
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "testschema,schema_heap"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -S testschema -S schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_schema" file should be created under " "
        And verify that the "schema" file in " " dir contains "schema_ao"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And table "schema_ao.ao_part_table" is assumed to be in dirty state in "bkdb"
        And table "schema_ao.ao_index_table" is dropped in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And verify that there is no table "schema_heap.heap_table" in "bkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data

    Scenario: Full Backup and Restore with option --change-schema
        Given the test is initialized
        And there is schema "schema_heap, schema_ao, schema_new" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap.heap_table|schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs "gpdbrestore --change-schema=schema_new -a --table-file include_file" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new.heap_table" of "heap" type in "bkdb" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new.ao_part_table" of "ao" type in "bkdb" with same data as table "schema_ao.ao_part_table"

    Scenario: Incremental Backup and Restore with option --change-schema
        Given the test is initialized
        And there is schema "schema_heap, schema_ao, schema_new" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap.heap_table|schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x bkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And table "schema_ao.ao_part_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And the user runs "gpdbrestore --change-schema=schema_new -a --table-file include_file" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new.heap_table" of "heap" type in "bkdb" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new.ao_part_table" of "ao" type in "bkdb" with same data as table "schema_ao.ao_part_table"

    Scenario: Full backup and restore with statistics
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And the database "bkdb" is analyzed
        When the user runs "gpcrondump -a -x bkdb --dump-stats"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "statistics" file should be created under " "
        When the user runs gpdbrestore with the stored timestamp and options "--restore-stats"
        Then gpdbrestore should return a return code of 0
        And verify that the restored table "public.heap_table" in database "bkdb" is analyzed
        And verify that the restored table "public.ao_part_table" in database "bkdb" is analyzed
        And database "bkdb" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs gpdbrestore with the stored timestamp and options "--restore-stats only"
        Then gpdbrestore should return a return code of 2
        When the user runs gpdbrestore with the stored timestamp and options "--restore-stats only" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that the restored table "public.heap_table" in database "bkdb" is analyzed
        And verify that the restored table "public.ao_part_table" in database "bkdb" is analyzed

    Scenario: Backup and restore with statistics and table filters
        Given the test is initialized
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And the database "bkdb" is analyzed
        When the user runs "gpcrondump -a -x bkdb --dump-stats -t public.heap_table -t public.heap_index_table"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "statistics" file should be created under " "
        And verify that the "statistics" file in " " dir does not contain "Schema: public, Table: ao_part_table"
        And database "bkdb" is dropped and recreated
        When the user runs gpdbrestore with the stored timestamp and options "-T public.heap_index_table --noanalyze"
        Then gpdbrestore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--restore-stats -T public.heap_table" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that the table "public.heap_index_table" in database "bkdb" is not analyzed
        And verify that the restored table "public.heap_table" in database "bkdb" is analyzed

    Scenario: Simple full backup and restore with special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        When the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        And the directory "/tmp/special_table_data.ans" is removed or does not exist
        And the directory "/tmp/special_table_data.out" is removed or does not exist
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: Funny characters in the table name or schema name for gpcrondump
        Given the test is initialized
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char.sql testdb"
        When the user runs command "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb -t Schema\\t,1.Table\\n\!1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb -T Schema\\t,1.Table\\n\!1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb -s Schema\\t,1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb -S Schema\\t,1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb --table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb --exclude-table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb --schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char_schema.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout
        When the user runs command "gpcrondump -a -x testdb --exclude-schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char_schema.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Name has an invalid character "\\t" "\\n" "!" "," "." to stdout

    Scenario: Funny characters in the table name or schema name for gpdbrestore
        Given the test is initialized
        And database "testdb" exists
        And there is a "heap" table "public.table1" in "testdb" with data
        When the user runs command "gpcrondump -a -x testdb"
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout
        When the user runs gpdbrestore with the stored timestamp and options "-T pub\\t\\nlic.!,\\t\\n.1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout
        When the user runs gpdbrestore with the stored timestamp and options "--redirect A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout
        When the user runs command "gpdbrestore -s "A\\t\\n.,!1""
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout
        When the user runs gpdbrestore with the stored timestamp and options "-T public.table1 --change-schema A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout
        When the user runs gpdbrestore with the stored timestamp and options "-S A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Name has an invalid character to stdout

    Scenario: gpcrondump with -T option where table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        #And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/filter_test.sql template1"
        And there is a list of files "ao,heap" of tables " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 , S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " exists for validation

        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " -T " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 "." co_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify with backedup file "ao" that there is a "ao" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify with backedup file "heap" that there is a "heap" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpcrondump with --exclude-table-file option where table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        And there is a list of files "ao,heap" of tables " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 , S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " exists for validation
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --exclude-table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/exclude-table-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify with backedup file "ao" that there is a "ao" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify with backedup file "heap" that there is a "heap" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpcrondump with --table-file option where table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        And there is a list of files "ao,heap" of tables " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 , S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " exists for validation
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/table-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs gpdbrestore with the stored timestamp and options " " without -e option
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "ao" that there is a "ao" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify with backedup file "heap" that there is a "heap" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpcrondump with -t option where table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        And there is a list of files "ao" of tables " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " exists for validation
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " -t " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 "." ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs gpdbrestore with the stored timestamp and options " " without -e option
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "ao" that there is a "ao" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpcrondump with --schema-file, --exclude-schema-file, -s and -S option when schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"

        # --schema-file option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/schema-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        And the user runs gpdbrestore with the stored timestamp
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        And verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # -s option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " -s " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        And the user runs gpdbrestore with the stored timestamp
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        And verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # --exclude-schema-file option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --exclude-schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/schema-file.txt"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And verify that there is no table " ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And verify that there is no table " heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "

        # -S option
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " -S " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And verify that there is no table " ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "
        And verify that there is no table " heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "

        # cleanup
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/drop_special_database.sql template1"
        And the directory "/tmp/specail_schema_data.out" is removed or does not exist
        And the directory "/tmp/specail_schema_data.ans" is removed or does not exist

    Scenario: Gpcrondump, --table-file, --exclude-table-file, --schema-file and --exclude-schema-file if file contains double quoted table and schema name then gpcrondump should error out finding table does not exists
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        # --table-file=<filename> option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/table-file-double-quote.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print does not exist to stdout
        # --exclude-table-file=<filename> option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --exclude-table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/table-file-double-quote.txt"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print does not exist to stdout
        And gpcrondump should print All exclude table names have been removed due to issues to stdout
        # --schema-file
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/schema-file-double-quote.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print does not exist to stdout
        # --exclude-schema-file
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --exclude-schema-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/schema-file-double-quote.txt"
        Then gpcrondump should return a return code of 0
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/drop_special_database.sql template1"

    Scenario: Gpdbrestore, --change-schema option does not work with -S schema level restore option
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -T " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 "." ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 " --change-schema=" S\`~@#\$%^&*()_-+[{]}|\\;: \\'\"/?><1 " -S " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><2 " " with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpcrondump should print -S option cannot be used with --change-schema option to stdout

    Scenario: Gpdbrestore with --table-file, -T, --truncate and --change-schema options when table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        And there is a list of files "ao,heap" of tables " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 , S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " exists for validation

        # --table-file=<filename> option
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--table-file gppylib/test/behave/mgmt_utils/steps/data/special_chars/table-file.txt"
        Then gpdbrestore should return a return code of 0
        And verify with backedup file "ao" that there is a "ao" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . ao_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify with backedup file "heap" that there is a "heap" table " S`~@#$%^&*()-+[{]}|\;: \'"/?><1 . heap_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 " with data
        And verify that there is no table " co_T`~@#$%^&*()-+[{]}|\;: \'"/?><1 " in " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 "

        # -T, --change-schema options
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/add_schema.sql template1"
        And the user runs command "psql -f  psql -c """select * from \" S\`~@#\$%^&*()-+[{]}|\\;: \\'\"\"/?><1 \".\" ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"\"/?><1 \" order by 1""" -d " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 "  > /tmp/table_data.ans"
        And the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -T " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 "." ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 " --change-schema=" S\`~@#\$%^&*()_-+[{]}|\\;: \\'\"/?><2 " -a --truncate" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f  psql -c """select * from \" S\`~@#\$%^&*()_-+[{]}|\\;: \\'\"\"/?><2 \".\" ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"\"/?><1 \" order by 1""" -d " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 "  > /tmp/table_data.out"
        And verify that the contents of the files "/tmp/table_data.ans" and "/tmp/table_data.out" are identical

        # --truncate option
        When the user runs "gpdbrestore -T " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 "." ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 " -a --truncate" with the stored timestamp
        And the user runs command "psql -f  psql -c """select * from \" S\`~@#\$%^&*()-+[{]}|\\;: \\'\"\"/?><1 \".\" ao_T\`~@#\$%^&*()-+[{]}|\\;: \\'\"\"/?><1 \" order by 1""" -d " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 "  > /tmp/table_data.out"
        Then verify that the contents of the files "/tmp/table_data.ans" and "/tmp/table_data.out" are identical

        # cleanup
        And the directory "/tmp/table_data.ans" is removed or does not exist
        And the directory "/tmp/table_data.out" is removed or does not exist
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpcrondump with --incremental option when table name, schema name and database name contains special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"

        # --incremental dump whole database
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        And the user runs gpdbrestore with the stored timestamp
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # cleanup
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/drop_special_database.sql template1"
        And the directory "/tmp/special_table_data.out" is removed or does not exist
        And the directory "/tmp/special_table_data.ans" is removed or does not exist

    Scenario: gpdbrestore, --redirect option with special db name, and all table name, schema name and database name contain special character
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"

        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        When the user runs gpdbrestore with the stored timestamp and options "--redirect " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;2 "" without -e option
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;2 " > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # cleanup
        And the directory "/tmp/special_table_data.out" is removed or does not exist
        And the directory "/tmp/special_table_data.ans" is removed or does not exist
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;2 ""

    Scenario: gpdbrestore, -s option with special chars
        Given the test is initialized
        When the user runs command "gpdbrestore -s " DB\`~@#\$%^&*()_-+[{]}|\\;:.;\n\t \\'/?><;2 ""
        Then gpdbrestore should print Name has an invalid character to stdout

    Scenario: gpdbrestore, -S option, -S truncate option schema level restore with special chars in schema name
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"

        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.ans"
        When the user runs gpdbrestore with the stored timestamp and options "-S " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 ""
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # -S with truncate option
        When the user runs "gpdbrestore -S " S\`~@#\$%^&*()-+[{]}|\\;: \\'\"/?><1 " -a --truncate" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_table_data.out"
        Then verify that the contents of the files "/tmp/special_table_data.out" and "/tmp/special_table_data.ans" are identical

        # cleanup
        And the directory "/tmp/special_table_data.out" is removed or does not exist
        And the directory "/tmp/special_table_data.ans" is removed or does not exist
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: gpdbrestore, --noplan option with special chars in database name, schema name, and table name
        Given the test is initialized
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_table.sql template1"

        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""
        Then gpcrondump should return a return code of 0
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/insert_into_special_ao_table.sql template1"
        When the user runs command "gpcrondump -a -x " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_ao_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_ao_table_data.ans"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/truncate_special_ao_table.sql template1"
        And the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option
        And the user runs command "psql -f gppylib/test/behave/mgmt_utils/steps/data/special_chars/select_from_special_ao_table.sql " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 " > /tmp/special_ao_table_data.out"
        Then verify that the contents of the files "/tmp/special_ao_table_data.out" and "/tmp/special_ao_table_data.ans" are identical

        # cleanup
        And the directory "/tmp/special_ao_table_data.out" is removed or does not exist
        And the directory "/tmp/special_ao_table_data.ans" is removed or does not exist
        And the user runs command "dropdb " DB\`~@#\$%^&*()_-+[{]}|\\;: \\'/?><;1 ""

    Scenario: Restoring a nonexistent table should fail with clear error message
        Given the test is initialized
        And there is a "heap" table "heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "-T public.heap_table2 -q"
        Then gpdbrestore should return a return code of 2
        Then gpdbrestore should print Tables \[\'public.heap_table2\'\] to stdout
        Then gpdbrestore should not print Issue with 'ANALYZE' of restored table 'public.heap_table2' in 'bkdb' database to stdout

    Scenario: Absolute path should be provided with -u option for gpcrondump
        Given the test is initialized
        And there is a "heap" table "heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u foo/db"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print is not an absolute path to stdout

    @backupfire
    Scenario: Full Backup with option --schema-file with prefix option and Restore
        Given the test is initialized
        And the prefix "foo" is stored
        And there is schema "schema_heap, schema_ao, testschema" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, testschema.heap_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap|schema_ao"
        When the user runs "gpcrondump -a -x bkdb --schema-file include_file --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "bkdb" with data
        And verify that there is no table "testschema.heap_table" in "bkdb"

    Scenario: Simple Full Backup with AO/CO statistics w/ filter
        Given the test is initialized
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "public.ao_table"
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "4380" tuples in "bkdb" for table "public.ao_table"

    Scenario: Simple Full Backup with AO/CO statistics w/ filter schema
        Given the test is initialized
        And there is schema "schema_ao, testschema" exists in "bkdb"
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_foo" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--noaostats"
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "public.ao_table"
        And verify that there are "0" tuples in "bkdb" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "schema_ao.ao_part_table"
        And verify that there are "0" tuples in "bkdb" for table "testschema.ao_foo"
        When the user runs gpdbrestore with the stored timestamp and options "-S schema_ao -S testschema" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "public.ao_table"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_1"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_2"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_3"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_1"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_2"
        And verify that there are "730" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_3"
        And verify that there are "4380" tuples in "bkdb" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "schema_ao.ao_part_table"
        When the user runs gpdbrestore with the stored timestamp and options "-S schema_ao -S testschema --truncate" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that there are "0" tuples in "bkdb" for table "public.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "public.ao_table"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_1"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_2"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p1_2_prt_3"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_1"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_2"
        And verify that there are "365" tuples in "bkdb" for table "testschema.ao_foo_1_prt_p2_2_prt_3"
        And verify that there are "2190" tuples in "bkdb" for table "schema_ao.ao_index_table"
        And verify that there are "0" tuples in "bkdb" for table "schema_ao.ao_part_table"
    Scenario: Restore with --redirect option should not rely on existance of dumped database
        Given the test is initialized
        When the user runs "gpcrondump -a -x bkdb"
        And the timestamp from gpcrondump is stored
        And the database "bkdb" does not exist
        And database "bkdb1" is dropped and recreated
        When the user runs gpdbrestore with the stored timestamp and options "--redirect=bkdb1"
        Then gpdbrestore should return a return code of 0
        And the database "bkdb1" does not exist

    Scenario: Tables with same name but different partitioning should not pollute one another's dump during backup
        Given the test is initialized
        And there is schema "withpartition" exists in "bkdb"
        And there is schema "withoutpartition" exists in "bkdb"
        And there is schema "aaa" exists in "bkdb"
        And there is a "heap" table "withoutpartition.rank" in "bkdb" with data
        And there is a "heap" partition table "withpartition.rank" in "bkdb" with data
        When the user runs "psql -c 'alter table withpartition.rank_1_prt_p1 set SCHEMA aaa;' bkdb"
        Then psql should return a return code of 0
        And the user runs "psql -c 'alter table withpartition.rank_1_prt_p2 set SCHEMA aaa;' bkdb"
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb -t withoutpartition.rank"
        And the timestamp from gpcrondump is stored
        Then verify the metadata dump file does not contain "ALTER TABLE rank_1_prt_p1 SET SCHEMA aaa"
        Then verify the metadata dump file does not contain "ALTER TABLE rank_1_prt_p2 SET SCHEMA aaa"
        When the user runs "gpcrondump -a -x bkdb -t withpartition.rank"
        And the timestamp from gpcrondump is stored
        Then verify the metadata dump file does contain "ALTER TABLE rank_1_prt_p1 SET SCHEMA aaa"
        Then verify the metadata dump file does contain "ALTER TABLE rank_1_prt_p2 SET SCHEMA aaa"

    Scenario: Database owner can be assigned to role containing special characters
        Given the test is initialized
        When the user runs "psql -c 'DROP ROLE IF EXISTS "Foo%user"' -d bkdb"
        Then psql should return a return code of 0
        When the user runs "psql -c 'CREATE ROLE "Foo%user"' -d bkdb"
        Then psql should return a return code of 0
        When the user runs "psql -c 'ALTER DATABASE bkdb OWNER TO "Foo%user"' -d bkdb"
        Then psql should return a return code of 0
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "cdatabase" file in " " dir contains "OWNER = "Foo%user""
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the owner of "bkdb" is "Foo%user"
        And database "bkdb" is dropped and recreated
        When the user runs "psql -c 'DROP ROLE "Foo%user"' -d bkdb"
        Then psql should return a return code of 0

    @exclude_schema
    Scenario: Exclude schema (-S) should not dump pg_temp schemas
        Given the test is initialized
        And the user runs the command "psql bkdb -f 'gppylib/test/behave/mgmt_utils/steps/data/gpcrondump/create_temp_schema_in_transaction.sql'" in the background without sleep
        When the user runs "gpcrondump -a -S good_schema -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Then read pid from file "gppylib/test/behave/mgmt_utils/steps/data/gpcrondump/pid_leak" and kill the process
        And the temporary file "gppylib/test/behave/mgmt_utils/steps/data/gpcrondump/pid_leak" is removed
        And waiting "2" seconds
        And verify that the "dump" file in " " dir does not contain "pg_temp"
        And the user runs command "dropdb bkdb"

    @ignore_pg_temp
    Scenario: pg_temp should be ignored from gpcrondump --table_file option and -t option when given
        Given the test is initialized
        And there is a "ao" table "public.foo4" in "bkdb" with data
        # NOTE: pg_temp does not exist in the database at all. We are just valiating that we can still
        # do a backup given that the user tries to backup a temporary table
        # --table-file option ignore pg_temp
        And there is a table-file "/tmp/table_file_foo4" with tables "public.foo4, pg_temp_1337.foo4"
        When the user runs "gpcrondump -a --table-file /tmp/table_file_foo4 -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there are "2190" tuples in "bkdb" for table "public.foo4"
        # -t option ignore pg_temp
        When the user runs "gpcrondump -a -t public.foo4 -t pg_temp_1337.foo4 -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there are "2190" tuples in "bkdb" for table "public.foo4"

    Scenario: Schema level restore with gpdbrestore -S option for views, sequences, and functions
        Given the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/schema_level_test_workload.sql template1"
        When the user runs command "gpcrondump -a -x schema_level_test_db"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -S s1 -a -e" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that sequence "id_seq" exists in schema "s1" and database "schema_level_test_db"
        And verify that view "v1" exists in schema "s1" and database "schema_level_test_db"
        And verify that function "increment(integer)" exists in schema "s1" and database "schema_level_test_db"
        And verify that table "apples" exists in schema "s1" and database "schema_level_test_db"
        And the user runs command "dropdb schema_level_test_db"

    # THIS SHOULD BE THE LAST TEST
    @backupfire
    Scenario: cleanup for backup feature
        Given the backup files in "/tmp" are deleted
