@backup
Feature: Validate command line arguments

    @backupfire
    Scenario: Entering an invalid argument
        When the user runs "gpcrondump -X"
        Then gpcrondump should print no such option: -X error message
        And gpcrondump should return a return code of 2

    @backupfire
    Scenario: Incremental Backup With Table filters
        Given the database is running
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
        When the user runs "gpcrondump -K 20140101010101 -x bkdb,fulldb -a"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print multi-database backup is not supported with -K option to stdout
        When the user runs "gpcrondump -a --incremental -x bkdb,fulldb"
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
    @slb
    Scenario: Schema level backup With Table filters
        Given the database is running
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "bkdb" with data
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
    @truncate
    Scenario: gpdbrestore with --truncate option
        When the user runs "gpdbrestore -t 20140101010101 --truncate -a" 
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print --truncate can be specified only with -T or --table-file option to stdout
        When the user runs "gpdbrestore -t 20140101010101 --truncate -e -T public.foo -a" 
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify --truncate and -e together to stdout


    ### 
    ### Any tests that require this data set should come after
    ###
    @backupsmoke
    @dataset
    Scenario Outline: partition table test with data on all segments, with/without compression
        Given there is a "<tabletype>" partition table "<tablename>" with compression "<compressiontype>" in "<dbname>" with data
        Then data for partition table "<tablename>" with partition level "<partlevel>" is distributed across all segments on "<dbname>"
        And verify that partitioned tables "<tablename>" in "bkdb" have 6 partitions
        Examples:
        | tabletype | tablename           | compressiontype | dbname | partlevel |
        | ao        | ao_part_table       | None            | bkdb   | 1         |
        | co        | co_part_table       | None            | bkdb   | 1         |
        | heap      | heap_part_table     | None            | bkdb   | 1         |
        | ao        | ao_part_table_comp  | quicklz         | bkdb   | 1         |
        | co        | co_part_table_comp  | zlib            | bkdb   | 1         |

    @backupsmoke
    @dataset
    Scenario Outline: regular table test with data on all segments, with/without compression
        Given there is a "<tabletype>" table "<tablename>" with compression "<compressiontype>" in "<dbname>" with data
        Then data for table "<tablename>" is distributed across all segments on "<dbname>"
        Examples:
        | tabletype | tablename      | compressiontype | dbname |
        | ao        | ao_table       | None            | bkdb |
        | co        | co_table       | None            | bkdb |
        | heap      | heap_table     | None            | bkdb |
        | ao        | ao_table_comp  | quicklz         | bkdb |
        | co        | co_table_comp  | zlib            | bkdb |

    @backupsmoke
    @dataset
    Scenario Outline: regular table test with data on all segments, with/without compression and with indexes
        Given there is a "<tabletype>" table "<tablename>" with index "<indexname>" compression "<compressiontype>" in "<dbname>" with data
        Then data for table "<tablename>" is distributed across all segments on "<dbname>"
        Examples:
        | tabletype | tablename            | compressiontype | dbname | indexname      |
        | ao        | ao_index_table       | None            | bkdb   | ao_in          |
        | co        | co_index_table       | None            | bkdb   | co_in          |
        | heap      | heap_index_table     | None            | bkdb   | heap_in        |
        | ao        | ao_index_table_comp  | quicklz         | bkdb   | ao_comp_in     |
        | co        | co_index_table_comp  | zlib            | bkdb   | co_comp_in     |

    @backupsmoke
    @dataset
    Scenario: Partition tables with mixed storage types
        Given there is a mixed storage partition table "part_mixed_1" in "bkdb" with data
        Then data for partition table "part_mixed_1" with partition level "1" is distributed across all segments on "bkdb"
        And verify that storage_types of the partition table "part_mixed_1" are as expected in "bkdb"
    
	@backupsmoke
    @dataset
    Scenario: Partition tables with external partition 
        Given the user runs "echo > /tmp/backup_gpfdist_dummy"
        And the user runs "gpfdist -p 8098 -d /tmp &"
        And there is a partition table "part_external" has external partitions of gpfdist with file "backup_gpfdist_dummy" on port "8098" in "bkdb" with data
        Then data for partition table "part_external" with partition level "0" is distributed across all segments on "bkdb"
        And verify that storage_types of the partition table "part_external" are as expected in "bkdb"

    @backupsmoke
    Scenario: Negative test for Incremental Backup
        Given the database is running
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And partition "1" in partition level "0" of partition table "part_external" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And there are no backup files
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout 

    Scenario: Negative test for Incremental Backup - Incremental with -u after a full backup to default directory
        Given the database is running
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And there are no backup files
        And the backup files in "/tmp" are deleted
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout 

    Scenario: Negative test for Incremental Backup - Incremental to default directory after a full backup with -u option
        Given the database is running
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And there are no backup files
        And the backup files in "/tmp" are deleted
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout 

    Scenario: Negative test for Incremental Backup - Incremental after a full backup of different database
        Given the database is running
        And there is a "heap" table "heap_table2" with compression "None" in "fullbkdb" with data
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And there are no backup files
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print No full backup found for incremental to stdout
    
    Scenario: Dirty table list check on recreating a table with same data and contents
        Given the database is running
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When the "public.ao_table" is recreated with same data in "testdb"
        And the user runs "gpcrondump -a --incremental -x testdb"
        And the timestamp from gpcrondump is stored 
        Then gpcrondump should return a return code of 0
        And "public.ao_table" is marked as dirty in dirty_list file
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that plan file has latest timestamp for "public.ao_table"
        
    Scenario: Negative test for missing state file 
        Given the database is running
        And there are no backup files
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
        Given the database is running
        And there are no backup files
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
        Given the database is running
        And there are no backup files
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Validating disk space to stdout 
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "last_operation" files are generated under " " for stored "full" timestamp
        When partition "1" of partition table "ao_part_table, co_part_table_comp, part_mixed_1" is assumed to be in dirty state in "bkdb" in schema "public"
        When partition "1" in partition level "0" of partition table "part_external" is assumed to be in dirty state in "bkdb" in schema "public"
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
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And the user runs gp_restore with the the stored timestamp and subdir in "bkdb"
        And gp_restore should return a return code of 0
        And verify that partitioned tables "ao_part_table, co_part_table, heap_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table_comp, co_part_table_comp" in "bkdb" have 6 partitions
        And verify that partitioned tables "part_external" in "bkdb" have 5 partitions in partition level "0"
        And verify that partitioned tables "ao_part_table, co_part_table_comp" in "bkdb" has 5 empty partitions
        And verify that partitioned tables "co_part_table, ao_part_table_comp" in "bkdb" has 6 empty partitions
        And verify that partitioned tables "heap_part_table" in "bkdb" has 0 empty partitions
        And verify that there is a "heap" table "heap_table" in "bkdb"
        And verify that there is a "heap" table "heap_index_table" in "bkdb"
        And verify that there is partition "1" of "ao" partition table "ao_part_table" in "bkdb" in "public"
        And verify that there is partition "1" of "co" partition table "co_part_table_comp" in "bkdb" in "public"
        And verify that there is partition "1" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "2" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "3" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "1" of mixed partition table "part_mixed_1" with storage_type "c"  in "bkdb" in "public"
        And verify that there is partition "2" in partition level "0" of mixed partition table "part_external" with storage_type "x"  in "bkdb" in "public"
        And verify that tables "public.ao_table, public.co_table, public.ao_table_comp, public.co_table_comp" in "bkdb" has no rows
        And verify that tables "public.co_index_table, public.ao_index_table_comp, public.co_index_table_comp" in "bkdb" has no rows
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore
        And verify that the distribution policy of all the tables in "bkdb" are validated after restore
        And verify that the incremental file has the stored timestamp
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: Increments File Check With Complicated Scenario
        Given the database is running
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        And the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        And verify that the incremental file has all the stored timestamps

    Scenario: Incremental File Check With Different Directory
        Given the database is running
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
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
    Scenario: Simple Incremental Backup with -u option
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And partition "1" in partition level "0" of partition table "part_external" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in "/tmp" dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under "/tmp"
        And the subdir from gpcrondump is stored 
        And all the data from "bkdb" is saved for verification
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And the user runs gp_restore with the stored timestamp and subdir in "bkdb" and backup_dir "/tmp"
        And gp_restore should return a return code of 0
        And verify that partitioned tables "ao_part_table, co_part_table, heap_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table_comp, co_part_table_comp" in "bkdb" have 6 partitions
        And verify that partitioned tables "part_external" in "bkdb" have 5 partitions in partition level "0"
        And verify that partitioned tables "ao_part_table, co_part_table_comp" in "bkdb" has 5 empty partitions
        And verify that partitioned tables "co_part_table, ao_part_table_comp" in "bkdb" has 6 empty partitions
        And verify that partitioned tables "heap_part_table" in "bkdb" has 0 empty partitions
        And verify that there is a "heap" table "heap_table" in "bkdb"
        And verify that there is a "heap" table "heap_index_table" in "bkdb"
        And verify that there is partition "1" of "ao" partition table "ao_part_table" in "bkdb" in "public"
        And verify that there is partition "2" in partition level "0" of mixed partition table "part_external" with storage_type "x"  in "bkdb" in "public"
        And verify that there is partition "1" of "co" partition table "co_part_table_comp" in "bkdb" in "public"
        And verify that there is partition "1" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "2" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that there is partition "3" of "heap" partition table "heap_part_table" in "bkdb" in "public"
        And verify that tables "public.ao_table, public.co_table, public.ao_table_comp, public.co_table_comp" in "bkdb" has no rows
        And verify that tables "public.co_index_table, public.ao_index_table_comp, public.co_index_table_comp" in "bkdb" has no rows
        And verify that the data of the dirty tables under "/tmp" in "bkdb" is validated after restore
        And verify that the distribution policy of all the tables in "bkdb" are validated after restore

    Scenario: gpdbrestore with -R for full dump
        Given the database is running
        And there are no backup files        
        And the backup files in "/tmp" are deleted
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored 
        And all the data from "bkdb" is saved for verification
        And the subdir from gpcrondump is stored 
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And all the data from the remote segments in "bkdb" are stored in path "/tmp" for "full"
        And the user runs gpdbrestore with "-R" option in path "/tmp"
        And gpdbrestore should return a return code of 0
        And verify that the data of "74" tables in "bkdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    Scenario: gpdbrestore with -R for incremental dump
        Given the database is running
        And there are no backup files        
        And the backup files in "/tmp" are deleted
        When the user runs "gpcrondump -a -x bkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        When table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp"
        And the timestamp from gpcrondump is stored 
        Then gpcrondump should return a return code of 0
        And all files for full backup have been removed in path "/tmp"
        And all the data from the remote segments in "bkdb" are stored in path "/tmp" for "inc"
        And the user runs gpdbrestore with "-R" option in path "/tmp"
        And gpdbrestore should print -R is not supported for restore with incremental timestamp to stdout

    @backupsmoke
    Scenario: Full Backup and Restore
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
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
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    @backupfire
    Scenario: Full Backup and Restore with -y
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -y /tmp -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And gpcrondump should print -y is a deprecacted option.  Report files are always generated with the backup set. to stdout
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    Scenario: Full Backup and Restore using gp_dump
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gp_dump --gp-d=db_dumps --gp-s=p --gp-c fullbkdb"
        Then gp_dump should return a return code of 0
        And the timestamp from gp_dump is stored and subdir is " " 
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the user runs gp_restore with the the stored timestamp and subdir in "fullbkdb"
        And gp_restore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And there are no report files in the master data directory

    Scenario: gpdbrestore -L with Full Backup
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        When the user runs gpdbrestore with the stored timestamp and options "-L" 
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Table public.ao_part_table to stdout 
        And gpdbrestore should print Table public.heap_table to stdout 

    @backupfire
    Scenario: gpcrondump -b with Full and Incremental backup
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb -b"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Bypassing disk space check to stdout
        And gpcrondump should not print Validating disk space to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb -b --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Bypassing disk space checks for incremental backup to stdout
        And gpcrondump should not print Validating disk space to stdout

    @backupfire
    Scenario: gpdbrestore -b with Full timestamp
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp and options "-b"
        Then gpdbrestore should return a return code of 0

    Scenario: Output info gpdbrestore 
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Full Database to stdout
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Table Restore to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Incremental Restore to stdout
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Restore type               = Incremental Table Restore to stdout

    Scenario: Output info gpcrondump
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Dump type                                = Full to stdout 
        And table "public.ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Dump type                                = Incremental to stdout 

    Scenario: gpcrondump -G with Full timestamp
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb -G"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And table "public.ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb -G --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "

    @Gonly
    Scenario: Backup and restore with -G only
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And the user runs "psql -c 'CREATE ROLE foo_user' fullbkdb"
        And verify that a role "foo_user" exists in database "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb -G"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And the user runs "psql -c 'DROP ROLE foo_user' fullbkdb"
        And the user runs gpdbrestore with the stored timestamp and options "-G only" 
        And gpdbrestore should return a return code of 0
        And verify that a role "foo_user" exists in database "fullbkdb"
        And verify that there is no table "heap_table" in "fullbkdb"
        And the user runs "psql -c 'DROP ROLE foo_user' fullbkdb"

    @valgrind
    Scenario: Valgrind test of gp_dump incremental
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental fullbkdb" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump incremental with table file
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table, ao_part_table" in "fullbkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "fullbkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental fullbkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump full with table file
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table, ao_part_table" in "fullbkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "fullbkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c fullbkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental with table file
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table, ao_part_table" in "fullbkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "fullbkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only fullbkdb --incremental --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent full with table file
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table, ao_part_table" in "fullbkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "fullbkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only fullbkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental
        Given the database is running
        And the backup files in "/tmp" are deleted
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only fullbkdb --incremental" and options " " 

    @valgrind
    Scenario: Valgrind test of gp_restore for incremental backup
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "fullbkdb"
        And the user runs "gpcrondump -a -x fullbkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs valgrind with "gp_restore" and options "-i --gp-i --gp-l=p -d fullbkdb --gp-c"

    @valgrind
    Scenario: Valgrind test of gp_restore_agent for incremental backup
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "fullbkdb"
        And the user runs "gpcrondump -a -x fullbkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs valgrind with "gp_restore_agent" and options "--gp-c /bin/gunzip -s --post-data-schema-only --target-dbid 1 -d fullbkdb"

    @backupfire
    Scenario: gp_dump timestamp test
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gp_dump --gp-d=db_dumps --gp-k=20121228111527 --gp-s=p --gp-c fullbkdb"
        Then gp_dump should return a return code of 0
        And the timestamp from gp_dump is stored and subdir is " " 
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the user runs "gp_restore -i --gp-k 20121228111527 --gp-d db_dumps --gp-i --gp-r db_dumps --gp-l=p -d fullbkdb --gp-c" 
        And gp_restore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    @backupfire
    Scenario: gp_dump with invalid timestamp
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gp_dump --gp-d=db_dumps --gp-k=aa121228111527 --gp-s=p --gp-c fullbkdb"
        Then gp_dump should return a return code of 1 
        And gp_dump should print Invalid timestamp key provided error message
    
    @backupfire
    Scenario: Full Backup with option -t and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        And the temp files "include_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x fullbkdb -t public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "include_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is no table "ao_part_table" in "fullbkdb"

    @backupfire
    Scenario: Full Backup with option -T and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        And the temp files "exclude_dump_tables" are removed from the system
        When the user runs "gpcrondump -a -x fullbkdb -T public.heap_table"
        Then gpcrondump should return a return code of 0
        And the temp files "exclude_dump_tables" are not created in the system
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And verify that there is no table "heap_table" in "fullbkdb"

    @backupfire
    Scenario: Full Backup with option -s and Restore
        Given the database is running
        And there is schema "schema_heap, schema_ao" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "fullbkdb"

    @backupfire
    Scenario: Full Backup with option --exclude-table-file and Restore
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "co_part_table" in "fullbkdb" exists for validation
        And there is a file "exclude_file" with tables "public.heap_table,public.ao_part_table" 
        When the user runs "gpcrondump -a -x fullbkdb --exclude-table-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "co" table "co_part_table" in "fullbkdb" with data
        And verify that there is no table "ao_part_table" in "fullbkdb"
        And verify that there is no table "heap_table" in "fullbkdb"

    @backupfire
    Scenario: Full Backup with option --table-file and Restore
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a backupfile of tables "ao_part_table,heap_table" in "fullbkdb" exists for validation        
        And there is a file "include_file" with tables "public.heap_table,public.ao_part_table" 
        When the user runs "gpcrondump -a -x fullbkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is no table "co_part_table" in "fullbkdb"

    Scenario: gpcrondump should generate a timestamp and pass it to gp_dump
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        And the timestamp key is stored
        Then gpcrondump should print --gp-k to stdout
        And gpcrondump should return a return code of 0
        And the timestamp in the report file should be same as timestamp key
        And there should be dump files with filename having timestamp key in "fullbkdb"

    Scenario: plan file creation in directory
        Given the database is running  
        And there are no backup files
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
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
        Given the database is running  
        And there is a "heap" table "heap_table" with compression "None" in "smalldb" with data
        And there is a "heap" table "heap2_table" with compression "None" in "smalldb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "smalldb" with data
        And there is a "ao" table "ao2_table" with compression "quicklz" in "smalldb" with data
        And there is a "co" table "co_table" with compression "None" in "smalldb" with data
        And there is a "co" table "co2_table" with compression "None" in "smalldb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x smalldb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the user runs "gpcrondump -a -x smalldb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts1"
        And "dirty_list" file should be created under " "
        And table "public.co_table" is assumed to be in dirty state in "smalldb"
        And the user runs "gpcrondump -a -x smalldb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts2"
        And "dirty_list" file should be created under " "
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file is validated against "data/plan1"

    Scenario: Simple Plan File Test With Partitions
        Given the database is running  
        And there are no backup files
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts1"
        And "dirty_list" file should be created under " "
        And partition "1" of partition table "co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And the user runs "gpcrondump -a -x bkdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts2"
        And "dirty_list" file should be created under " "
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file is validated against "data/plan2"

    Scenario: No plan file generated
        Given the database is running  
        And there are no backup files
        And partition "1" of partition table "ao_part_table, co_part_table_comp" is assumed to be in dirty state in "bkdb" in schema "public"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should not be created under " "

    @backupfire
    Scenario: Incremental backup of Non-public schema
        Given the database is running
        And there are no "dirty_backup_list" tempfiles
        And database "schematestdb" is created if not exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "heap" table "pepper.heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_index_table" with index "ao_index" compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_index_table" with index "co_index" compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And table "pepper.ao_index_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts1"
        And "dirty_list" file should be created under " "
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And the user runs "gpcrondump -a -x schematestdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts2"
        And "dirty_list" file should be created under " "
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file is validated against "data/plan3"
        And verify there are no "dirty_backup_list" tempfiles

    Scenario: Schema only restore of incremental backup
        Given the database is running  
        And there are no backup files
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

    @backupfire
    Scenario: Multiple - Incremental backup and restore
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "heap" table "pepper.heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x schematestdb"
        And the full backup timestamp from gpcrondump is stored        
        And gpcrondump should return a return code of 0
        And table "pepper.ao_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And table "pepper.heap_table" is deleted in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "pepper.ao_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "schematestdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "pepper.heap_table" in "schematestdb"
        And verify that the data of "10" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"
        And verify that the plan file is created for the latest timestamp 

    @backupsmoke
    Scenario: Simple Incremental Backup for Multiple Schemas with common tablenames
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "heap" table "pepper.heap_table" with compression "None" in "schematestdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "schematestdb" with data
        And there is a "heap" partition table "pepper.heap_part_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table2" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And table "public.ao_table" is assumed to be in dirty state in "schematestdb"
        And table "pepper.ao_table2" is assumed to be in dirty state in "schematestdb"
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And partition "2" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "public"
        And partition "3" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And partition "3" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "public"
        And table "public.heap_table2" is deleted in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan4"
        And verify that the data of "33" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupfire
    Scenario: pg_stat_last_operation registers truncate for regular tables 
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        When the user truncates "ao_table, co_table" tables in "schematestdb"
        Then pg_stat_last_operation registers the truncate for tables "ao_table, co_table" in "schematestdb" in schema "public"

    @backupfire
    Scenario: pg_stat_last_operation registers truncate for partition tables 
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        When the user truncates "pepper.ao_part_table" tables in "schematestdb"
        When the user truncates "pepper.co_part_table_1_prt_p1_2_prt_2" tables in "schematestdb"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_1" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_2" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p1_2_prt_3" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_1" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_2" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "ao_part_table_1_prt_p2_2_prt_3" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p1_2_prt_1" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation registers the truncate for tables "co_part_table_1_prt_p1_2_prt_2" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p1_2_prt_3" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_1" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_2" in "schematestdb" in schema "pepper"
        Then pg_stat_last_operation does not register the truncate for tables "co_part_table_1_prt_p2_2_prt_3" in "schematestdb" in schema "pepper"

    Scenario: Simple Incremental Backup to test TRUNCATE
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user truncates "pepper.ao_table" tables in "schematestdb"
        And the user truncates "pepper.co_table" tables in "schematestdb"
        And the numbers "1" to "10000" are inserted into "pepper.ao_table" tables in "schematestdb"
        And the numbers "1" to "10000" are inserted into "pepper.co_table" tables in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And the user truncates "pepper.co_table" tables in "schematestdb"
        And the numbers "1" to "10000" are inserted into "pepper.co_table" tables in "schematestdb"
        And table "pepper.co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan5"
        And verify that the data of "2" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    Scenario: IB TRUNCATE partitioned table 
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And the user truncates "pepper.ao_part_table" tables in "schematestdb"
        And the partition table "pepper.ao_part_table" in "schematestdb" is populated with similar data
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan6"
        And verify that the data of "18" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupsmoke
    Scenario: IB TRUNCATE partitioned table 2 times
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And the user truncates "pepper.ao_part_table" tables in "schematestdb"
        And the partition table "pepper.ao_part_table" in "schematestdb" is populated with same data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And the user truncates "pepper.ao_part_table" tables in "schematestdb"
        And the partition table "pepper.ao_part_table" in "schematestdb" is populated with similar data
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan6"
        And verify that the data of "18" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    Scenario: Simple Incremental Backup to test ADD COLUMN
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table2" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And the user adds column "foo" with type "int" and default "0" to "pepper.ao_table" table in "schematestdb"
        And the user adds column "foo" with type "int" and default "0" to "pepper.co_table" table in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan7"
        And verify that the data of "4" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupsmoke
    Scenario: Incremental Backup to test ADD COLUMN with partitions
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb -v"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And the user adds column "foo" with type "int" and default "0" to "pepper.ao_part_table" table in "schematestdb"
        And the user adds column "foo" with type "int" and default "0" to "pepper.co_part_table" table in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan8"
        And verify that the data of "19" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupfire
    Scenario: Non compressed incremental backup 
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "heap" table "pepper.heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x schematestdb -z"
        And the full backup timestamp from gpcrondump is stored        
        And gpcrondump should return a return code of 0
        And table "pepper.ao_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And table "pepper.heap_table" is deleted in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "pepper.ao_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a -x schematestdb --incremental -z"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "schematestdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "pepper.heap_table" in "schematestdb"
        And verify that the data of "10" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"
        And verify that the plan file is created for the latest timestamp 

    Scenario: Rollback Insert
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And an insert on "pepper.ao_table" in "schematestdb" is rolled back  
        And table "pepper.co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan9"
        And verify that the data of "2" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    Scenario: Rollback Truncate Table
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And a truncate on "pepper.ao_table" in "schematestdb" is rolled back  
        And table "pepper.co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan9"
        And verify that the data of "2" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    Scenario: Rollback Alter table
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the full backup timestamp from gpcrondump is stored 
        And an alter on "pepper.ao_table" in "schematestdb" is rolled back  
        And table "pepper.co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts1"
        Then the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And "dirty_list" file should be created under " "
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And the plan file is validated against "data/plan9"
        And verify that the data of "2" tables in "schematestdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupsmoke
    Scenario: Out of Sync timestamp
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there is a list to store the incremental backup timestamps
        And there are no backup files
        # this test will break after 2021 year
        And there is a fake timestamp for "20210130093000"
        When the user runs "gpcrondump -a -x schematestdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print There is a future dated backup on the system preventing new backups to stdout

    Scenario: Test for a quadrillion rows
        Given the database is running 
        And the database "tempdb" does not exist
        And database "tempdb" exists
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "tempdb"
        And the row "1, 0, 1000000000000000, 1000000000000000, 0, 0" is inserted into "public.tuple_count_table" in "tempdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "tempdb" for ao table "pepper.t1" 
        Then an exception should be raised with "Exceeded backup max tuple count of 1 quadrillion rows per table for:" 

    Scenario: Test for a quadrillion rows in 2 files
        Given the database is running 
        And the database "tempdb" does not exist
        And database "tempdb" exists
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "tempdb"
        And the row "1, 0, 999999999999999, 999999999999999, 0, 0" is inserted into "public.tuple_count_table" in "tempdb"
        And the row "2, 0, 1, 1, 0, 0" is inserted into "public.tuple_count_table" in "tempdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "tempdb" for ao table "pepper.t1"
        Then an exception should be raised with "Exceeded backup max tuple count of 1 quadrillion rows per table for:" 

    Scenario: Test for less than a quadrillion rows in 2 files
        Given the database is running 
        And the database "tempdb" does not exist
        And database "tempdb" exists
        And there is a fake pg_aoseg table named "public.tuple_count_table" in "tempdb"
        And the row "1, 0, 999999999999998, 999999999999998, 0, 0" is inserted into "public.tuple_count_table" in "tempdb"
        And the row "2, 0, 1, 1, 0, 0" is inserted into "public.tuple_count_table" in "tempdb"
        When the method get_partition_state is executed on table "public.tuple_count_table" in "tempdb" for ao table "pepper.t1"
        Then the get_partition_state result should contain "pepper, t1, 999999999999999"

    @backupfire
    Scenario: Verify the gpcrondump -o option is not broken
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        And the user runs "gpcrondump -a -x schematestdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o" 
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist 

    @backupfire
    Scenario: Verify the gpcrondump -c  option is not broken
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        And the user runs "gpcrondump -a -x schematestdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -c -x schematestdb -a" 
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist 

    Scenario: Verify the gpcrondump -g option is not broken
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    @backupfire
    Scenario: Verify the gpcrondump -g option with incremental is not broken
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x schematestdb -g --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    Scenario: Verify the gpcrondump -h option works with full and incremental backups
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb -h"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that there is a "heap" table "gpcrondump_history" in "schematestdb"
        And verify that the table "gpcrondump_history" in "schematestdb" has dump info for the stored timestamp
        When the user runs "gpcrondump -a -x schematestdb -h --incremental" 
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that there is a "heap" table "gpcrondump_history" in "schematestdb"
        And verify that the table "gpcrondump_history" in "schematestdb" has dump info for the stored timestamp

     @backupfire
     Scenario: Verify gpdbrestore -s option with full backup
         Given the database is running
         And the database "testdb1" does not exist
         And the database "testdb2" does not exist
         And database "testdb1" exists
         And database "testdb2" exists
         And there is a "ao" table "ao_table1" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table1" with compression "None" in "testdb1" with data
         And there is schema "pepper2" exists in "testdb2"
         And there is a "ao" table "ao_table2" with compression "None" in "testdb2" with data
         And there is a "co" table "co_table2" with compression "None" in "testdb2" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb1"
         And gpcrondump should return a return code of 0
         And all the data from "testdb1" is saved for verification
         And the user runs "gpcrondump -a -x testdb2"
         And gpcrondump should return a return code of 0
         And the database "testdb2" does not exist
         And the user runs "gpdbrestore  -e -s testdb1 -a"
         And gpdbrestore should return a return code of 0
         Then verify that the data of "2" tables in "testdb1" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb1"
         And verify that database "testdb2" does not exist

     @backupfire
     Scenario: Verify gpdbrestore -s option with incremental backup 
         Given the database is running
         And the database "testdb1" does not exist
         And the database "testdb2" does not exist
         And database "testdb1" exists
         And database "testdb2" exists
         And there is a "ao" table "ao_table1" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table1" with compression "None" in "testdb1" with data
         And there is a "ao" table "ao_table2" with compression "None" in "testdb2" with data
         And there is a "co" table "co_table2" with compression "None" in "testdb2" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb1"
         And gpcrondump should return a return code of 0
         And the user runs "gpcrondump -a -x testdb2"
         And gpcrondump should return a return code of 0
         And table "public.ao_table1" is assumed to be in dirty state in "testdb1"
         And the user runs "gpcrondump -a -x testdb1 --incremental"
         And gpcrondump should return a return code of 0
         And the user runs "gpcrondump -a -x testdb2 --incremental"
         And gpcrondump should return a return code of 0
         And all the data from "testdb1" is saved for verification
         And the database "testdb2" does not exist
         And the user runs "gpdbrestore -e -s testdb1 -a" 
         Then gpdbrestore should return a return code of 0
         And verify that the data of "2" tables in "testdb1" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb1"
         And verify that database "testdb2" does not exist

    @backupfire
    Scenario: gpdbrestore -u option with full backup timestamp
         Given the database is running
         And the database "testdb" does not exist
         And database "testdb" exists
         And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
         And there is a "co" table "co_table" with compression "None" in "testdb" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb -u /tmp"
         And gpcrondump should return a return code of 0
         And the timestamp from gpcrondump is stored
         And all the data from "testdb" is saved for verification
         And verify that db_dumps directory does not exist in master or segments
         And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --verbose" 
         And gpdbrestore should return a return code of 0
         Then verify that the data of "2" tables in "testdb" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb"
        
    @backupsmoke
    Scenario: gpdbrestore -u option with incremental backup timestamp
         Given the database is running
         And the database "testdb" does not exist
         And database "testdb" exists
         And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
         And there is a "co" table "co_table" with compression "None" in "testdb" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb -u /tmp"
         And gpcrondump should return a return code of 0
         And table "public.ao_table" is assumed to be in dirty state in "testdb"
         When the user runs "gpcrondump -a -x testdb -u /tmp --incremental"
         And gpcrondump should return a return code of 0
         And the timestamp from gpcrondump is stored
         And all the data from "testdb" is saved for verification
         And there are no backup files
         And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --verbose" 
         And gpdbrestore should return a return code of 0
         Then verify that the data of "2" tables in "testdb" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb"
        
    Scenario: gpcrondump -x with multiple databases
         Given the database is running
         And the database "testdb1" does not exist
         And the database "testdb2" does not exist
         And database "testdb1" exists
         And database "testdb2" exists
         And there is a "ao" table "ao_table1" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table1" with compression "None" in "testdb1" with data
         And there is a "ao" table "ao_table2" with compression "None" in "testdb2" with data
         And there is a "co" table "co_table2" with compression "None" in "testdb2" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb1,testdb2"
         And the timestamp for database dumps "testdb1, testdb2" are stored
         And gpcrondump should return a return code of 0
         And all the data from "testdb1" is saved for verification
         And all the data from "testdb2" is saved for verification
         And the user runs "gpdbrestore -e -s testdb1 -a" 
         And gpdbrestore should return a return code of 0
         And the user runs "gpdbrestore -e -s testdb2 -a" 
         And gpdbrestore should return a return code of 0
         Then verify that the data of "2" tables in "testdb1" is validated after restore 
         And verify that the data of "2" tables in "testdb2" is validated after restore 
         And the dump timestamp for "testdb1, testdb2" are different
         And verify that the tuple count of all appendonly tables are consistent in "testdb1"
         And verify that the tuple count of all appendonly tables are consistent in "testdb2"

    Scenario: gpdbrestore with -T and --table-file should fail
         Given the database is running
         And the database "testdb1" does not exist
         And database "testdb1" exists
         And there is a "ao" table "ao_table" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table" with compression "None" in "testdb1" with data
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb1"
         And gpcrondump should return a return code of 0
         And the timestamp from gpcrondump is stored 
         And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table --table-file foo"
         Then gpdbrestore should return a return code of 2
         And gpdbrestore should print Cannot specify -T and --table-file together to stdout 

    Scenario: gpdbrestore with --table-file option
         Given the database is running
         And the database "testdb1" does not exist
         And database "testdb1" exists
         And there is a "ao" table "ao_table" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table" with compression "None" in "testdb1" with data
         And there are no backup files
         And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
         When the user runs "gpcrondump -a -x testdb1"
         And gpcrondump should return a return code of 0
         And the timestamp from gpcrondump is stored 
         And all the data from "testdb1" is saved for verification
         And the user runs gpdbrestore with the stored timestamp and options "--table-file /tmp/table_file_foo"
         Then gpdbrestore should return a return code of 0
         And verify that the data of "2" tables in "testdb1" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb1"
         And verify that the restored table "public.ao_table" in database "testdb1" is analyzed
         And verify that the restored table "public.co_table" in database "testdb1" is analyzed

    @backupsmoke
    Scenario: Restore Scenarios
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is schema "pepper" exists in "schematestdb"
        And there is a "heap" table "pepper.heap_table" with compression "None" in "schematestdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "schematestdb" with data
        And there is a "heap" partition table "pepper.heap_part_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "pepper.ao_table2" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" partition table "pepper.co_part_table" with compression "quicklz" in "schematestdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "pepper.ao_table2" is assumed to be in dirty state in "schematestdb"
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "pepper"
        And partition "2" of partition table "co_part_table" is assumed to be in dirty state in "schematestdb" in schema "public"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the numbers "1" to "100" are inserted into "pepper.ao_table" tables in "schematestdb"
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "34" tables in "schematestdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupfire
    Scenario: gpcrondump should not track external tables
         Given the database is running
         And the database "testdb1" does not exist
         And database "testdb1" exists
         And there is a "ao" table "ao_table" with compression "None" in "testdb1" with data
         And there is a "co" table "co_table" with compression "None" in "testdb1" with data
         And there is an external table "ext_tab" in "testdb1" with data for file "/tmp/ext_tab"
         And there are no backup files
         When the user runs "gpcrondump -a -x testdb1"
         And gpcrondump should return a return code of 0
         When the user runs "gpcrondump -a -x testdb1 --incremental"
         And gpcrondump should return a return code of 0
         And the timestamp from gpcrondump is stored 
         And all the data from "testdb1" is saved for verification
         And the user runs gpdbrestore with the stored timestamp
         Then gpdbrestore should return a return code of 0
         And verify that the data of "3" tables in "testdb1" is validated after restore
         And verify that the tuple count of all appendonly tables are consistent in "testdb1"
         And verify that there is no "public.ext_tab" in the "dirty_list" file in " "
         And verify that there is no "public.ext_tab" in the "table_list" file in " "
         Then the file "/tmp/ext_tab" is removed from the system

    @backupfire
    Scenario: Full backup -T test 1 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "ao_part_table" in "fullbkdb"
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    @backupfire
    Scenario: Full backup -T test 2 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "ao_part_table" in "fullbkdb"
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    @backupfire
    @truncate
    Scenario: Full backup and restore with -T and --truncate
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        And the user runs "gpdbrestore -T public.ao_index_table -a --truncate" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data
        And verify that the restored table "public.ao_index_table" in database "fullbkdb" is analyzed
        And the user runs "gpdbrestore -T public.ao_part_table_1_prt_p1_2_prt_1 -a --truncate" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data

    @backupfire
    Scenario: Full backup -T test 3
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user truncates "public.ao_index_table" tables in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data
        And verify that the restored table "public.ao_index_table" in database "fullbkdb" is analyzed

    Scenario: Full backup -T test 4 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When table "public.ao_index_table" is deleted in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data
        And verify that the restored table "public.ao_index_table" in database "fullbkdb" is analyzed

    Scenario: Full backup -T test 5
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_part_table_1_prt_p2_2_prt_3 -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table_1_prt_p2_2_prt_3" in "fullbkdb" with data
        And verify that the restored table "public.ao_part_table_1_prt_p2_2_prt_3" in database "fullbkdb" is analyzed

    Scenario: Full backup -T test 6
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "fullbkdb"
        And the user runs "gpdbrestore -T ao_part_table_1_prt_p2_2_prt_3 -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        Then gpdbrestore should print No schema name supplied to stdout

    Scenario: Full backup with gpdbrestore -T for DB with FUNCTION having DROP SQL
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_function_with_drop_table.sql fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        When table "public.ao_index_table" is deleted in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data
        And verify that there is a "heap" table "public.heap_table" in "fullbkdb" with data

    @backupfire
    Scenario: Incremental restore with table filter
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "schematestdb"
        And table "co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.co_table"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "schematestdb" have been restored 

    Scenario: Incremental restore with invalid table filter
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "schematestdb"
        And table "co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.invalid"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Invalid tables for -T option: The following tables were not found in plan file to stdout

    @backupfire
    Scenario: Incremental backup -T test 1 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "ao_part_table" in "fullbkdb" 
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    Scenario: Incremental backup -T test 2 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "ao_part_table" in "fullbkdb" 
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    Scenario: Incremental backup -T test 3
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user truncates "public.ao_index_table" tables in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    Scenario: Incremental backup -T test 4 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When table "public.ao_index_table" is deleted in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    Scenario: Incremental backup -T test 5
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "public.ao_part_table_1_prt_p2_2_prt_3" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_part_table_1_prt_p2_2_prt_3 -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_part_table_1_prt_p2_2_prt_3" in "fullbkdb" with data

    Scenario: Incremental backup -T test 6
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "public.ao_part_table_1_prt_p2_2_prt_3" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        When the user truncates "public.ao_part_table_1_prt_p2_2_prt_3" tables in "fullbkdb"
        And the user runs "gpdbrestore -T ao_part_table_1_prt_p2_2_prt_3 -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        Then gpdbrestore should print No schema name supplied to stdout

    Scenario: Incremental backup -T test 7
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table_1_prt_p2_2_prt_3" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        And the user runs "gpdbrestore -T public.ao_part_table -a -e" with the stored timestamp
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is no table "ao_index_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data

    @backupfire
    Scenario: gpdbrestore -L with -u option
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in "/tmp" dir contains "Backup Type: Full"
        When the user runs gpdbrestore with the stored timestamp and options "-L -u /tmp" 
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Table public.ao_part_table to stdout 
        And gpdbrestore should print Table public.heap_table to stdout 

    @backupfire
    Scenario: gpdbrestore -b with -u option for Full timestamp
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "schematestdb" with data
        When the user runs "gpcrondump -a -x schematestdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore on dump date directory with options "-u /tmp"
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "schematestdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    @backupfire
    Scenario: gpdbrestore should not support -u and ddboost parameters together
        Given the database is running
        When the user runs "gpdbrestore -u /tmp --ddboost -s testdb1" 
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print -u cannot be used with DDBoost parameters to stdout

    @backupfire
    Scenario: gpdbrestore with -s and -u options for full backup
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "schematestdb" with data
        When the user runs "gpcrondump -a -x schematestdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And all the data from "schematestdb" is saved for verification
        And the user runs "gpdbrestore -e -s schematestdb -u /tmp -a"
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "schematestdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"
        
    @backupfire
    Scenario: gpdbrestore with -s and -u options for incremental backup
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "schematestdb" with data
        When the user runs "gpcrondump -a -x schematestdb -u /tmp"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "schematestdb"
        When the user runs "gpcrondump -a -x schematestdb -u /tmp --incremental"
        Then gpcrondump should return a return code of 0
        And all the data from "schematestdb" is saved for verification
        And the user runs "gpdbrestore -e -s schematestdb -u /tmp -a"
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "schematestdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "schematestdb"

    Scenario: gpdbrestore -b option should display the timestamps in sorted order
        Given the database is running
        And there are no backup files
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "schematestdb" with data
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb --incremental -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb --incremental -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb --incremental -a"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "schematestdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp and options "-b"
        And the timestamps should be printed in sorted order

    Scenario: gpdbrestore -R option should display the timestamps in sorted order
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "schematestdb" with data
        When the user runs "gpcrondump -a -x schematestdb -u /tmp"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb -u /tmp -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb -u /tmp -a"
        And gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "schematestdb" 
        And the user runs "gpcrondump -x schematestdb -u /tmp -a"
        And the subdir from gpcrondump is stored 
        And gpcrondump should return a return code of 0
        And all the data from "schematestdb" is saved for verification
        Then the user runs gpdbrestore with "-R" option in path "/tmp"
        And the timestamps should be printed in sorted order

    Scenario Outline: Dirty File Scale Test
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
		And there are "<tablecount>" "heap" tables "public.heap_table" with data in "testdb"
		And there are "10" "ao" tables "public.ao_table" with data in "testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_2" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_3" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_4" is assumed to be in dirty state in "testdb"
        And all the data from "testdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the subdir from gpcrondump is stored 
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "testdb"
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_5, public.ao_table_6, public.ao_table_7" in "testdb" has no rows
        And verify that tables "public.ao_table_8, public.ao_table_9, public.ao_table_10" in "testdb" has no rows
        And verify that the data of the dirty tables under " " in "testdb" is validated after restore
        Examples:
        | tablecount | 
        | 95         | 
        | 96         | 
        | 97         | 
        | 195        | 
        | 196        | 
        | 197        | 

    Scenario Outline: Dirty File Scale Test for partitions
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
		And there are "<tablecount>" "heap" tables "public.heap_table" with data in "testdb"
        And there is a "ao" partition table "ao_table" with compression "None" in "testdb" with data
        Then data for partition table "ao_table" with partition level "1" is distributed across all segments on "testdb"
        And verify that partitioned tables "ao_table" in "testdb" have 6 partitions
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_1_prt_p2_2_prt_1" is assumed to be in dirty state in "testdb"
        And table "public.ao_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "testdb"
        And all the data from "testdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the subdir from gpcrondump is stored 
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "testdb"
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_1_prt_p1_2_prt_3, public.ao_table_1_prt_p2_2_prt_3" in "testdb" has no rows
        And verify that the data of the dirty tables under " " in "testdb" is validated after restore
        Examples:
        | tablecount | 
        | 95         | 
        | 96         | 
        | 97         | 
        | 195        | 
        | 196        | 
        | 197        | 

    Scenario: Config files have the same timestamp as the backup set
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And verify that the config files are backed up with the stored timestamp

    Scenario Outline: Incremental Backup With column-inserts, inserts and oids options 
        Given the database is running
        When the user runs "gpcrondump -a --incremental -x bkdb <options>"
        Then gpcrondump should print --inserts, --column-inserts, --oids cannot be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2
        Examples:
        | options          |
        | --inserts        |
        | --oids           |
        | --column-inserts |

    Scenario: Test gpcrondump and gpdbrestore verbose option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb --verbose"
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "fullbkdb"
        And the user runs "gpcrondump -a -x fullbkdb --incremental --verbose"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--verbose"
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "fullbkdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "fullbkdb"
    
    @backupfire
    Scenario: Incremental table filter gpdbrestore
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
		And there are "2" "heap" tables "public.heap_table" with data in "testdb"
        And there is a "ao" partition table "ao_part_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table1" with compression "None" in "testdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "testdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "testdb" have 6 partitions
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "testdb"
        And table "public.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "testdb"
        And table "public.ao_part_table_1_prt_p2_2_prt_1" is assumed to be in dirty state in "testdb"
        And table "public.ao_part_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "testdb"
        And all the data from "testdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        When the user runs "gpdbrestore -e -T public.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_3" in "testdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_3" in "testdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_2" in "testdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_2" in "testdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_1" in "testdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_1" in "testdb"
        And verify that there is no table "public.heap_table_1" in "testdb"
        And verify that there is no table "public.heap_table_2" in "testdb"

    @backupfire
    Scenario: Incremental table filter gpdbrestore with different schema for same tablenames
        Given the database is running
        And there are no backup files
        And database "schematestdb" is created if not exists
        And there is schema "pepper" exists in "schematestdb"
		And there are "2" "heap" tables "public.heap_table" with data in "schematestdb"
        And there is a "ao" partition table "ao_part_table" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "ao_part_table1" with compression "None" in "schematestdb" with data
        And there is a "ao" partition table "pepper.ao_part_table1" with compression "None" in "schematestdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "schematestdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "schematestdb"
        Then data for partition table "pepper.ao_part_table1" with partition level "1" is distributed across all segments on "schematestdb"
        When the user runs "gpcrondump -a -x schematestdb"
        Then gpcrondump should return a return code of 0
        When table "public.ao_part_table1_1_prt_p1_2_prt_1" is assumed to be in dirty state in "schematestdb"
        And table "public.ao_part_table1_1_prt_p1_2_prt_2" is assumed to be in dirty state in "schematestdb"
        And table "public.ao_part_table1_1_prt_p2_2_prt_1" is assumed to be in dirty state in "schematestdb"
        And table "public.ao_part_table1_1_prt_p2_2_prt_2" is assumed to be in dirty state in "schematestdb"
        And all the data from "schematestdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        When the user runs "gpdbrestore -e -T public.ao_part_table1 -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.ao_part_table_1_prt_p1_2_prt_3" in "schematestdb"
        And verify that there is no table "public.ao_part_table_1_prt_p2_2_prt_3" in "schematestdb"
        And verify that there is no table "public.ao_part_table_1_prt_p1_2_prt_2" in "schematestdb"
        And verify that there is no table "public.ao_part_table_1_prt_p2_2_prt_2" in "schematestdb"
        And verify that there is no table "public.ao_part_table_1_prt_p1_2_prt_1" in "schematestdb"
        And verify that there is no table "public.ao_part_table_1_prt_p2_2_prt_1" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p1_2_prt_3" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p2_2_prt_3" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p1_2_prt_2" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p2_2_prt_2" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p1_2_prt_1" in "schematestdb"
        And verify that there is no table "pepper.ao_part_table1_1_prt_p2_2_prt_1" in "schematestdb"
        And verify that there is no table "public.heap_table_1" in "schematestdb"
        And verify that there is no table "public.heap_table_2" in "schematestdb"

    @backupfire
    Scenario: Incremental table filter gpdbrestore with noplan option
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
		And there are "2" "heap" tables "public.heap_table" with data in "testdb"
        And there is a "ao" partition table "ao_part_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table1" with compression "None" in "testdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "testdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "testdb" have 6 partitions
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When all the data from "testdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x testdb"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored 
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "testdb"
        Then gp_restore should return a return code of 0
        And the user runs "gpdbrestore -T public.ao_part_table,public.heap_table_1 -a --noplan" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_3, public.ao_part_table_1_prt_p2_2_prt_3" in "testdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_2, public.ao_part_table_1_prt_p2_2_prt_2" in "testdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_1, public.ao_part_table_1_prt_p2_2_prt_1" in "testdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_3, public.ao_part_table1_1_prt_p2_2_prt_3" in "testdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_2, public.ao_part_table1_1_prt_p2_2_prt_2" in "testdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_1, public.ao_part_table1_1_prt_p2_2_prt_1" in "testdb" has no rows
        And verify that there is a "heap" table "public.heap_table_1" in "testdb" with data
   
    @backupsmoke
    Scenario: gpdbrestore list_backup option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "fullbkdb" with data
        And there is a "co" table "co_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.co_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a --incremental -x fullbkdb"
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
        Given the database is running
        When the user runs "gpdbrestore -a -e --list-backup -t 20130221093700"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Cannot specify --list-backup and -e together to stdout


    @backupfire
    Scenario: gpdbrestore list_backup option with -T table filter
        Given the database is running
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "bkdb" with data
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
        Given the database is running
        And the database "bkdb" does not exist
        And database "bkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options " "
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print --list-backup is not supported for restore with full timestamps to stdout

     Scenario: Full Backup with option -t and non-existant table
         Given the database is running
         And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
         And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
         And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
         When the user runs "gpcrondump -a -x fullbkdb -t cool.dude -t public.heap_table"
         Then gpcrondump should return a return code of 2
         And gpcrondump should print does not exist in to stdout
 
     Scenario: Full Backup with option -T and non-existant table
         Given the database is running
         And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
         And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
         And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
         When the user runs "gpcrondump -a -x fullbkdb -T public.heap_table -T cool.dude"
         Then gpcrondump should return a return code of 0
         And gpcrondump should print does not exist in to stdout
         And the timestamp from gpcrondump is stored 
         And the user runs gpdbrestore with the stored timestamp
         And verify that the "report" file in " " dir contains "Backup Type: Full"
         And gpdbrestore should return a return code of 0
         And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
         And verify that there is no table "heap_table" in "fullbkdb"

    @backupfire
    Scenario: Funny character table names
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_table" with funny characters in "testdb"
        And there is a "co" table "public.co_table" with funny characters in "testdb"
        And there are no backup files
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Tablename has an invalid character ".n", ":", "," : to stdout

    Scenario: Negative test gpdbrestore -G with incremental timestamp
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb "
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x fullbkdb --incremental -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "global" file should be created under " "
        And table "public.ao_part_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "-G" 
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to locate global file to stdout 

    @backupfire
    Scenario: gp_restore with no ao stats
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table, co_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gp_dump --gp-d=db_dumps --gp-s=p --gp-c fullbkdb"
        Then gp_dump should return a return code of 0
        And the timestamp from gp_dump is stored and subdir is " " 
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the user runs gp_restore with the the stored timestamp and subdir in "fullbkdb" and bypasses ao stats
        And gp_restore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And verify that there is a "co" table "co_part_table" in "fullbkdb" with data
        And verify that there are no aoco stats in "fullbkdb" for table "ao_part_table_1_prt_p1_2_prt_1, ao_part_table_1_prt_p1_2_prt_2, ao_part_table_1_prt_p1_2_prt_3"
        And verify that there are no aoco stats in "fullbkdb" for table "ao_part_table_1_prt_p2_2_prt_1, ao_part_table_1_prt_p2_2_prt_2, ao_part_table_1_prt_p2_2_prt_3"
        And verify that there are no aoco stats in "fullbkdb" for table "co_part_table_1_prt_p1_2_prt_1, co_part_table_1_prt_p1_2_prt_2, co_part_table_1_prt_p1_2_prt_3"
        And verify that there are no aoco stats in "fullbkdb" for table "co_part_table_1_prt_p2_2_prt_1, co_part_table_1_prt_p2_2_prt_2, co_part_table_1_prt_p2_2_prt_3"

    Scenario: Negative test for report file missing gpcrondump
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "report" file is removed under " "
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print is not a valid increment to stdout

    Scenario: gpdbrestore with missing report file for middle increment
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And "report" file is removed under " "
        And the timestamp from gpcrondump is stored
        When all the data from "fullbkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "19" tables in "fullbkdb" is validated after restore 

    Scenario: gpdbrestore with missing report file on full restore timestamp 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "report" file is removed under " "
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Report file does not exist for the given restore timestamp to stdout

    Scenario: gpdbrestore with missing report file on incremental restore timestamp 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "report" file is removed under " "
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Report file does not exist for the given restore timestamp to stdout
        
    Scenario: Negative tests for table filter options for gpcrondump
        Given the database is running
        When the user runs "gpcrondump -a -t public.table --table-file include_file -x schematestdb"
        Then gpcrondump should print -t can not be selected with --table-file option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a -t public.table --exclude-table-file exclude_file -x schematestdb"
        Then gpcrondump should print -t can not be selected with --exclude-table-file option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a -T public.table --exclude-table-file exclude_file -x schematestdb"
        Then gpcrondump should print -T can not be selected with --exclude-table-file option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a -T public.table --table-file include_file -x schematestdb"
        Then gpcrondump should print -T can not be selected with --table-file option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a -t public.table1 -T public.table2 -x schematestdb"
        Then gpcrondump should print -t can not be selected with -T option to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a --table-file include_file --exclude-table-file exclude_file -x schematestdb"
        Then gpcrondump should print --table-file can not be selected with --exclude-table-file option to stdout
        And gpcrondump should return a return code of 2

    Scenario: User specified timestamp key for dump
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        Then gpcrondump should return a return code of 0
        And verify that "full" dump files have stored timestamp in their filename
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "testdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "testdb"
        
    Scenario: User specified timestamp key for dump
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 201301010101"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Invalid timestamp key to stdout
    
    Scenario: User specified timestamp key for dump
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        And verify that "full" dump files have stored timestamp in their filename
        When the user runs "gpcrondump -a --incremental -x testdb -K 20130201010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that "incremental" dump files have stored timestamp in their filename
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "11" tables in "testdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "testdb"
   
    @backupsmoke
    Scenario: --list-backup-files option for dump
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --verbose"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And there are no dump files created under " "

    Scenario: --list-backup-files option for dump with -G
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files -G --verbose"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options "-G" is validated after dump operation
        And the "regular_files" file under " " with options "-G" is validated after dump operation
        And there are no dump files created under " " 

    Scenario: --list-backup-files option for dump with -g
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files -g --verbose"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options "-g" is validated after dump operation
        And the "regular_files" file under " " with options "-g" is validated after dump operation
        And there are no dump files created under " " 

    Scenario: --list-backup-files option for dump without compression
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files -z --verbose"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And there are no dump files created under " " 

    @backupsmoke
    Scenario: --list-backup-files option for dump with --incremental
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20120101010101"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored 
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --incremental --verbose"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options "--incremental" is validated after dump operation
        And the "regular_files" file under " " with options "--incremental" is validated after dump operation
        And there are no dump files created under " " 

    @backupsmoke
    @filter
    Scenario: --list-backup-files option for dump with --prefix and filtering
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And the prefix "foo" is stored
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --verbose --prefix=foo -t ao_index_table"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options "-t,--prefix=foo" is validated after dump operation
        And the "regular_files" file under " " with options "-t,--prefix=foo" is validated after dump operation
        And there are no dump files created under " "

    Scenario: Full Backup and Restore with named pipes
        Given the database is running
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb --list-backup-files -K 20130101010101"
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
        When the user runs "gpcrondump -a -x fullbkdb -K 20130101010101"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And the named pipe script for the "restore" is run for the files under " "
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data 
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    Scenario: --list-backup-files option for dump -u
        Given the database is running
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --verbose -u /tmp"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under "/tmp"
        Then "regular_files" file should be created under "/tmp"
        And the "pipes" file under "/tmp" with options " " is validated after dump operation
        And the "regular_files" file under "/tmp" with options " " is validated after dump operation
        And there are no dump files created under "/tmp" 

    Scenario: Full Backup and Restore with named pipes with -G option
        Given the database is running
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb --list-backup-files -K 20130101010101 -G"
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
        When the user runs "gpcrondump -a -x fullbkdb -K 20130101010101 -G"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And the named pipe script for the "restore" is run for the files under " "
        And the user runs gpdbrestore with the stored timestamp and options "-G"
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data 
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    Scenario: Full and Incremental Backup with -g option using named pipes
        Given the database is running
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb --list-backup-files -K 20130101010101 -g"
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
        When the user runs "gpcrondump -a -x fullbkdb -K 20130101010101 -g"
        Then gpcrondump should return a return code of 0 
        When the user runs "gpcrondump -a -x fullbkdb -K 20130101020101 -g --incremental"
        Then gpcrondump should return a return code of 0 

    Scenario: Incremental Backup with no heap tables, backup at the end
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "testdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x testdb"
        And the full backup timestamp from gpcrondump is stored        
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts1"
        When the user runs "gpcrondump -a -x testdb --incremental"
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts2"
        And gpcrondump should return a return code of 0
        And all the data from "testdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "testdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "testdb"
        And the plan file is validated against "/data/plan11"

    Scenario: Incremental Backup with no heap tables, backup in the middle
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_1" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_table_2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_table_3" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "testdb" with data
        And there are no backup files
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x testdb"
        And the full backup timestamp from gpcrondump is stored        
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts1"
        And table "public.ao_table_1" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts2"
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x testdb --incremental"
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts3"
        And table "public.ao_table_3" is assumed to be in dirty state in "testdb"
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x testdb --incremental"
        And the timestamp from gpcrondump is stored in a list
        And the timestamp is labeled "ts4"
        And gpcrondump should return a return code of 0
        And all the data from "testdb" is saved for verification
        Then the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "12" tables in "testdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "testdb"
        And the plan file is validated against "/data/plan12"

    Scenario: Incremental Backup and Restore with named pipes
        Given the database is running
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        And there is a list to store the incremental backup timestamps
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb --list-backup-files -K 20130101010101"
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
        When the user runs "gpcrondump -a -x fullbkdb -K 20130101010101"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a -x fullbkdb --list-backup-files -K 20140101010101 --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp key "20140101010101" for gpcrondump is stored 
        And "pipes" file should be created under " "
        And "regular_files" file should be created under " "
        And the "pipes" file under " " with options " " is validated after dump operation
        And the "regular_files" file under " " with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20140101010101" under " "
        And the named pipes are validated against the timestamp "20140101010101" under " "
        And the named pipe script for the "dump" is run for the files under " "
        And table "public.ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb -K 20140101010101 --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        And the named pipe script for the "restore" is run for the files under " "
        And all the data from "fullbkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "fullbkdb" is validated after restore 
        And close all opened pipes

    Scenario: Incremental Backup and Restore with named pipes with -u
        Given the database is running
        And the backup files in "/tmp" are deleted
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --verbose -u /tmp"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under "/tmp"
        Then "regular_files" file should be created under "/tmp"
        And the "pipes" file under "/tmp" with options " " is validated after dump operation
        And the "regular_files" file under "/tmp" with options " " is validated after dump operation
        And there are no dump files created under "/tmp" 
        And the named pipes are created for the timestamp "20130101010101" under "/tmp"
        And the named pipes are validated against the timestamp "20130101010101" under "/tmp"
        And the named pipe script for the "dump" is run for the files under "/tmp"
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 -u /tmp"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        When the user runs "gpcrondump -a -x testdb --list-backup-files -K 20140101010101 --incremental -u /tmp"
        Then gpcrondump should return a return code of 0 
        And the timestamp key "20140101010101" for gpcrondump is stored 
        And "pipes" file should be created under "/tmp"
        And "regular_files" file should be created under "/tmp"
        And the "pipes" file under "/tmp" with options " " is validated after dump operation
        And the "regular_files" file under "/tmp" with options " " is validated after dump operation
        And the named pipes are created for the timestamp "20140101010101" under "/tmp"
        And the named pipes are validated against the timestamp "20140101010101" under "/tmp"
        And the named pipe script for the "dump" is run for the files under "/tmp"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb -K 20140101010101 --incremental -u /tmp"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        And the named pipe script for the "restore" is run for the files under "/tmp"
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --verbose" 
        And gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "testdb" is validated after restore 
        And close all opened pipes

    @backupsmoke
    Scenario: Full Backup and Restore with --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    Scenario: Full Backup and Restore with --prefix option for multiple databases
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb1" does not exist
        And database "testdb1" exists
        And the database "testdb2" does not exist
        And database "testdb2" exists
        And there is a "heap" table "heap_table1" with compression "None" in "testdb1" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb1" with data
        And there is a backupfile of tables "heap_table1, ao_part_table" in "testdb1" exists for validation
        And there is a "heap" table "heap_table2" with compression "None" in "testdb2" with data
        And there is a backupfile of tables "heap_table2" in "testdb2" exists for validation
        When the user runs "gpcrondump -a -x testdb1,testdb2 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table1" in "testdb1" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb1" with data
        And verify that there is a "heap" table "heap_table2" in "testdb2" with data

    @backupsmoke
    Scenario: Incremental Backup and Restore with --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And there should be dump files under " " with prefix "foo"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental --prefix=foo"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And there should be dump files under " " with prefix "foo"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental --prefix=foo"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And there should be dump files under " " with prefix "foo"
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore with -g and --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo -g"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And config files should be backed up on all segments
        And there should be dump files under " " with prefix "foo"
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    Scenario: Full Backup and Restore with -G and --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo -G"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And "global" file should be created under " "
        And there should be dump files under " " with prefix "foo"
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    Scenario: Full Backup and Restore with -u and --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --prefix=foo" 
        And gpdbrestore should return a return code of 0
        And there should be dump files under "/tmp" with prefix "foo"
        And verify that there is a "heap" table "heap_table" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    Scenario: Full Backup with --list-backup-files and --prefix options
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --list-backup-files --prefix=foo"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print Added the list of pipe names to the file to stdout
        And gpcrondump should print Added the list of file names to the file to stdout
        And gpcrondump should print Successfully listed the names of backup files and pipes to stdout
        And the timestamp key "20130101010101" for gpcrondump is stored 
        Then "pipes" file should be created under " "
        Then "regular_files" file should be created under " "
        And the "pipes" file under " " with options "--prefix=foo" is validated after dump operation
        And the "regular_files" file under " " with options "--prefix=foo" is validated after dump operation
        And there are no dump files created under " "

    Scenario: Full and incremental backups with mixed prefix
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And there should be dump files under " " with prefix "foo"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 2 
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Incremental with multiple full backups having different prefix
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp 
        And gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "testdb" is validated after restore 

    Scenario: Incremental backup with prefix based off full backup without prefix
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0 
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental --prefix=foo"
        Then gpcrondump should return a return code of 2 
        And gpcrondump should print No full backup found for incremental to stdout

    Scenario: Restore database without prefix for a dump with prefix
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2 
        And gpdbrestore should print Dump file .* does not exist on Master to stdout

    Scenario: Multiple full and incrementals with and without prefix
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental --prefix=foo"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        Then gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "testdb" is validated after restore 

    Scenario: Incremental Backup and Restore with --prefix and -u options
        Given the database is running
        And the prefix "foo" is stored
        And the backup files in "/tmp" are deleted
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -u /tmp"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental --prefix=foo -u /tmp"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo -u /tmp" 
        And gpdbrestore should return a return code of 0 
        And verify that the data of "11" tables in "testdb" is validated after restore 

    @filter
    @backupsmoke
    Scenario: Incremental Backup and Restore with -t filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_index_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And gpcrondump should print Filtering tables using: to stdout
        And gpcrondump should print Prefix                        = foo to stdout
        And gpcrondump should print Full dump timestamp           = [0-9]{14} to stdout
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering testdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table1 to stdout
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    Scenario: Incremental Backup and Restore with -T filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -T public.ao_part_table -T public.heap_table2"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering testdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table1 to stdout
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    Scenario: Incremental Backup and Restore with --table-file filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a table-file "/tmp/table_file_1" with tables "public.ao_index_table, public.heap_table1"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --table-file /tmp/table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the temp files "table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering testdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table1 to stdout
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    Scenario: Incremental Backup and Restore with --exclude-table-file filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a table-file "/tmp/exclude_table_file_1" with tables "public.ao_part_table, public.heap_table2"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --exclude-table-file /tmp/exclude_table_file_1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the temp files "exclude_table_file_1" are removed from the system
        And the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering testdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table1 to stdout
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    Scenario: Multiple Incremental Backups and Restore with -t filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_index_table -t public.heap_table1 -t public.ao_part_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_part_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored in a list
        And table "public.heap_table1" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And gpcrondump should print Filtering testdb for the following tables: to stdout
        And gpcrondump should print public.ao_index_table to stdout
        And gpcrondump should print public.heap_table1 to stdout
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that partitioned tables "ao_part_table" in "testdb" have 6 partitions
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    Scenario: Incremental Backup and Restore with Multiple Schemas and -t filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And there is a "heap" table "pepper.heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "pepper.heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "pepper.co_table" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "co" table "co_index_table" with compression "None" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -K 20120101010101 --prefix=foo -t public.ao_index_table -t public.heap_table1 -t pepper.ao_table -t pepper.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And verify that the "filter" file in " " dir contains "pepper.ao_table"
        And verify that the "filter" file in " " dir contains "pepper.heap_table1"
        And table "public.heap_table1" is assumed to be in dirty state in "testdb"
        And table "pepper.heap_table1" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp key "20130101010101" for gpcrondump is stored
        And all the data from "testdb" is saved for verification
        When the user truncates "public.ao_index_table, public.heap_table1" tables in "testdb"
        And the user truncates "pepper.ao_table, pepper.heap_table1" tables in "testdb"
        And the user runs "gpdbrestore -a -t 20130101010101 --prefix=foo"
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table2" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is a "co" table "public.co_index_table" in "testdb" with data
        And verify that there is a "heap" table "pepper.heap_table1" in "testdb" with data
        And verify that there is a "heap" table "pepper.heap_table2" in "testdb" with data
        And verify that there is a "ao" table "pepper.ao_table" in "testdb" with data
        And verify that there is a "co" table "pepper.co_table" in "testdb" with data

    @filter
    Scenario: Multiple Full and Incremental Backups with -t filters for different prefixes in parallel
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "co_table" with compression "None" in "testdb" with data
        And the prefix "foo1" is stored
        When the user runs "gpcrondump -a -x testdb -K 20120101010101 --prefix=foo1 -t public.ao_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        Given the prefix "foo2" is stored
        When the user runs "gpcrondump -a -x testdb -K 20130101010101 --prefix=foo2 -t public.co_table -t public.heap_table2"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.co_table"
        And verify that the "filter" file in " " dir contains "public.heap_table2"
        And table "public.ao_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb -K 20120201010101 --prefix=foo1 --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And table "public.co_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb -K 20130201010101 --prefix=foo2 --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        When all the data from "testdb" is saved for verification
        And the user runs "gpdbrestore -a -e -t 20120201010101 --prefix=foo1"
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that there is no table "public.co_table" in "testdb"
        When the user runs "gpdbrestore -a -e -t 20130201010101 --prefix=foo2"
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table2" in "testdb" with data
        And verify that there is a "co" table "public.co_table" in "testdb" with data
        And verify that there is no table "public.heap_table1" in "testdb"
        And verify that there is no table "public.ao_table" in "testdb"

    @filter
    Scenario: Incremental Backup with table filter on Full Backup should update the tracker files  
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_index_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And "public.ao_index_table" is marked as dirty in dirty_list file
        And "public.heap_table1" is marked as dirty in dirty_list file
        And verify that there is no "public.heap_table2" in the "dirty_list" file in " "
        And verify that there is no "public.ao_part_table" in the "dirty_list" file in " "
        And verify that there is no "public.heap_table2" in the "table_list" file in " "
        And verify that there is no "public.ao_part_table" in the "table_list" file in " "
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"

    @filter
    @backupsmoke
    Scenario: Filtered Incremental Backup and Restore with -t and added partition
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_part_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And partition "3" is added to partition table "public.ao_part_table" in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 9 partitions
        And verify that there is partition "3" of "ao" partition table "ao_part_table" in "testdb" in "public"
        And verify that the data of "14" tables in "testdb" is validated after restore 

    @filter
    Scenario: Filtered Incremental Backup and Restore with -T and added partition
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -T public.ao_index_table -T public.heap_table2"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And partition "3" is added to partition table "public.ao_part_table" in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 9 partitions
        And verify that there is partition "3" of "ao" partition table "ao_part_table" in "testdb" in "public"
        And verify that the data of "14" tables in "testdb" is validated after restore 

    @filter
    Scenario: Filtered Incremental Backup and Restore with -t and dropped partition
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_part_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And partition "2" is dropped from partition table "public.ao_part_table" in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 3 partitions
        And verify that the data of "6" tables in "testdb" is validated after restore 

    @filter
    Scenario: Filtered Incremental Backup and Restore with -T and dropped partition
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -T public.ao_index_table -T public.heap_table2"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And partition "2" is dropped from partition table "public.ao_part_table" in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 3 partitions
        And verify that the data of "6" tables in "testdb" is validated after restore 

    @filter
    Scenario: Filtered Incremental Backup and Restore with -t and dropped non-partition and partition table
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table1" with compression "quicklz" in "testdb" with data
        And there is a "ao" partition table "ao_part_table2" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_part_table1 -t public.ao_part_table2 -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table1"
        And verify that the "filter" file in " " dir contains "public.ao_part_table2"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        When table "public.ao_part_table2" is dropped in "testdb"
        And table "public.heap_table1" is dropped in "testdb"
        And the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "public.ao_part_table1" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that there is no table "public.ao_part_table2" in "testdb"
        And verify that there is no table "public.heap_table1" in "testdb"
        And verify that partitioned tables "ao_part_table1" in "testdb" have 6 partitions
        And verify that the data of "9" tables in "testdb" is validated after restore 

    @filter
    Scenario: Filtered Multiple Incremental Backups and Restore with -t and dropped partition between IBs
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -t public.ao_part_table -t public.heap_table1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_part_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And partition "2" is dropped from partition table "public.ao_part_table" in "testdb"
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.ao_index_table" in "testdb"
        And verify that there is no table "public.heap_table2" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 6 partitions
        And verify that the data of "10" tables in "testdb" is validated after restore 
        When the user runs "gpcrondump -x testdb --prefix=foo --incremental  < gppylib/test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpcrondump should return a return code of 0 

    Scenario: Gpcrondump with no PGPORT set
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        And the environment variable "PGPORT" is not set
        When the user runs "gpcrondump -G -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the environment variable "PGPORT" is reset
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    @backupsmoke
    Scenario: Checking for abnormal whitespace
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "fullbkdb" with data
        And there is a file "include_file_with_whitespace" with tables "public.heap_table   ,public.ao_part_table" 
        And there is a backupfile of tables "heap_table,ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb --table-file include_file_with_whitespace"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is no table "co_part_table" in "fullbkdb"

    Scenario: Full Backup and Restore of one table with -C option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is an external table "ext_tab" in "fullbkdb" with data for file "/tmp/ext_tab"
        When the user runs "gpcrondump -a -x fullbkdb -C -K 20140101010101"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And all the data from "fullbkdb" is saved for verification
        And the user runs "gpdbrestore -a -t 20140101010101 -T public.ao_part_table,public.ext_tab"
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data
        And verify that there is a "external" table "public.ext_tab" in "fullbkdb"

    Scenario: Full Backup and Restore of all tables with -C option
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb -C"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "ao_table" in "fullbkdb"
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb"

    Scenario: Backup and Restore of schema table with -C option redirected to empty database
        Given the database is running
        And the database "fullbkdb" does not exist
        And the database "fullbkdb2" does not exist
        And database "fullbkdb" exists
        And database "fullbkdb2" exists
        And there are no backup files
        And there is schema "testschema" exists in "fullbkdb"
        And there is a "heap" table "testschema.schema_heap_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb -C"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -a --redirect fullbkdb2" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "testschema.schema_heap_table" in "fullbkdb2"
    
    Scenario: Incremental Backup with -C option
        Given the database is running
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb -C"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x fullbkdb --incremental -C"
        Then gpcrondump should print -C option can not be selected with incremental backup to stdout
        And gpcrondump should return a return code of 2 

    Scenario: Full Backup and Restore of specified metadata
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" partition table "pepper.heap_part_table" with compression "quicklz" in "testdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a "co" table "public.co_table" with compression "None" in "testdb" with data
        And there is a "co" partition table "public.co_part_table" with compression "quicklz" in "testdb" with data
        And there is a "heap" table "pepper.heap_table_ex" with compression "None" in "testdb" with data
        And there is a "heap" partition table "public.heap_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "ao" table "public.ao_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" partition table "public.ao_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "co" table "pepper.co_table_ex" with compression "None" in "testdb" with data
        And there is a "co" partition table "pepper.co_part_table_ex" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And there is a file "restore_file" with tables "public.heap_table,pepper.ao_table,public.co_table" 
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "pepper.ao_table" in "testdb" with data
        And verify that there is a "co" table "public.co_table" in "testdb" with data
        And verify that there is no table "pepper.heap_table_ex" in "testdb"
        And verify that there is no table "public.ao_table_ex" in "testdb"
        And verify that there is no table "pepper.co_table_ex" in "testdb"
        And verify that there is no table "public.heap_part_table_ex" in "testdb"
        And verify that there is no table "public.ao_part_table_ex" in "testdb"
        And verify that there is no table "pepper.co_part_table_ex" in "testdb"
        And verify that there is no table "pepper.heap_part_table" in "testdb"
        And verify that there is no table "public.ao_part_table" in "testdb"
        And verify that there is no table "public.co_part_table" in "testdb"
        And verify that the data of "3" tables in "testdb" is validated after restore 

    Scenario: Incremental Backup and Restore of specified metadata
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "heap" partition table "heap_part_table" with compression "quicklz" in "testdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "testdb" with data
        And there is a "heap" table "heap_table_ex" with compression "None" in "testdb" with data
        And there is a "heap" partition table "heap_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "ao" table "ao_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table_ex" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table_ex" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And table "public.co_table" is assumed to be in dirty state in "testdb"
        And partition "2" of partition table "ao_part_table" is assumed to be in dirty state in "testdb" in schema "public"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And there is a file "restore_file" with tables "public.heap_table,public.ao_table,public.co_table,public.ao_part_table" 
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is a "co" table "public.co_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that there is no table "public.heap_table_ex" in "testdb"
        And verify that there is no table "public.ao_table_ex" in "testdb"
        And verify that there is no table "public.co_table_ex" in "testdb"
        And verify that there is no table "public.heap_part_table_ex" in "testdb"
        And verify that there is no table "public.ao_part_table_ex" in "testdb"
        And verify that there is no table "public.co_part_table_ex" in "testdb"
        And verify that there is no table "public.co_part_table" in "testdb"
        And verify that there is no table "public.heap_part_table" in "testdb"
        And verify that partitioned tables "ao_part_table" in "testdb" have 6 partitions
        And verify that the data of "12" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore with filtering different types of metadata
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "pepper.co_table_ex" with compression "None" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And the user runs "gpdbrestore -T pepper.ao_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "pepper.ao_table" in "testdb" with data
        And verify that there is no table "public.heap_table" in "testdb"
        And verify that there is no table "pepper.co_table_ex" in "testdb"
        And verify that the data of "1" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore of external and ao table 
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "public.co_table_ex" with compression "None" in "testdb" with data
        And there is an external table "ext_tab" in "testdb" with data for file "/tmp/ext_tab"
        And the user runs "psql -c 'CREATE LANGUAGE plpythonu' testdb"
        And there is a function "pymax" in "testdb"
        And the user runs "psql -c 'CREATE VIEW vista AS SELECT text 'Hello World' AS hello' testdb"
        And the user runs "psql -c 'COMMENT ON  TABLE public.ao_table IS 'Hello World' testdb"
        And the user runs "psql -c 'CREATE ROLE foo_user' testdb"
        And the user runs "psql -c 'GRANT INSERT ON TABLE public.ao_table TO foo_user' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.ao_table ADD CONSTRAINT null_check CHECK (column1 <> NULL);' testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a file "restore_file" with tables "public.ao_table,public.ext_tab" 
        And the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is a "external" table "public.ext_tab" in "testdb" with data
        And verify that there is a constraint "null_check" in "testdb"
        And verify that there is no table "public.heap_table" in "testdb"
        And verify that there is no table "public.co_table_ex" in "testdb"
        And verify that there is no view "vista" in "testdb"
        And verify that there is no procedural language "plpythonu" in "testdb"
        And the user runs "psql -c '\z' testdb"
        And psql should print foo_user=a/ to stdout
        And the user runs "psql -c '\df' testdb"
        And psql should not print pymax to stdout
        And verify that the data of "2" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore filtering tables with post data objects 
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" table "public.heap_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "public.co_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And there is a trigger "heap_trigger" on table "public.heap_table" in "testdb" based on function "heap_trigger_func"
        And there is a trigger "heap_ex_trigger" on table "public.heap_table_ex" in "testdb" based on function "heap_ex_trigger_func"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table_ex ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table_ex(column1, column2, column3);' testdb"
        And the user runs "psql -c """create rule heap_co_rule as on insert to heap_table where column1=100 do instead insert into co_table_ex values(27, 'restore', '2013-08-19');""" testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And there is a file "restore_file" with tables "public.ao_table,public.ao_index_table,public.heap_table" 
        When table "public.ao_index_table" is dropped in "testdb"
        And table "public.ao_table" is dropped in "testdb"
        And table "public.heap_table" is dropped in "testdb"
        And the index "bitmap_co_index" in "testdb" is dropped
        And the trigger "heap_ex_trigger" on table "public.heap_table_ex" in "testdb" is dropped
        Then the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table_ex" in "testdb" with data
        And verify that there is a "co" table "public.co_table_ex" in "testdb" with data
        And verify that there is a "co" table "public.co_index_table" in "testdb" with data
        And the user runs "psql -c '\d public.ao_index_table' testdb"
        And psql should print \"bitmap_ao_index\" bitmap \(column3\) to stdout
        And psql should print \"btree_ao_index\" btree \(column1\) to stdout
        And the user runs "psql -c '\d public.heap_table' testdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_trigger AFTER INSERT OR DELETE OR UPDATE ON heap_table FOR EACH STATEMENT EXECUTE PROCEDURE heap_trigger_func\(\) to stdout
        And psql should print heap_co_rule AS\n.*ON INSERT TO heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO co_table_ex \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table_ex\(column1, column2, column3\) to stdout
        And the user runs "psql -c '\d public.co_index_table' testdb"
        And psql should not print bitmap_co_index to stdout
        And the user runs "psql -c '\d public.heap_table_ex' testdb"
        And psql should not print heap_ex_trigger to stdout
        And verify that the data of "6" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore dropped database filtering tables with post data objects 
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" table "public.heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "public.co_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And there is a trigger "heap_trigger" on table "public.heap_table" in "testdb" based on function "heap_trigger_func"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table2 ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table2(column1, column2, column3);' testdb"
        And the user runs "psql -c """create rule heap_ao_rule as on insert to heap_table where column1=100 do instead insert into ao_table values(27, 'restore', '2013-08-19');""" testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And there is a file "restore_file" with tables "public.ao_table,public.ao_index_table,public.heap_table,public.heap_table2" 
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a trigger function "heap_trigger_func" on table "public.heap_table" in "testdb"
        Then the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table2" in "testdb" with data
        And verify that there is no table "public.co_table_ex" in "testdb"
        And verify that there is no table "public.co_index_table" in "testdb"
        And the user runs "psql -c '\d public.ao_index_table' testdb"
        And psql should print \"bitmap_ao_index\" bitmap \(column3\) to stdout
        And psql should print \"btree_ao_index\" btree \(column1\) to stdout
        And the user runs "psql -c '\d public.heap_table' testdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_trigger AFTER INSERT OR DELETE OR UPDATE ON heap_table FOR EACH STATEMENT EXECUTE PROCEDURE heap_trigger_func\(\) to stdout
        And psql should print heap_ao_rule AS\n.*ON INSERT TO heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO ao_table \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table2\(column1, column2, column3\) to stdout
        And verify that the data of "4" tables in "testdb" is validated after restore 

    Scenario: Full Backup and Restore filtering tables post data objects with -C
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "public.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" table "public.heap_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_table" with compression "None" in "testdb" with data
        And there is a "co" table "public.co_table_ex" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And there is a trigger "heap_trigger" on table "public.heap_table" in "testdb" based on function "heap_trigger_func"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table_ex ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table_ex(column1, column2, column3);' testdb"
        And the user runs "psql -c """create rule heap_co_rule as on insert to heap_table where column1=100 do instead insert into co_table_ex values(27, 'restore', '2013-08-19');""" testdb"
        When the user runs "gpcrondump -a -x testdb -C"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        When the index "bitmap_co_index" in "testdb" is dropped
        And the index "bitmap_ao_index" in "testdb" is dropped
        And the user runs "psql -c 'CREATE INDEX bitmap_ao_index_new ON public.ao_index_table USING bitmap(column3);' testdb"
        Then there is a file "restore_file" with tables "public.ao_table,public.ao_index_table,public.heap_table" 
        And the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "ao" table "public.ao_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table_ex" in "testdb" with data
        And verify that there is a "co" table "public.co_table_ex" in "testdb" with data
        And verify that there is a "co" table "public.co_index_table" in "testdb" with data
        And the user runs "psql -c '\d public.ao_index_table' testdb"
        And psql should print \"bitmap_ao_index\" bitmap \(column3\) to stdout
        And psql should print \"btree_ao_index\" btree \(column1\) to stdout
        And psql should not print bitmap_ao_index_new to stdout
        And the user runs "psql -c '\d public.heap_table' testdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_trigger AFTER INSERT OR DELETE OR UPDATE ON heap_table FOR EACH STATEMENT EXECUTE PROCEDURE heap_trigger_func\(\) to stdout
        And psql should print heap_co_rule AS\n.*ON INSERT TO heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO co_table_ex \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table_ex\(column1, column2, column3\) to stdout
        And the user runs "psql -c '\d public.co_index_table' testdb"
        And psql should not print bitmap_co_index to stdout
        And verify that the data of "6" tables in "testdb" is validated after restore 

    Scenario: Incremental Backup and Restore of specified post data objects
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "pepper" exists in "testdb"
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "pepper.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" partition table "heap_part_table" with compression "quicklz" in "testdb" with data
        And there is a "ao" table "pepper.ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "pepper.ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table" with compression "quicklz" in "testdb" with data
        And there is a "heap" table "heap_table_ex" with compression "None" in "testdb" with data
        And there is a "heap" partition table "heap_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "co" table "pepper.co_table_ex" with compression "None" in "testdb" with data
        And there is a "co" partition table "pepper.co_part_table_ex" with compression "quicklz" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY pepper.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table_ex ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -c 'ALTER TABLE ONLY pepper.heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table_ex(column1, column2, column3);' testdb"
        And the user runs "psql -c """create rule heap_co_rule as on insert to pepper.heap_table where column1=100 do instead insert into pepper.co_table_ex values(27, 'restore', '2013-08-19');""" testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And table "public.co_table" is assumed to be in dirty state in "testdb"
        And partition "2" of partition table "ao_part_table" is assumed to be in dirty state in "testdb" in schema "pepper"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And there is a file "restore_file" with tables "pepper.heap_table,pepper.ao_table,public.co_table,pepper.ao_part_table" 
        And table "pepper.heap_table" is dropped in "testdb"
        And table "pepper.ao_table" is dropped in "testdb"
        And table "public.co_table" is dropped in "testdb"
        And table "pepper.ao_part_table" is dropped in "testdb"
        When the index "bitmap_co_index" in "testdb" is dropped
        Then the user runs "gpdbrestore --table-file restore_file -a" with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "pepper.heap_table" in "testdb" with data
        And verify that there is a "ao" table "pepper.ao_table" in "testdb" with data
        And verify that there is a "co" table "public.co_table" in "testdb" with data
        And verify that there is a "ao" table "pepper.ao_part_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_table_ex" in "testdb" with data
        And verify that there is a "co" table "pepper.co_table_ex" in "testdb" with data
        And verify that there is a "heap" table "public.heap_part_table_ex" in "testdb" with data
        And verify that there is a "co" table "pepper.co_part_table_ex" in "testdb" with data
        And verify that there is a "co" table "public.co_part_table" in "testdb" with data
        And verify that there is a "heap" table "public.heap_part_table" in "testdb" with data
        And verify that there is a "co" table "public.co_index_table" in "testdb" with data
        And the user runs "psql -c '\d pepper.heap_table' testdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_co_rule AS\n.*ON INSERT TO pepper.heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO pepper.co_table_ex \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table_ex\(column1, column2, column3\) to stdout
        And the user runs "psql -c '\d public.co_index_table' testdb"
        And psql should not print bitmap_co_index to stdout
        And verify that the data of "51" tables in "testdb" is validated after restore 

    Scenario: Simple incremental for table names with multibyte (chinese) characters
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_table_name.sql testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/insert_data_multi_byte_char.sql testdb"
        When the user runs "gpcrondump --incremental -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp 
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char.sql testdb"
        Then psql should print 2000 to stdout 

    Scenario: Full Backup with option -T and Restore with exactly 1000 partitions
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_part_table" in "testdb" having "1000" partitions
        When the user runs "gpcrondump -a -x testdb -T public.ao_part_table"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "testdb" is saved for verification 
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "1" tables in "testdb" is validated after restore 

    Scenario: Testing pg_dump log messages
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "pg_dump testdb 2> /tmp/pg_dump_log -f /tmp/pg_dump"
        Then pg_dump should return a return code of 0
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading schemas"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined functions"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined types"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading type storage options"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading procedural languages"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined aggregate functions"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined operators"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined external protocols"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined operator classes"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined conversions"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading user-defined tables"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading table inheritance information"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading rewrite rules"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading type casts"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "finding inheritance relationships"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading column info for interesting tables"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "flagging inherited columns in subtables"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading indexes"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading constraints"
        And verify that the "pg_dump_log" file in "/tmp" dir does not contain "reading triggers"
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f /tmp/pg_dump testdb" 
        And psql should return a return code of 0
        And there are no report files in the master data directory

    Scenario: Testing pg_dump log messages with verbose option set
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "pg_dump testdb 2> /tmp/pg_dump_log -f /tmp/pg_dump --verbose"
        Then pg_dump should return a return code of 0
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading schemas"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined functions"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined types"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading type storage options"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading procedural languages"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined aggregate functions"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined operators"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined external protocols"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined operator classes"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined conversions"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading user-defined tables"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading table inheritance information"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading rewrite rules"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading type casts"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "finding inheritance relationships"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading column info for interesting tables"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "flagging inherited columns in subtables"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading indexes"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading constraints"
        And verify that the "pg_dump_log" file in "/tmp" dir contains "reading triggers"
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f /tmp/pg_dump testdb" 
        And psql should return a return code of 0
        And there are no report files in the master data directory

    Scenario: Single table restore with shared sequence across multiple tables
        Given the database is running
        And there are no backup files
        And the database "seqtestdb" does not exist
        And database "seqtestdb" exists
        And there is a sequence "shared_seq" in "seqtestdb"
        And the user runs "psql -c """CREATE TABLE table1 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" seqtestdb"
        And the user runs "psql -c """CREATE TABLE table2 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" seqtestdb"
        And the user runs "psql -c 'CREATE TABLE table3 (column1 INT4)' seqtestdb"
        And the user runs "psql -c 'CREATE TABLE table4 (column1 INT4)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table1 (price) SELECT * FROM generate_series(1000,1009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2000,2009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table3 SELECT * FROM generate_series(3000,3009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table4 SELECT * FROM generate_series(4000,4009)' seqtestdb"
        When the user runs "gpcrondump -x seqtestdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "seqtestdb" is saved for verification
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2010,2019)' seqtestdb"
        When table "public.table1" is dropped in "seqtestdb"
        And the user runs "gpdbrestore -T public.table1 -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.table1" in "seqtestdb" with data
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2020,2029)' seqtestdb"
        And verify that there are no duplicates in column "column1" of table "public.table2" in "seqtestdb"

    Scenario: Single table restore from an incremental backup with shared sequence across multiple tables
        Given the database is running
        And there are no backup files
        And the database "seqtestdb" does not exist
        And database "seqtestdb" exists
        And there is a sequence "shared_seq" in "seqtestdb"
        And the user runs "psql -c """CREATE TABLE table1 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" seqtestdb"
        And the user runs "psql -c """CREATE TABLE table2 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" seqtestdb"
        And the user runs "psql -c 'CREATE TABLE table3 (column1 INT4)' seqtestdb"
        And the user runs "psql -c 'CREATE TABLE table4 (column1 INT4)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table1 (price) SELECT * FROM generate_series(1000,1009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2000,2009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table3 SELECT * FROM generate_series(3000,3009)' seqtestdb"
        And the user runs "psql -c 'INSERT INTO table4 SELECT * FROM generate_series(4000,4009)' seqtestdb"
        When the user runs "gpcrondump -x seqtestdb -a -t public.table1 -t public.table2 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the user runs "psql -c 'INSERT INTO table1 (price) SELECT * FROM generate_series(2010,2019)' seqtestdb"
        When the user runs "gpcrondump -x seqtestdb --incremental --prefix=foo -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "seqtestdb" is saved for verification
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2010,2019)' seqtestdb"
        When table "public.table1" is dropped in "seqtestdb"
        And the user runs "gpdbrestore -T public.table1 --prefix=foo -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.table1" in "seqtestdb" with data
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2020,2029)' seqtestdb"
        And verify that there are no duplicates in column "column1" of table "public.table2" in "seqtestdb"

    Scenario: Single table backup of the table with partitions in a separate schema
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "schema_parent, schema_child" exists in "testdb"
        And there is a "ao" partition table "schema_parent.ao_part_table" with compression "None" in "testdb" with data
        And partition "1" of partition table "schema_parent.ao_part_table" is assumed to be in schema "schema_child" in database "testdb"
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -a -v -x testdb -t schema_parent.ao_part_table"
        Then gpcrondump should return a return code of 0

    Scenario: Database backup, while one of the tables have a partition in a separate schema
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "schema_parent, schema_child" exists in "testdb"
        And there is a "ao" partition table "schema_parent.ao_part_table" with compression "None" in "testdb" with data
        And there is a mixed storage partition table "public.part_mixed_1" in "testdb" with data
        And there is a "heap" table "schema_parent.heap_table" with compression "None" in "testdb" with data
        And partition "1" of partition table "schema_parent.ao_part_table" is assumed to be in schema "schema_child" in database "testdb"
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -a -v -x testdb -t schema_parent.ao_part_table"
        Then gpcrondump should return a return code of 0

    Scenario: Database backup, while one of the tables have all the partitions in a separate schema
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "schema_parent, schema_child" exists in "testdb"
        And there is a "ao" partition table "schema_child.ao_part_table" with compression "None" in "testdb" with data
        And the user runs "psql -d testdb -c 'alter table schema_child.ao_part_table set schema schema_parent'"
        And there is a mixed storage partition table "public.part_mixed_1" in "testdb" with data
        And there is a "heap" table "schema_parent.heap_table" with compression "None" in "testdb" with data
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -a -v -x testdb -t schema_parent.ao_part_table"
        Then gpcrondump should return a return code of 0

    Scenario: Incremental backup of partitioned tables which have child partitions in different schema
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "schema_parent, schema_child" exists in "testdb"
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "schema_parent.ao_part_table" with compression "None" in "testdb" with data
        And partition "1" of partition table "schema_parent.ao_part_table" is assumed to be in schema "schema_child" in database "testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And partition "1" of partition table "ao_part_table" is assumed to be in dirty state in "testdb" in schema "schema_parent"
        And the user runs "gpcrondump -a --incremental -x testdb"
        Then gpcrondump should return a return code of 0

    Scenario: Full backup should not backup temp tables
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "a" exists in "testdb"
        And there is a "heap" table "a.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "a.ao_part_table" with compression "None" in "testdb" with data
        And there is schema "b" exists in "testdb"
        And there is a "heap" table "b.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "b.ao_part_table" with compression "None" in "testdb" with data
        And there is schema "z" exists in "testdb"
        And there is a "heap" table "z.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "z.ao_part_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "testdb" with data
        And the user runs the query "CREATE TEMP TABLE temp_1 as select generate_series(1, 100);" on "testdb" for "120" seconds
        When the user runs "gpcrondump -x testdb -a"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "40" tables in "testdb" is validated after restore

    Scenario: Incremental backup should not backup temp tables
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "a" exists in "testdb"
        And there is a "heap" table "a.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "a.ao_part_table" with compression "None" in "testdb" with data
        And there is schema "b" exists in "testdb"
        And there is a "heap" table "b.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "b.ao_part_table" with compression "None" in "testdb" with data
        And there is schema "z" exists in "testdb"
        And there is a "heap" table "z.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "z.ao_part_table" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "testdb" with data
        When the user runs "gpcrondump -x testdb -a"
        And gpcrondump should return a return code of 0
        And the user runs the query "CREATE TEMP TABLE temp_1 as select generate_series(1, 100);" on "testdb" for "120" seconds
        When the user runs "gpcrondump -x testdb -a --incremental"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "40" tables in "testdb" is validated after restore

    Scenario: Full backup and restore using gpcrondump with drop table in the middle
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data and 1000000 rows
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        And all the data from "fullbkdb" is saved for verification
        Then the user runs the query "drop table heap_table" in database "fullbkdb" in a worker pool "w1" as soon as pg_class is locked
        And the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the worker pool "w1" is cleaned up
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data

    @42backupsmoke
    @wip
    Scenario: Full backup and restore with pending drop table transaction
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data and 1000000 rows
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        And all the data from "fullbkdb" is saved for verification
        And the user drops "heap_table" in "fullbkdb" in a worker pool "w1"
        Then the user runs the "gpcrondump -a -x fullbkdb" in a worker pool "w2"
        And the worker pool "w2" is cleaned up
        And the timestamp from gpcrondump is stored
        And the worker pool "w1" is cleaned up
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data

    @wip
    Scenario: Full Backup and Restore using gp_dump without no-lock
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data and 1000000 rows
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs the "gp_dump --gp-d=db_dumps --gp-s=p --gp-c fullbkdb" in a worker pool "w1"
        And this test sleeps for "1" seconds
        And the worker pool "w1" is cleaned up
        Then gp_dump should return a return code of 0
        And the timestamp from gp_dump is stored and subdir is " "
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the user runs gp_restore with the the stored timestamp and subdir in "fullbkdb"
        And gp_restore should return a return code of 0
        And verify that there is a "heap" table "heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "ao_part_table" in "fullbkdb" with data
        And there are no report files in the master data directory

    Scenario: Full Backup and Restore using gp_dump with no-lock
        Given the database is running
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data and 1000000 rows
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "fullbkdb" exists for validation
        When the user runs the "gp_dump --gp-s=p --gp-c --no-lock fullbkdb" in a worker pool "w1"
        And this test sleeps for "2" seconds
        And the worker pool "w1" is cleaned up
        Then gp_dump should return a return code of 0

    Scenario: Restore -T for incremental dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is schema "schema_heap" exists in "testdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "testdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "testdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql testdb"
        When the user runs "gpcrondump --incremental -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' testdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "ao_index_table, co_index_table, heap_index_table" in "testdb" exists for validation
        And table "public.ao_index_table" is dropped in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql testdb" 
        When the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql testdb"
        Then psql should print 2000 to stdout 4 times
        And verify that there is a "ao" table "ao_index_table" in "testdb" with data
        And verify that there is a "co" table "co_index_table" in "testdb" with data
        And verify that there is a "heap" table "heap_index_table" in "testdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' testdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    Scenario: Restore -T for full dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "testdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "testdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "testdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify the metadata dump file syntax under " " for comments and types
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' testdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "ao_index_table, co_index_table, heap_index_table" in "testdb" exists for validation
        And table "public.ao_index_table" is dropped in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql testdb" 
        When the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql testdb"
        Then psql should print 1000 to stdout 4 times
        And verify that there is a "ao" table "ao_index_table" in "testdb" with data
        And verify that there is a "co" table "co_index_table" in "testdb" with data
        And verify that there is a "heap" table "heap_index_table" in "testdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' testdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    Scenario: Restore -T for full dump should restore GRANT privileges for tablenames with English and multibyte (chinese) characters
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql testdb"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" testdb"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" testdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_3" with index "heap_index_3" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_3 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_3 TO customer;" testdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_3 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_3 TO select_group;" testdb"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" testdb"""
        When the user runs "psql -c '\d customer.heap_index_table_1' testdb > /tmp/describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' testdb > /tmp/privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' testdb > /tmp/describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' testdb > /tmp/privileges_heap_index_table_2_before"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_before"
        When the user runs "gpcrondump -x testdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When there is a backupfile of tables "customer.heap_index_table_1, customer.heap_index_table_2, customer.heap_index_table_3" in "testdb" exists for validation
        And table "customer.heap_index_table_1" is dropped in "testdb"
        And table "customer.heap_index_table_2" is dropped in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql testdb" 
        And the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql testdb"
        Then psql should print 1000 to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "testdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "testdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_3" in "testdb" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' testdb > /tmp/describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' testdb > /tmp/privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' testdb > /tmp/describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' testdb > /tmp/privileges_heap_index_table_2_after"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_after"
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
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/grant_multi_byte_char_table_name.sql testdb"
        And the user runs """psql -c "CREATE ROLE test_gpadmin LOGIN ENCRYPTED PASSWORD 'changeme' SUPERUSER INHERIT CREATEDB CREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE customer LOGIN ENCRYPTED PASSWORD 'changeme' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE select_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE ROLE test_group NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE pg_default;" testdb"""
        And the user runs """psql -c "CREATE SCHEMA customer AUTHORIZATION test_gpadmin" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_gpadmin;" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO customer;" testdb"""
        And the user runs """psql -c "GRANT USAGE ON SCHEMA customer TO select_group;" testdb"""
        And the user runs """psql -c "GRANT ALL ON SCHEMA customer TO test_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_1" with index "heap_index_1" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_1 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO customer;" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_1 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_1 TO select_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_2" with index "heap_index_2" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_2 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_2 TO customer;" testdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_2 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_2 TO select_group;" testdb"""
        And there is a "heap" table "customer.heap_index_table_3" with index "heap_index_3" compression "None" in "testdb" with data
        And the user runs """psql -c "ALTER TABLE customer.heap_index_table_3 owner to customer" testdb"""
        And the user runs """psql -c "GRANT ALL ON TABLE customer.heap_index_table_3 TO customer;" testdb"""
        And the user runs """psql -c "GRANT SELECT, UPDATE, INSERT, DELETE ON TABLE customer.heap_index_table_3 TO test_group;" testdb"""
        And the user runs """psql -c "GRANT SELECT ON TABLE customer.heap_index_table_3 TO select_group;" testdb"""
        And the user runs """psql -c "ALTER ROLE customer SET search_path = customer, public;" testdb"""
        When the user runs "psql -c '\d customer.heap_index_table_1' testdb > /tmp/describe_heap_index_table_1_before"
        And the user runs "psql -c '\dp customer.heap_index_table_1' testdb > /tmp/privileges_heap_index_table_1_before"
        And the user runs "psql -c '\d customer.heap_index_table_2' testdb > /tmp/describe_heap_index_table_2_before"
        And the user runs "psql -c '\dp customer.heap_index_table_2' testdb > /tmp/privileges_heap_index_table_2_before"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_before"
        When the user runs "gpcrondump -x testdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And table "customer.heap_index_table_1" is assumed to be in dirty state in "testdb"
        And table "customer.heap_index_table_2" is assumed to be in dirty state in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql testdb"
        When the user runs "gpcrondump --incremental -x testdb -g -G -a -b -v -u /tmp --rsyncable"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When there is a backupfile of tables "customer.heap_index_table_1, customer.heap_index_table_2, customer.heap_index_table_3" in "testdb" exists for validation
        And table "customer.heap_index_table_1" is dropped in "testdb"
        And table "customer.heap_index_table_2" is dropped in "testdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql testdb" 
        And the user runs "gpdbrestore --table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_grant_permissions -u /tmp -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql testdb"
        Then psql should print 2000 to stdout 4 times
        And verify that there is a "heap" table "customer.heap_index_table_1" in "testdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_2" in "testdb" with data
        And verify that there is a "heap" table "customer.heap_index_table_3" in "testdb" with data
        When the user runs "psql -c '\d customer.heap_index_table_1' testdb > /tmp/describe_heap_index_table_1_after"
        And the user runs "psql -c '\dp customer.heap_index_table_1' testdb > /tmp/privileges_heap_index_table_1_after"
        And the user runs "psql -c '\d customer.heap_index_table_2' testdb > /tmp/describe_heap_index_table_2_after"
        And the user runs "psql -c '\dp customer.heap_index_table_2' testdb > /tmp/privileges_heap_index_table_2_after"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql testdb > /tmp/describe_multi_byte_char_after"
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

    Scenario: Redirected Restore (RR) Full Backup and Restore without -e option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the database "testdb" does not exist
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=testdb -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data

    Scenario: (RR) Full Backup and Restore with -e option
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the database "testdb" does not exist
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=testdb -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data

    Scenario: (RR) Incremental Backup and Restore 
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the database "testdb" does not exist
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "ao_part_table1" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -x fullbkdb -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table1" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -x fullbkdb -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        And the user runs "gpdbrestore --redirect=testdb -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "testdb" is validated after restore 

    Scenario: (RR) Full backup and restore with -T
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the database "testdb" does not exist
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user truncates "public.ao_index_table" tables in "fullbkdb"
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=testdb -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data

    Scenario: (RR) Full backup and restore with -T and --truncate
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And the database "testdb" does not exist
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "fullbkdb" is saved for verification
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=testdb --truncate -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        And gpdbrestore should print Failure from truncating tables, FATAL:  database "testdb" does not exist to stdout
        And the user runs "gpdbrestore -T public.ao_index_table_1 --truncate -a" with the stored timestamp
        And gpdbrestore should return a return code of 2
        And gpdbrestore should print Could not truncate table fullbkdb.public.ao_index_table_1 to stdout
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And the user runs "gpdbrestore -T public.ao_index_table --redirect=testdb --truncate -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data

    Scenario: (RR) Incremental restore with table filter
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And the database "testdb" does not exist
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "schematestdb"
        And table "co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.co_table --redirect=testdb"
        Then gpdbrestore should return a return code of 0
        And verify that exactly "2" tables in "testdb" have been restored 

    Scenario: (RR) Full Backup and Restore with --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the database "testdb1" does not exist
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --redirect=testdb1"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table" in "testdb1" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb1" with data

    Scenario: (RR) Full Backup and Restore with --prefix option for multiple databases
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the database "testdb1" does not exist
        And database "testdb1" exists
        And the database "testdb2" does not exist
        And database "testdb2" exists
        And there is a "heap" table "heap_table1" with compression "None" in "testdb1" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb1" with data
        And there is a backupfile of tables "heap_table1, ao_part_table" in "testdb1" exists for validation
        And there is a "heap" table "heap_table2" with compression "None" in "testdb2" with data
        And there is a backupfile of tables "heap_table2" in "testdb2" exists for validation
        When the user runs "gpcrondump -a -x testdb1,testdb2 --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --redirect=testdb"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table1" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    Scenario: (RR) Incremental Backup and Restore with -T filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the database "testdb1" does not exist
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "heap_table1" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb --prefix=foo -T public.ao_part_table -T public.heap_table2"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table1"
        And table "public.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And the user runs "gpcrondump -x testdb --incremental --prefix=foo -a --list-filter-tables"
        And gpcrondump should return a return code of 0
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --redirect=testdb1" 
        And gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "public.heap_table1" in "testdb1" with data
        And verify that there is a "ao" table "public.ao_index_table" in "testdb1" with data
        And verify that there is no table "public.ao_part_table" in "testdb1"
        And verify that there is no table "public.heap_table2" in "testdb1"

    Scenario: Full Backup and Restore with the master dump file missing
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "rm $MASTER_DATA_DIRECTORY/db_dumps/*/gp_dump_1_1*"
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to find .* or .*. Skipping restore. to stdout

    Scenario: Full Backup and Restore with the master dump file missing without compression
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -x testdb  -z -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "rm $MASTER_DATA_DIRECTORY/db_dumps/*/gp_dump_1_1*"
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to find .* or .*. Skipping restore. to stdout

    Scenario: Incremental Backup and Restore with the master dump file missing
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x testdb -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "rm $MASTER_DATA_DIRECTORY/db_dumps/*/gp_dump_1_1*"
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Unable to find .* or .*. Skipping restore. to stdout

    Scenario: Uppercase Dbname (UD) Full Backup and Restore using timestamp
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    Scenario: Uppercase Dbname (UD) Full Backup and Restore using -s option
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore -s TESTING -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    Scenario: Uppercase Dbname (UD) Full Backup and Restore using -s option DBname in quotes for gpdbrestore
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore -s \"TESTING\" -e -a"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    Scenario: (UD) Incremental Backup and Restore
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table1" with compression "quicklz" in "TESTING" with data
        When the user runs "gpcrondump -x TESTING -a"
        Then gpcrondump should return a return code of 0
        And table "public.ao_part_table1" is assumed to be in dirty state in "TESTING"
        When the user runs "gpcrondump -x TESTING -a --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "TESTING" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that the data of "10" tables in "TESTING" is validated after restore 

    Scenario: (UD) Full backup and restore with -T
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "TESTING" with data
        And there is a "ao" table "ao_index_table" with compression "None" in "TESTING" with data
        When the user runs "gpcrondump -a -x TESTING"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "TESTING" is saved for verification
        When the user truncates "public.ao_index_table" tables in "TESTING"
        And the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "ao" table "public.ao_index_table" in "TESTING" with data

    Scenario: (UD) Incremental restore with table filter
        Given the database is running
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" table "ao_table" with compression "None" in "TESTING" with data
        And there is a "ao" table "ao_table2" with compression "None" in "TESTING" with data
        And there is a "co" table "co_table" with compression "None" in "TESTING" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x TESTING"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "TESTING"
        And table "co_table" is assumed to be in dirty state in "TESTING"
        And the user runs "gpcrondump -a --incremental -x TESTING"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "TESTING" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.co_table"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that exactly "2" tables in "TESTING" have been restored

    Scenario: (UD) Full Backup and Restore with --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "TESTING" does not exist
        And database "TESTING" exists
        And there is a "heap" table "heap_table" with compression "None" in "TESTING" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "TESTING" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "TESTING" exists for validation
        When the user runs "gpcrondump -a -x TESTING --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table" in "TESTING" with data
        And verify that there is a "ao" table "ao_part_table" in "TESTING" with data

    Scenario: Full backup and Restore should create the gp_toolkit schema with -e option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And the gp_toolkit schema for "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "testdb" is validated after restore
        And the gp_toolkit schema for "testdb" is verified after restore 

    Scenario: Incremental backup and Restore should create the gp_toolkit schema with -e option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And the gp_toolkit schema for "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -x testdb --incremental -a"
        THen gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "testdb" is validated after restore
        And the gp_toolkit schema for "testdb" is verified after restore 

    Scenario: Redirected Restore should create the gp_toolkit schema with -e option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And the gp_toolkit schema for "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=fullbkdb -e -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "fullbkdb" is validated after restore
        And the gp_toolkit schema for "fullbkdb" is verified after restore 

    Scenario: Redirected Restore should create the gp_toolkit schema without -e option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And the database "fullbkdb" does not exist
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And the gp_toolkit schema for "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --redirect=fullbkdb -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that the data of "10" tables in "fullbkdb" is validated after restore
        And the gp_toolkit schema for "fullbkdb" is verified after restore 

    Scenario: gpdbrestore with noanalyze
        Given the database is running
        And there are no backup files        
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "gpdbrestore --noanalyze -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbestore should print Analyze bypassed on request to stdout
        And verify that the data of "10" tables in "testdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "testdb"

    Scenario: gpdbrestore without noanalyze
        Given the database is running
        And there are no backup files        
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "testdb" is saved for verification
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "gpdbrestore -a" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And gpdbestore should print Commencing analyze of testdb database to stdout
        And gpdbestore should print Analyze of testdb completed without error to stdout
        And verify that the data of "10" tables in "testdb" is validated after restore 
        And verify that the tuple count of all appendonly tables are consistent in "testdb"

    Scenario: Writable Report/Status Directory (WRD) Full Backup and Restore without --report-status-dir option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print gp-r to stdout
        And gpdbestore should not print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"

    Scenario: Writable Report/Status Directory (WRD) Full Backup and Restore with --report-status-dir option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore --report-status-dir=/tmp -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbestore should print gp-r to stdout
        And gpdbestore should print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that report file is generated in /tmp
        And verify that status file is generated in /tmp
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"

    Scenario: Writable Report/Status Directory (WRD) Full Backup and Restore with -u option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a -u /tmp -K 20140227010101" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "gpdbrestore -u /tmp -e -a -t 20140227010101"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should print gp-r to stdout
        And gpdbestore should print status to stdout
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that report file is generated in /tmp/db_dumps/20140227
        And verify that status file is generated in /tmp/db_dumps/20140227
        And the backup files in "/tmp" are deleted

    Scenario: Writable Report/Status Directory (WRD) Full Backup and Restore with no write access -u option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And the backup files in "/tmp" are deleted
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        When the user runs "gpcrondump -x testdb -a -u /tmp -K 20140227010101" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "chmod -R 555 /tmp/db_dumps"
        And the user runs "gpdbrestore -u /tmp -e -a -t 20140227010101"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print gp-r to stdout
        And gpdbestore should not print --status= to stdout
        And verify that there is a "heap" table "public.heap_table" in "testdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "testdb" with data
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And the user runs command "chmod -R 777 /tmp/db_dumps"
        And the backup files in "/tmp" are deleted

    Scenario: Writable Report/Status Directory (WRD) Full Backup and Restore with --report-status-dir option
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there are no backup files
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -x testdb -a" 
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs command "mkdir -p /tmp/test"
        And the user runs command "chmod -R 555 /tmp/test"
        And the user runs "gpdbrestore --report-status-dir=/tmp/test -e -a" with the stored timestamp
        Then gpdbrestore should return a return code of 2
        And the user runs command "chmod -R 777 /tmp/test"
        And the user runs command "rm -rf /tmp/test"

    Scenario: WRD Incremental restore with table filter with --report-status-dir
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there are no backup files
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "schematestdb"
        And table "co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.co_table --report-status-dir=/tmp"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should print gp-r to stdout
        And gpdbestore should print --status= to stdout
        And verify that exactly "2" tables in "schematestdb" have been restored 
        And verify that report file is generated in /tmp
        And verify that status file is generated in /tmp
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"

    Scenario: WRD Incremental restore with table filter and no --report-status-dir
        Given the database is running
        And the database "schematestdb" does not exist
        And database "schematestdb" exists
        And there are no backup files
        And there are no report files in "/tmp"
        And there are no status files in "/tmp"
        And there is a "heap" table "heap_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table" with compression "None" in "schematestdb" with data
        And there is a "ao" table "ao_table2" with compression "None" in "schematestdb" with data
        And there is a "co" table "co_table" with compression "None" in "schematestdb" with data
        And there are no backup files
        When the user runs "gpcrondump -a -x schematestdb"
        And gpcrondump should return a return code of 0
        And table "ao_table2" is assumed to be in dirty state in "schematestdb"
        And table "co_table" is assumed to be in dirty state in "schematestdb"
        And the user runs "gpcrondump -a --incremental -x schematestdb"
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And all the data from "schematestdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table2,public.co_table"
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print gp-r to stdout
        And gpdbestore should not print --status= to stdout
        And verify that exactly "2" tables in "schematestdb" have been restored 
        And verify that report file is generated in master_data_directory
        And verify that status file is generated in segment_data_directory
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"

    @backupfire
    Scenario: Filtered Full Backup with Partition Table
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "ao_index_table" in "fullbkdb" 
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data
        And verify that the data of "9" tables in "fullbkdb" is validated after restore

    @backupfire
    Scenario: Filtered Incremental Backup with Partition Table
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And table "ao_index_table" is assumed to be in dirty state in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        When the user runs "gpdbrestore -e -T public.ao_part_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "ao_index_table" in "fullbkdb" 
        And verify that there is no table "heap_table" in "fullbkdb"
        And verify that there is a "ao" table "public.ao_part_table" in "fullbkdb" with data
        And verify that the data of "9" tables in "fullbkdb" is validated after restore

    Scenario: Gpdbrestore runs 'ANALYZE' on restored table only
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "fullbkdb" with data 
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "fullbkdb" with data 
        And there is a "ao" table "ao_index_table" with compression "None" in "fullbkdb" with data 
        And the database "fullbkdb" is analyzed
        When the user runs "gpcrondump -a -x fullbkdb"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored 
        And all the data from "fullbkdb" is saved for verification
        And the user truncates "public.ao_index_table" tables in "fullbkdb"
        And the user deletes rows from the table "heap_table" of database "fullbkdb" where "column1" is "1088"
        When the user runs "gpdbrestore -T public.ao_index_table -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_index_table" in "fullbkdb" with data
        And verify that the restored table "public.ao_index_table" in database "fullbkdb" is analyzed
        And verify that the table "public.heap_table" in database "fullbkdb" is not analyzed

    Scenario: Gpcrondump with --email-file option
        Given the database is running
        And the database "testdb1" does not exist
        And the database "testdb2" does not exist
        And database "testdb1" exists
        And database "testdb2" exists
        And there is a "heap" table "heap_table1" with compression "None" in "testdb1" with data
        And there is a "heap" table "heap_table2" with compression "None" in "testdb2" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "gppylib/test/behave/mgmt_utils/steps/data/test_email_details.yaml" stores email details is in proper format
        When the user runs "gpcrondump -a -x testdb1,testdb2 --email-file gppylib/test/behave/mgmt_utils/steps/data/test_email_details.yaml --verbose"
        Then gpcrondump should return a return code of 0
        And verify that emails are sent to the given contacts with appropriate messages after backup of "testdb1,testdb2"
        And the mail_contacts file does not exist

    Scenario: Gpcrondump without mail_contacts
        Given the database is running
        And the database "testdb1" does not exist
        And database "testdb1" exists
        And there is a "heap" table "heap_table1" with compression "None" in "testdb1" with data
        And the mail_contacts file does not exist
        When the user runs "gpcrondump -a -x testdb1"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print unable to send dump email notification to stdout as warning

    Scenario: Negative case for gpcrondump with --email-file option
        Given the database is running
        And the database "testdb1" does not exist
        And database "testdb1" exists
        And there is a "heap" table "heap_table1" with compression "None" in "testdb1" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "gppylib/test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml" stores email details is not in proper format
        When the user runs "gpcrondump -a -x testdb1 --email-file gppylib/test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml --verbose"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print file is not formatted properly to stdout
        And the mail_contacts file does not exist

    @backupfire
    @slb
    Scenario: Full Backup with multiple -S option and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap1.heap_table1, schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -S schema_heap -S schema_heap1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "fullbkdb"
        And verify that there is no table "schema_heap1.heap_table1" in "fullbkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    @backupfire
    @slb
    Scenario: Full Backup with multiple -s option and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, schema_heap1.heap_table1" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -s schema_heap -s schema_heap1"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "fullbkdb" with data
        And verify that there is a "heap" table "schema_heap1.heap_table1" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "fullbkdb"

    @backupfire
    @slb
    Scenario: Full Backup with option -S and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -S schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is no table "schema_heap.heap_table" in "fullbkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    @backupfire
    @slb
    Scenario: Full Backup with option -s and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        When the user runs "gpcrondump -a -x fullbkdb -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "fullbkdb"

    @backupfire
    @slb
    Scenario: Full Backup with option --exclude-schema-file and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, schema_heap1.heap_table1" in "fullbkdb" exists for validation
        And there is a file "exclude_file" with tables "schema_heap1,schema_ao" 
        When the user runs "gpcrondump -a -x fullbkdb --exclude-schema-file exclude_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_part_table" in "fullbkdb"
        And verify that there is no table "schema_heap1.heap_table" in "fullbkdb"

    @backupfire
    @slb
    Scenario: Full Backup with option --schema-file and Restore
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table, schema_heap1.heap_table1" in "fullbkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap,schema_ao" 
        When the user runs "gpcrondump -a -x fullbkdb --schema-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "fullbkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data
        And verify that there is no table "schema_heap1.heap_table" in "fullbkdb"

    @backupsmoke
    @slb
    Scenario: Full Backup and Restore with --prefix option
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "quicklz" in "testdb" with data
        And there is a backupfile of tables "heap_table, ao_part_table" in "testdb" exists for validation
        When the user runs "gpcrondump -a -x testdb --prefix=foo"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored 
        And the user runs gpdbrestore with the stored timestamp and options "--prefix=foo"
        And gpdbrestore should return a return code of 0
        And there should be dump files under " " with prefix "foo"
        And verify that there is a "heap" table "heap_table" in "testdb" with data
        And verify that there is a "ao" table "ao_part_table" in "testdb" with data

    @backupsmoke
    @slb
    Scenario: Incremental Backup and Restore without --prefix option
        Given the database is running
        And there are no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "testdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "testdb" with data
        And there is a "heap" table "schema_heap1.heap_table1" with compression "None" in "testdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb -s schema_heap -s schema_ao"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And table "schema_ao.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And table "schema_ao.ao_index_table" is assumed to be in dirty state in "testdb"
        When the user runs "gpcrondump -a -x testdb --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "testdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp
        And gpdbrestore should return a return code of 0 
        And verify that the data of "12" tables in "testdb" is validated after restore 

    @filter
    @slb
    Scenario: Incremental Backup and Restore with -s filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table2" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo -s schema_ao -s schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "schema_ao.ao_index_table"
        And verify that the "filter" file in " " dir contains "schema_heap.heap_table1"
        And there is a "heap" table "schema_heap.heap_table2" with compression "None" in "fullbkdb" with data
        And table "schema_heap.heap_table1" is dropped in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "fullbkdb" is saved for verification
        And the user runs "psql -c 'drop table schema_heap.heap_table2' fullbkdb"
        And the user runs "psql -c 'drop table schema_ao.ao_index_table' fullbkdb"
        And the user runs "psql -c 'drop table schema_ao.ao_part_table' fullbkdb"
        When the user runs "gpdbrestore --prefix=foo -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "schema_heap.heap_table2" in "fullbkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_index_table" in "fullbkdb" with data
        And verify that there is no table "schema_heap.heap_table1" in "fullbkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    @filter
    @slb
    Scenario: Incremental Backup and Restore with --schema-file filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table2" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "schema_ao,schema_heap"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --schema-file /tmp/schema_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "schema_ao.ao_index_table"
        And verify that the "filter" file in " " dir contains "schema_heap.heap_table1"
        And there is a "heap" table "schema_heap.heap_table2" with compression "None" in "fullbkdb" with data
        And partition "3" is added to partition table "schema_ao.ao_part_table" in "fullbkdb"
        And partition "2" is dropped from partition table "schema_ao.ao_part_table" in "fullbkdb"
        And table "schema_heap.heap_table1" is dropped in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "fullbkdb" is saved for verification
        And the user runs "psql -c 'drop table schema_heap.heap_table2' fullbkdb"
        And the user runs "psql -c 'drop table schema_ao.ao_index_table' fullbkdb"
        And the user runs "psql -c 'drop table schema_ao.ao_part_table' fullbkdb"
        When the user runs "gpdbrestore --prefix=foo -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0 
        And verify that there is no table "schema_heap.heap_table1" in "fullbkdb"
        And verify that there is a "heap" table "schema_heap.heap_table2" in "fullbkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_index_table" in "fullbkdb" with data
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    @filter
    @slb
    Scenario: Incremental Backup and Restore with --exclude-schema-file filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table2" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "schema_heap1,schema_heap"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --exclude-schema-file /tmp/schema_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "schema_ao.ao_index_table"
        And there is a "heap" table "schema_heap.heap_table2" with compression "None" in "fullbkdb" with data
        And table "schema_ao.ao_index_table" is dropped in "fullbkdb"
        And partition "3" is added to partition table "schema_ao.ao_part_table" in "fullbkdb"
        And partition "2" is dropped from partition table "schema_ao.ao_part_table" in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "fullbkdb" is saved for verification
        And the user runs "psql -c 'drop table schema_ao.ao_index_table' fullbkdb"
        And the user runs "psql -c 'drop table schema_ao.ao_part_table' fullbkdb"
        When the user runs "gpdbrestore --prefix=foo -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "schema_heap.heap_table1" in "fullbkdb" with data
        And verify that there is a "heap" table "schema_heap.heap_table2" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_index_table" in "fullbkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    @filter
    @slb
    Scenario: Incremental Backup and Restore with -S filter for Full
        Given the database is running
        And the prefix "foo" is stored
        And there are no backup files
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is a list to store the incremental backup timestamps
        And there is schema "schema_heap, schema_ao, schema_heap1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table1" with compression "None" in "fullbkdb" with data
        And there is a "heap" table "schema_heap1.heap_table2" with compression "None" in "fullbkdb" with data
        And there is a "ao" table "schema_ao.ao_index_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a table-file "/tmp/schema_file" with tables "schema_heap1,schema_heap"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo -S schema_heap1 -S schema_heap"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored 
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "schema_ao.ao_index_table"
        And there is a "heap" table "schema_heap.heap_table2" with compression "None" in "fullbkdb" with data
        And table "schema_ao.ao_part_table" is assumed to be in dirty state in "fullbkdb"
        And table "schema_ao.ao_index_table" is dropped in "fullbkdb"
        When the user runs "gpcrondump -a -x fullbkdb --prefix=foo --incremental"
        Then gpcrondump should return a return code of 0 
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "fullbkdb" is saved for verification
        And the user runs "psql -c 'drop table schema_ao.ao_part_table' fullbkdb"
        When the user runs "gpdbrestore --prefix=foo -a" with the stored timestamp
        Then gpdbrestore should return a return code of 0 
        And verify that there is a "heap" table "schema_heap.heap_table1" in "fullbkdb" with data
        And verify that there is a "heap" table "schema_heap.heap_table2" in "fullbkdb" with data
        And verify that there is no table "schema_ao.ao_index_table" in "fullbkdb"
        And verify that there is a "ao" table "schema_ao.ao_part_table" in "fullbkdb" with data

    Scenario: pg_dump correctly backup aggregate functions
        Given the user runs "deletedb test_pg_dump; createdb test_pg_dump"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/pg_dump_function_test.sql test_pg_dump"
        When the user runs "pg_dump -s -n public -f /tmp/public_pg_dump_ddl.dmp test_pg_dump"
        Then verify that function is backedup correctly in "/tmp/public_pg_dump_ddl.dmp"

    @filter
    Scenario: Full Backup and Restore with option --change-schema
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_new1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap.heap_table,schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x fullbkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Full"
        And the user runs "gpdbrestore --change-schema=schema_new1 -a --table-file include_file" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new1.heap_table" of "heap" type in "fullbkdb" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new1.ao_part_table" of "ao" type in "fullbkdb" with same data as table "schema_ao.ao_part_table"

    @filter
    Scenario: Incremental Backup and Restore with option --change-schema
        Given the database is running
        And the database "fullbkdb" does not exist
        And database "fullbkdb" exists
        And there is schema "schema_heap, schema_ao, schema_new1" exists in "fullbkdb"
        And there is a "heap" table "schema_heap.heap_table" with compression "None" in "fullbkdb" with data
        And there is a "ao" partition table "schema_ao.ao_part_table" with compression "quicklz" in "fullbkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_part_table" in "fullbkdb" exists for validation
        And there is a file "include_file" with tables "schema_heap.heap_table,schema_ao.ao_part_table"
        When the user runs "gpcrondump -a -x fullbkdb --table-file include_file"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And table "schema_ao.ao_part_table" is assumed to be in dirty state in "fullbkdb"
        And the user runs "gpcrondump -a --incremental -x fullbkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that the "report" file in " " dir contains "Backup Type: Incremental"
        And the user runs "gpdbrestore --change-schema=schema_new1 -a --table-file include_file" with the stored timestamp
        And gpdbrestore should return a return code of 0
        And verify that there is a table "schema_new1.heap_table" of "heap" type in "fullbkdb" with same data as table "schema_heap.heap_table"
        And verify that there is a table "schema_new1.ao_part_table" of "ao" type in "fullbkdb" with same data as table "schema_ao.ao_part_table"

    # THIS SHOULD BE THE LAST TEST
    @backupfire
    Scenario: cleanup for backup feature
        Given the backup files in "/tmp" are deleted
