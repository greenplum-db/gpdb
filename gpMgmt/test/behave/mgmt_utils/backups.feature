@backups
Feature: Validate command line arguments

    Scenario: Entering an invalid argument
        When the user runs "gpcrondump -X"
        Then gpcrondump should print "no such option: -X" error message
        And gpcrondump should return a return code of 2

    Scenario: Valid option combinations for incremental backup
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a --incremental -t public.bkdb -x bkdb"
        Then gpcrondump should print "include table list can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -T public.bkdb -x bkdb"
        And gpcrondump should print "exclude table list can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental --exclude-table-file /tmp/foo -x bkdb"
        And gpcrondump should print "exclude table file can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental --table-file /tmp/foo -x bkdb"
        And gpcrondump should print "include table file can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -s foo -x bkdb"
        And gpcrondump should print "-s option can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -c -x bkdb"
        And gpcrondump should print "-c option can not be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -f 10 -x bkdb"
        And gpcrondump should print "-f option cannot be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump -a --incremental -o -x bkdb"
        And gpcrondump should print "-o option cannot be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        And the user runs "gpcrondump --incremental"
        And gpcrondump should print "Must supply -x <database name> with incremental option" to stdout
        And gpcrondump should return a return code of 2
        When the user runs "gpcrondump --list-backup-files"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Must supply -K option when listing backup files" to stdout
        When the user runs "gpcrondump -K 20140101010101 -x bkdb -x fulldb -a"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "multi-database backup is not supported with -K option" to stdout
        When the user runs "gpcrondump -a --incremental -x bkdb -x fulldb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "multi-database backup is not supported with incremental backup" to stdout
        When the user runs "gpcrondump -a --list-backup-files -K 20130101010101 --ddboost -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "list backup files not supported with ddboost option" to stdout
        When the user runs "gpcrondump -a --list-filter-tables -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "list filter tables option requires --prefix and --incremental" to stdout
        When the user runs "gpcrondump -a --list-filter-tables -x bkdb --incremental"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "list filter tables option requires --prefix and --incremental" to stdout
        When the user runs "gpdbrestore --help"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "noanalyze" to stdout

    Scenario: Valid option combinations for schema level backup
        Given the backup test is initialized with no backup files
        And schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -s schema_heap -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with -s option" to stdout
        When the user runs "gpcrondump -a -S schema_heap -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with -S option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -t schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with --exclude-schema-file option" to stdout
        When the user runs "gpcrondump -a -s schema_heap -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with -s option" to stdout
        When the user runs "gpcrondump -a -S schema_heap -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with -S option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-t and -T can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -T schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        When the user runs "gpcrondump -a -s schema_heap --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with -s option" to stdout
        When the user runs "gpcrondump -a -S schema_heap --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with -S option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with --exclude-schema-file option" to stdout
        When the user runs "gpcrondump -a -s schema_heap --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with -s option" to stdout
        When the user runs "gpcrondump -a -S schema_heap --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with -S option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --exclude-table-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--table-file and --exclude-table-file can not be selected with --exclude-schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo --schema-file /tmp/foo -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--exclude-schema-file can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -s schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-s can not be selected with --exclude-schema-file option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -s schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-s can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a --exclude-schema-file /tmp/foo -S schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-S can not be selected with --exclude-schema-file option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo -S schema_heap.heap_table -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-S can not be selected with --schema-file option" to stdout
        When the user runs "gpcrondump -a -s schema_heap -S schema_heap -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-s can not be selected with -S option" to stdout
        When the user runs "gpcrondump -a --schema-file /tmp/foo --exclude-schema-file /tmp/bar -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "--exclude-schema-file can not be selected with --schema-file option" to stdout
        And the user runs "psql -c 'drop schema schema_heap cascade;' bkdb"

    Scenario: Negative test for Incremental Backup
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "No full backup found for incremental" to stdout

    Scenario: Negative test for Incremental Backup - Incremental with -u after a full backup to default directory
        Given the backup test is initialized with no backup files
        And the directory "/tmp/backup_test" exists
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp/backup_test"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "No full backup found for incremental" to stdout
        And the directory "/tmp/backup_test" does not exist

    Scenario: Negative test for Incremental Backup - Incremental to default directory after a full backup with -u option
        Given the backup test is initialized with no backup files
        And the directory "/tmp/full_test" exists
        And the directory "/tmp/incr_test" exists
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp/full_test"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb -u /tmp/incr_test"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "No full backup found for incremental" to stdout
        And the directory "/tmp/full_test" does not exist
        And the directory "/tmp/incr_test" does not exist

    Scenario: Negative test for Incremental Backup - Incremental after a full backup of different database
        Given the backup test is initialized with no backup files
        And database "bkdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the user runs "gpcrondump -a --incremental -x bkdb2"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "No full backup found for incremental" to stdout

    Scenario: Negative test for missing state file
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "ao" state file under " " is saved for the "full" timestamp
        And the saved state file is deleted
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "state file does not exist" to stdout

    Scenario: Negative test for invalid state file format
        Given the backup test is initialized with no backup files
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And the state files are generated under " " for stored "full" timestamp
        And the "ao" state file under " " is saved for the "full" timestamp
        And the saved state file is corrupted
        When the user runs "gpcrondump -a --incremental -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Invalid state file format" to stdout

    @nbupartIII
    @ddpartIII
    Scenario: Increments File Check With Complicated Scenario
        Given the backup test is initialized with no backup files
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

    @nbupartIII
    Scenario: Incremental File Check With Different Directory
        Given the backup test is initialized with no backup files
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

    @nbupartIII
    Scenario: gpcrondump -b with Full and Incremental backup
        Given the backup test is initialized with no backup files
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -b"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Bypassing disk space check" to stdout
        And gpcrondump should not print "Validating disk space" to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -b --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Bypassing disk space checks for incremental backup" to stdout
        And gpcrondump should not print "Validating disk space" to stdout

    Scenario: Output info gpcrondump
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Dump type                                = Full" to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Dump type                                = Incremental" to stdout

    @nbupartIII
    @ddpartIII
    Scenario: gpcrondump -G with Full timestamp
        Given the backup test is initialized with no backup files
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

    @valgrind
    Scenario: Valgrind test of gp_dump incremental
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental bkdb" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump incremental with table file
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --incremental bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump full with table file
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental with table file
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_-1_1_ --gp-d /tmp --pre-data-schema-only bkdb --incremental --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent full with table file
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.ao_table" are in dirty hack file "/tmp/dirty_hack.txt"
        And partition "1" of partition tables "ao_part_table" in "bkdb" in schema "public" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_-1_1_ --gp-d /tmp --pre-data-schema-only bkdb --table-file=/tmp/dirty_hack.txt" and options " "

    @valgrind
    Scenario: Valgrind test of gp_dump_agent incremental
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -x bkdb -a"
        Then gpcrondump should return a return code of 0
        And the user runs valgrind with "gp_dump_agent --gp-k 11111111111111_-1_1_ --gp-d /tmp --pre-data-schema-only bkdb --incremental" and options " "

    @nbupartIII
    Scenario: Test gpcrondump dump deletion only (-o option)
        Given the backup test is initialized with no backup files
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

    @nbupartIII
    Scenario: Negative test gpcrondump dump deletion only (-o option)
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And the user runs "gpcrondump -a -x bkdb"
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        When the user runs "gpcrondump -o"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "No old backup sets to remove" to stdout
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o --cleanup-date=20130100"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Timestamp dump 20130100 does not exist." to stdout
        When the user runs "gpcrondump -o --cleanup-total=2"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Unable to delete 2 backups.  Only have 1 backups." to stdout
        When the user runs "gpcrondump --cleanup-date=20130100"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Must supply -c or -o with --cleanup-date option" to stdout
        When the user runs "gpcrondump --cleanup-total=2"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Must supply -c or -o with --cleanup-total option" to stdout

    @nbupartIII
    Scenario: Test gpcrondump dump deletion (-c option)
        Given the backup test is initialized with no backup files
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

    @nbupartIII
    @ddpartIII
    Scenario: Verify the gpcrondump -g option works with full backup
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    @nbupartIII
    @ddpartIII
    Scenario: Verify the gpcrondump -g option works with incremental backup
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb -g --incremental"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    @nbupartIII
    @ddpartIII
    Scenario: Verify the gpcrondump history table works by default with full and incremental backups
        Given the backup test is initialized with no backup files
        And schema "testschema" exists in "bkdb"
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

    @nbupartIII
    @ddpartIII
    Scenario: Verify the gpcrondump -H option should not create history table
        Given the backup test is initialized with no backup files
        And schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -H"
        Then gpcrondump should return a return code of 0
        Then verify that there is no table "public.gpcrondump_history" in "bkdb"

    Scenario: Verify the gpcrondump -H and -h option can not be specified together with DDBoost
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -H -h --ddboost"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "-H option cannot be selected with -h option" to stdout

    @nbupartIII
    @ddpartIII
    Scenario: Config files have the same timestamp as the backup set
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And a backup file of tables "public.heap_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And config files should be backed up on all segments

    @nbupartIII
    @ddpartIII
    Scenario Outline: Incremental Backup With column-inserts, inserts and oids options
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a --incremental -x bkdb <options>"
        Then gpcrondump should print "--inserts, --column-inserts, --oids cannot be selected with incremental backup" to stdout
        And gpcrondump should return a return code of 2
        Examples:
        | options         |
        | --inserts        |
        | --oids          |
        | --column-inserts |

    Scenario: Full and Incremental Backup with -g option using named pipes
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" with compression "None" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --list-backup-files -K 20130101010101 -g"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Added the list of pipe names to the file" to stdout
        And gpcrondump should print "Added the list of file names to the file" to stdout
        And gpcrondump should print "Successfully listed the names of backup files and pipes" to stdout
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
        When the user runs "gpcrondump -a -x bkdb -K 20130101020102 -g --incremental"
        Then gpcrondump should return a return code of 0

    @nbupartIII
    @ddpartIII
    Scenario: Full Backup with option -t and non-existant table
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -t cool.dude -t public.heap_table"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "does not exist in" to stdout

    Scenario: Full Backup and Restore using gp_dump with no-lock
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data and 1000000 rows
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And a backup file of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs the "gp_dump --gp-s=p --gp-c --no-lock bkdb" in a worker pool "w1"
        And this test sleeps for "2" seconds
        And the worker pool "w1" is cleaned up
        Then gp_dump should return a return code of 0

    Scenario: Gpcrondump with --email-file option
        Given the backup test is initialized with no backup files
        And database "testdb1" is dropped and recreated
        And database "testdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "testdb1" with data
        And there is a "heap" table "public.heap_table" in "testdb2" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "test/behave/mgmt_utils/steps/data/test_email_details.yaml" stores email details is in proper format
        When the user runs "gpcrondump -a -x testdb1 -x testdb2 --email-file test/behave/mgmt_utils/steps/data/test_email_details.yaml --verbose"
        Then gpcrondump should return a return code of 0
        And verify that emails are sent to the given contacts with appropriate messages after backup of "testdb1,testdb2"
        And the mail_contacts file does not exist

    Scenario: Gpcrondump without mail_contacts
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the mail_contacts file does not exist
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "Unable to send dump email notification" to stdout

    Scenario: Negative case for gpcrondump with --email-file option
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the mail_contacts file does not exist
        And the mail_contacts file exists
        And the yaml file "test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml" stores email details is not in proper format
        When the user runs "gpcrondump -a -x bkdb --email-file test/behave/mgmt_utils/steps/data/test_email_details_wrong_format.yaml --verbose"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "file is not formatted properly" to stdout
        And the mail_contacts file does not exist

    Scenario: If file contains double quoted table and schema name then gpcrondump should error out finding table does not exist
        Given the backup test is initialized with no backup files
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_database.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_schema.sql template1"
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/create_special_table.sql template1"
        # --table-file=<filename> option
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --table-file test/behave/mgmt_utils/steps/data/special_chars/table-file-double-quote.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "does not exist" to stdout
        # --exclude-table-file=<filename> option
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --exclude-table-file test/behave/mgmt_utils/steps/data/special_chars/table-file-double-quote.txt"
        Then gpcrondump should return a return code of 0
        And gpcrondump should print "does not exist" to stdout
        And gpcrondump should print "All exclude table names have been removed due to issues" to stdout
        # --schema-file
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --schema-file test/behave/mgmt_utils/steps/data/special_chars/schema-file-double-quote.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "does not exist" to stdout
        # --exclude-schema-file
        When the user runs "gpcrondump -a -x "$SP_CHAR_DB" --exclude-schema-file test/behave/mgmt_utils/steps/data/special_chars/schema-file-double-quote.txt"
        Then gpcrondump should return a return code of 0
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/drop_special_database.sql template1"

    Scenario: Absolute path should be provided with -u option for gpcrondump
        Given the backup test is initialized with no backup files
        And there is a "heap" table "heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u foo/db"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "is not an absolute path" to stdout

    Scenario: Tables with same name but different partitioning should not pollute one another's dump during backup
        Given the backup test is initialized with no backup files
        And schema "withpartition" exists in "bkdb"
        And schema "withoutpartition" exists in "bkdb"
        And schema "aaa" exists in "bkdb"
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

    Scenario: Exclude schema (-S) should not dump pg_temp schemas
        Given the backup test is initialized with no backup files
        And the user runs the command "psql bkdb -f 'test/behave/mgmt_utils/steps/data/gpcrondump/create_temp_schema_in_transaction.sql'" in the background without sleep
        When the user runs "gpcrondump -a -S good_schema -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Then read pid from file "test/behave/mgmt_utils/steps/data/gpcrondump/pid_leak" and kill the process
        And the temporary file "test/behave/mgmt_utils/steps/data/gpcrondump/pid_leak" is removed
        And waiting "2" seconds
        And verify that the "dump" file in " " dir does not contain "pg_temp"
        And the user runs command "dropdb bkdb"

    Scenario: Funny characters in the table name or schema name for gpcrondump
        Given the backup test is initialized with no backup files
        And the database "testdb" does not exist
        And database "testdb" exists
        And the user runs "psql -f test/behave/mgmt_utils/steps/data/special_chars/funny_char.sql testdb"
        When the user runs "gpcrondump -a -x testdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb -t Schema\\t,1.Table\\n\!1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb -T Schema\\t,1.Table\\n\!1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb -s Schema\\t,1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb -S Schema\\t,1"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb --table-file test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb --exclude-table-file test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb --schema-file test/behave/mgmt_utils/steps/data/special_chars/funny_char_schema.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout
        When the user runs "gpcrondump -a -x testdb --exclude-schema-file test/behave/mgmt_utils/steps/data/special_chars/funny_char_schema.txt"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "Name has an invalid character "\\t" "\\n" "!" "," "."" to stdout

    Scenario: Incremental backup with no-privileges
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And the user runs "psql -c 'DROP ROLE IF EXISTS temprole; CREATE ROLE temprole; GRANT ALL ON public.heap_table TO temprole;' bkdb"
        Then psql should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb --no-privileges"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Then verify the metadata dump file does not contain "GRANT"
        Then verify the metadata dump file does not contain "REVOKE"
        And the user runs "psql -c 'DROP ROLE temprole;' bkdb"

    Scenario: Incremental backup with use-set-session-authorization
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --use-set-session-authorization"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        Then verify the metadata dump file does contain "SESSION AUTHORIZATION"
        Then verify the metadata dump file does not contain "ALTER TABLE * OWNER TO"

    @nbupartIII
    Scenario: gpcrondump with -G and -g
        Given the backup test is initialized with no backup files
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -G -g"
        And the timestamp from gpcrondump is stored
        Then gpcrondump should return a return code of 0
        And "global" file should be created under " "
        And config files should be backed up on all segments

    Scenario: gpcrondump with -k (vacuum after backup)
        Given the backup test is initialized with no backup files
        And the user runs "psql -c 'vacuum;' bkdb"
        Then store the vacuum timestamp for verification in database "bkdb"
        When the user runs "gpcrondump -a -x bkdb -k"
        Then gpcrondump should return a return code of 0
        Then gpcrondump should print "Commencing post-dump vacuum" to stdout
        And gpcrondump should print "Vacuum of bkdb completed without error" to stdout
        And verify that vacuum has been run in database "bkdb"

    Scenario: gpcrondump with -f when not enough disk space
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -f 100"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "segment\(s\) failed disk space checks" to stdout

    Scenario: gpcrondump with -B with 1 process
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -B 1 -v"
        Then gpcrondump should return a return code of 0
        And gpcrondump should not print "\[worker1\] got a halt cmd" to stdout

    Scenario: gpcrondump with -d with invalid master data directory
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -d /tmp"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "gpcrondump failed.* No such file or directory" to stdout

    Scenario: gpcrondump with -l to log to /tmp directory
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x no_exist -l /tmp"
        Then gpcrondump should return a return code of 2
        And the "gpcrondump" log file should exist under "/tmp"

    @ddpartIII
    @ddonly
    Scenario: gpcrondump with -c on Data Domain
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -K 20150101010101"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb -c"
        Then gpcrondump should return a return code of 0
        Then no dump files should be present on the data domain server

    @ddpartIII
    @ddonly
    Scenario: gpcrondump with -o on Data Domain
        Given the backup test is initialized with no backup files
        When the user runs "gpcrondump -a -x bkdb -K 20150101010101"
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb -o"
        Then gpcrondump should return a return code of 0
        Then no dump files should be present on the data domain server

    @nbupartIII
    @ddpartIII
    Scenario: Out of Sync timestamp
        Given the backup test is initialized with no backup files
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        And there is a fake timestamp for "30160101010101"
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 2
        And gpcrondump should print "There is a future dated backup on the system preventing new backups" to stdout

    @nbupartIII
    @ddpartIII
    Scenario: The test suite is torn down
        Given the backup test is initialized with no backup files

    @ddonly
    @ddboostsetup
    Scenario: Cleanup DDBoost dump directories
        Given the DDBoost dump directory is deleted
