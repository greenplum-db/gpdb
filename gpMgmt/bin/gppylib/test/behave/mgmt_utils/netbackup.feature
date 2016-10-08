@netbackup
Feature: NetBackup Integration with GPDB

    @nbusetup76
    Scenario: Setup to load NBU libraries
        Given the NetBackup "7.6" libraries are loaded

    @nbusetup75
    Scenario: Setup to load NBU libraries
        Given the NetBackup "7.5" libraries are loaded

    @nbusetup71
    Scenario: Setup to load NBU libraries
        Given the NetBackup "7.1" libraries are loaded

    @nbupartI
    Scenario: Valgrind test of gp_dump with netbackup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c bkdb" and options " " and suppressions file "netbackup_suppressions.txt" using netbackup

    @nbupartI
    Scenario: Valgrind test of gp_dump with netbackup with table filter
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        And the tables "public.heap_table" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs valgrind with "gp_dump --gp-d=db_dumps --gp-s=p --gp-c --table-file=/tmp/dirty_hack.txt bkdb" and options " " and suppressions file "netbackup_suppressions.txt" using netbackup

    @nbupartI
    Scenario: Valgrind test of gp_restore with netbackup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs valgrind with "gp_restore" and options "-i --gp-i --gp-l=p -d bkdb --gp-c" and suppressions file "netbackup_suppressions.txt" using netbackup

    @nbupartI
    Scenario: Valgrind test of gp_restore_agent with netbackup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs valgrind with "gp_restore_agent" and options "--gp-c /bin/gunzip -s --post-data-schema-only --target-dbid 1 -d bkdb" and suppressions file "netbackup_suppressions.txt" using netbackup

    @nbupartI
    Scenario: Valgrind test of gp_dump_agent with table file
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs valgrind with "./gppylib/test/behave/mgmt_utils/steps/scripts/valgrind_nbu_test_1.sh" and options " " and suppressions file "netbackup_suppressions.txt" using netbackup

    @nbupartI
    Scenario: Valgrind test of gp_bsa_dump_agent and gp_bsa_restore_agent
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And the tables "public.heap_table" are in dirty hack file "/tmp/dirty_hack.txt"
        When the user runs backup command "cat /tmp/dirty_hack.txt | valgrind --suppressions=gppylib/test/behave/mgmt_utils/steps/netbackup_suppressions.txt gp_bsa_dump_agent --netbackup-filename /tmp/dirty_hack.txt" using netbackup
        And the user runs restore command "valgrind --suppressions=gppylib/test/behave/mgmt_utils/steps/netbackup_suppressions.txt gp_bsa_restore_agent --netbackup-filename /tmp/dirty_hack.txt" using netbackup

    @nbusmoke
    @nbupartI
    Scenario: gpdbrestore with non-supported options
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        When the user runs "gpdbrestore -e -s bkdb -a" using netbackup
        Then gpdbrestore should print -s is not supported with NetBackup to stdout
        And gpdbrestore should return a return code of 2
        When the user runs gpdbrestore with the stored timestamp and options "-b" using netbackup
        Then gpdbrestore should print -b is not supported with NetBackup to stdout
        And gpdbrestore should return a return code of 2
        When the user runs gpdbrestore with the stored timestamp and options "-L" using netbackup
        Then gpdbrestore should print -L is not supported with NetBackup to stdout
        And gpdbrestore should return a return code of 2
        When the user runs gpdbrestore with "-R" option in path "/tmp" using netbackup
        Then gpdbrestore should print -R is not supported for restore with NetBackup to stdout
        And gpdbrestore should return a return code of 2

    @nbupartI
    Scenario: Full Backup and Restore
        Given the test is initialized
        And the netbackup params have been parsed
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
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "68" tables in "bkdb" is validated after restore

    @nbusmoke
    @nbupartI
    Scenario: Full Backup and Restore with --netbackup-block-size option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options " --netbackup-block-size 1024 " using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbusmoke
    @nbupartI
    Scenario: Full Backup and Restore with --netbackup-keyword option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-keyword foo" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Full Backup and Restore with --netbackup-block-size and --netbackup-keyword options
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096 --netbackup-keyword foo" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options " --netbackup-block-size 4096 " using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Test netbackup parameter lengths for full backup and restore (<=127 and >127)
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-keyword aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127" using netbackup with long params
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb --netbackup-service-host aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128" using netbackup with long params
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Netbackup Service Hostname \(aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128\) exceeds the maximum length of 127 characters to stdout
        When the user runs "gpcrondump -a -x bkdb --netbackup-policy aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128" using netbackup with long params
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Netbackup Policy Name \(aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128\) exceeds the maximum length of 127 characters to stdout
        When the user runs "gpcrondump -a -x bkdb --netbackup-schedule aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128" using netbackup with long params
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Netbackup Schedule Name \(aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128\) exceeds the maximum length of 127 characters to stdout
        When the user runs "gpcrondump -a -x bkdb --netbackup-keyword aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128" using netbackup with long params
        Then gpcrondump should return a return code of 2
        And gpcrondump should print Netbackup Keyword \(aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128\) exceeds the maximum length of 127 characters to stdout
        When the user runs gpdbrestore with the stored timestamp and options "--netbackup-service-host netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127"
        Then gpdbrestore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--netbackup-service-host netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print Netbackup Service Hostname \(netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa128\) exceeds the maximum length of 127 characters to stdout

    @nbusmoke
    @nbupartI
    Scenario: Full Backup and Restore with -u option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options " -u /tmp " using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbusmoke
    @nbupartI
    Scenario: Full Backup with option -t and Restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -t public.co_table -t public.ao_table" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user truncates "public.co_table" tables in "bkdb"
        And the user truncates "public.ao_table" tables in "bkdb"
        And the user runs gpdbrestore with the stored timestamp and options " " without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb" is validated after restore

    @nbusmoke
    @nbupartI
    Scenario: Full Backup with option -T and Restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -T public.heap_table" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that there is no table "public.heap_table" in "bkdb"

    @nbupartI
    Scenario: Full Backup with option -s and Restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "schema_heap, schema_ao" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" table "schema_ao.ao_table" in "bkdb" with data
        And there is a backupfile of tables "schema_heap.heap_table, schema_ao.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -s schema_heap --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And verify that there is no table "schema_ao.ao_table" in "bkdb"

    @nbupartI
    Scenario: Full Backup with option -t and Restore after TRUNCATE on filtered tables
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb -t public.heap_table --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user truncates "public.heap_table" tables in "bkdb"
        And the user runs gpdbrestore with the stored timestamp and options "--netbackup-block-size 2048" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data

    @nbupartI
    Scenario: Full Backup with option --exclude-table-file and Restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a backupfile of tables "public.co_table" in "bkdb" exists for validation
        And there is a file "exclude_file" with tables "public.ao_table"
        When the user runs "gpcrondump -a -x bkdb --exclude-table-file exclude_file --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "co" table "public.co_table" in "bkdb" with data
        And verify that there is no table "public.ao_table" in "bkdb"

    @nbusmoke
    @nbupartI
    Scenario: Full Backup with option --table-file and Restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        And there is a file "include_file" with tables "public.ao_table|public.heap_table"
        When the user runs "gpcrondump -a -x bkdb --table-file include_file --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data
        And verify that there is no table "public.co_table" in "bkdb"

    @nbupartI
    Scenario: Schema only restore of full backup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the table names in "bkdb" is stored
        And the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And tables names should be identical to stored table names in "bkdb" except "public.gpcrondump_history"

    @nbusmoke
    @nbupartI
    Scenario: Full Backup and Restore without compression
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -z --netbackup-block-size 1024 --netbackup-keyword foo" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Verify the gpcrondump -o option is not broken due to NetBackup changes
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -o" using netbackup
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist

    @nbupartI
    Scenario: Verify the gpcrondump -c option is not broken due to NetBackup changes
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And older backup directories "20130101" exists
        When the user runs "gpcrondump -c -x bkdb -a" using netbackup
        Then gpcrondump should return a return code of 0
        And the dump directories "20130101" should not exist
        And the dump directory for the stored timestamp should exist

    @nbupartI
    Scenario: Verify the gpcrondump -g option is not broken due to NetBackup changes
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -g" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And config files should be backed up on all segments

    @nbupartI
    Scenario: Verify the gpcrondump -h option works with full backup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -h" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that there is a "heap" table "gpcrondump_history" in "bkdb"
        And verify that the table "gpcrondump_history" in "bkdb" has dump info for the stored timestamp

    @nbupartI
    Scenario: Verify the gpcrondump -H option should not create history table
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -H" using netbackup
        Then gpcrondump should return a return code of 0
        Then verify that there is no table "public.gpcrondump_history" in "bkdb"

    @nbupartI
    Scenario: Verify the gpcrondump -H and -h option can not be specified together
        Given the test is initialized
        And the netbackup params have been parsed
        When the user runs "gpcrondump -a -x bkdb -H -h" using netbackup
        Then gpcrondump should return a return code of 2
        And gpcrondump should print -H option cannot be selected with -h option to stdout

    @nbupartI
    Scenario: gpdbrestore -u option with full backup timestamp
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When there are no backup files
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --verbose" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @nbupartI
    Scenario: gpcrondump -x with multiple databases
        Given the test is initialized
        And the netbackup params have been parsed
        And database "bkdb2" is dropped and recreated
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a "co" table "public.co_table" in "bkdb2" with data
        And there is a list to store the backup timestamps
        When the user runs "gpcrondump -a -x bkdb -x bkdb2 --netbackup-block-size 2048" using netbackup
        And the timestamp for database dumps "bkdb, bkdb2" are stored
        Then gpcrondump should return a return code of 0
        And all the data from "bkdb" is saved for verification
        And all the data from "bkdb2" is saved for verification
        When the user runs gpdbrestore for the database "bkdb" with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        When the user runs gpdbrestore for the database "bkdb2" with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the data of "2" tables in "bkdb2" is validated after restore
        And the dump timestamp for "bkdb, bkdb2" are different
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the tuple count of all appendonly tables are consistent in "bkdb2"

    @nbupartI
    Scenario: gpdbrestore with --table-file option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--table-file /tmp/table_file_foo" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @nbupartI
    Scenario: Multiple full backup and restore from first full backup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the numbers "1" to "100" are inserted into "ao_table" tables in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @nbupartI
    Scenario: gpdbrestore with -T option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And database "bkdb" is dropped and recreated
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_table -a --netbackup-block-size 2048" without -e option using netbackup
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data

    @nbupartI
    Scenario: gpdbrestore list_backup option with full timestamp
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options "--netbackup-block-size 2048" using netbackup
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print --list-backup is not supported for restore with full timestamps to stdout

    @nbupartI
    Scenario: gpdbrestore list_backup option with incremental backup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "public.co_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options " " using netbackup
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout
        Then "plan" file is removed under " "
        When the user runs gpdbrestore with the stored timestamp to print the backup set with options "-a" using netbackup
        Then gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And verify that the list of stored timestamps is printed to stdout

    @nbupartI
    Scenario: User specified timestamp key for dump
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -K 20130101010101 --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that report file with prefix " " under subdir " " has been backed up using netbackup
        And verify that cdatabase file with prefix " " under subdir " " has been backed up using netbackup
        And verify that state file with prefix " " under subdir " " has been backed up using netbackup
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "2" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    ######## Add test for --list-netbackup-files option
    ######## Scenario: Full Backup with --list-netbackup-files and --prefix options
    ######## Add scenario for invalid netbackup params

    ######## Restore with NetBackup using invalid timestamp - error message can be improved
    @nbupartI
    Scenario: User specified invalid timestamp key for restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -K 20130101010101" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpdbrestore -t 20130101010102 -a -e" using netbackup
        Then gpdbrestore should return a return code of 2
        And gpcrondump should print Failed to get the NetBackup BSA restore object to perform Restore to stdout

    @nbusmoke
    @nbupartI
    Scenario: Full Backup and Restore with --prefix option
        Given the test is initialized
        And the netbackup params have been parsed
        And the prefix "foo" is stored
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --netbackup-block-size 2048" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that report file with prefix "foo" under subdir " " has been backed up using netbackup
        And verify that cdatabase file with prefix "foo" under subdir " " has been backed up using netbackup
        And verify that state file with prefix "foo" under subdir " " has been backed up using netbackup
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data

    @nbupartI
    Scenario: Full Backup and Restore with -u and --prefix option
        Given the test is initialized
        And the netbackup params have been parsed
        And the prefix "foo" is stored
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_table" in "bkdb" exists for validation
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -u /tmp --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And verify that report file with prefix "foo" under subdir "/tmp" has been backed up using netbackup
        And verify that cdatabase file with prefix "foo" under subdir "/tmp" has been backed up using netbackup
        And verify that state file with prefix "foo" under subdir "/tmp" has been backed up using netbackup
        When the user runs gpdbrestore with the stored timestamp and options "-u /tmp --prefix=foo --netbackup-block-size 2048" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data

    @nbupartI
    Scenario: Restore database without prefix for a dump with prefix
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print No object matched the specified predicate to stdout

    @nbupartI
    Scenario: Full Backup and Restore of external and ao table
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table_ex" in "bkdb" with data
        And there is an external table "ext_tab" in "bkdb" with data for file "/tmp/ext_tab"
        And the user runs "psql -c 'CREATE LANGUAGE plpythonu' bkdb"
        And there is a function "pymax" in "bkdb"
        And the user runs "psql -c 'CREATE VIEW vista AS SELECT text 'Hello World' AS hello' bkdb"
        And the user runs "psql -c 'COMMENT ON TABLE public.ao_table IS 'Hello World' bkdb"
        And the user runs "psql -c 'CREATE ROLE foo_user' bkdb"
        And the user runs "psql -c 'GRANT INSERT ON TABLE public.ao_table TO foo_user' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.ao_table ADD CONSTRAINT null_check CHECK (column1 <> NULL);' bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And database "bkdb" is dropped and recreated
        And there is a file "restore_file" with tables "public.ao_table|public.ext_tab"
        When the user runs gpdbrestore with the stored timestamp and options "--table-file restore_file --netbackup-block-size 2048" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data
        And verify that there is a "external" table "public.ext_tab" in "bkdb" with data
        And verify that there is a constraint "null_check" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"
        And verify that there is no table "public.co_table_ex" in "bkdb"
        And verify that there is no view "vista" in "bkdb"
        And verify that there is no procedural language "plpythonu" in "bkdb"
        And the user runs "psql -c '\z' bkdb"
        And psql should print foo_user=a/ to stdout
        And the user runs "psql -c '\df' bkdb"
        And psql should not print pymax to stdout
        And verify that the data of "2" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Full Backup and Restore filtering tables with post data objects
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table_ex" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table_ex" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a trigger "heap_trigger" on table "public.heap_table" in "bkdb" based on function "heap_trigger_func"
        And there is a trigger "heap_ex_trigger" on table "public.heap_table_ex" in "bkdb" based on function "heap_ex_trigger_func"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table_ex ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table_ex(column1, column2, column3);' bkdb"
        And the user runs "psql -c """create rule heap_co_rule as on insert to heap_table where column1=100 do instead insert into co_table_ex values(27, 'restore', '2013-08-19');""" bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And there is a file "restore_file" with tables "public.ao_table|public.ao_index_table|public.heap_table"
        When table "public.ao_index_table" is dropped in "bkdb"
        And table "public.ao_table" is dropped in "bkdb"
        And table "public.heap_table" is dropped in "bkdb"
        And the index "bitmap_co_index" in "bkdb" is dropped
        And the trigger "heap_ex_trigger" on table "public.heap_table_ex" in "bkdb" is dropped
        When the user runs gpdbrestore with the stored timestamp and options "--table-file restore_file --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table_ex" in "bkdb" with data
        And verify that there is a "co" table "public.co_table_ex" in "bkdb" with data
        And verify that there is a "co" table "public.co_index_table" in "bkdb" with data
        And the user runs "psql -c '\d public.ao_index_table' bkdb"
        And psql should print \"bitmap_ao_index\" bitmap \(column3\) to stdout
        And psql should print \"btree_ao_index\" btree \(column1\) to stdout
        And the user runs "psql -c '\d public.heap_table' bkdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_trigger AFTER INSERT OR DELETE OR UPDATE ON heap_table FOR EACH STATEMENT EXECUTE PROCEDURE heap_trigger_func\(\) to stdout
        And psql should print heap_co_rule AS\n.*ON INSERT TO heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO co_table_ex \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table_ex\(column1, column2, column3\) to stdout
        And the user runs "psql -c '\d public.co_index_table' bkdb"
        And psql should not print bitmap_co_index to stdout
        And the user runs "psql -c '\d public.heap_table_ex' bkdb"
        And psql should not print heap_ex_trigger to stdout
        And verify that the data of "7" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Full Backup and Restore dropped database filtering tables with post data objects
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table_ex" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a trigger "heap_trigger" on table "public.heap_table" in "bkdb" based on function "heap_trigger_func"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_index_table(column1, column2, column3);' bkdb"
        And the user runs "psql -c """create rule heap_ao_rule as on insert to heap_table where column1=100 do instead insert into ao_table values(27, 'restore', '2013-08-19');""" bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And there is a file "restore_file" with tables "public.ao_table|public.ao_index_table|public.heap_table|public.heap_index_table"
        And database "bkdb" is dropped and recreated
        And there is a trigger function "heap_trigger_func" on table "public.heap_table" in "bkdb"
        When the user runs gpdbrestore with the stored timestamp and options "--table-file restore_file --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And verify that there is no table "public.co_table_ex" in "bkdb"
        And verify that there is no table "public.co_index_table" in "bkdb"
        And the user runs "psql -c '\d public.ao_index_table' bkdb"
        And psql should print \"bitmap_ao_index\" bitmap \(column3\) to stdout
        And psql should print \"btree_ao_index\" btree \(column1\) to stdout
        And the user runs "psql -c '\d public.heap_table' bkdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_trigger AFTER INSERT OR DELETE OR UPDATE ON heap_table FOR EACH STATEMENT EXECUTE PROCEDURE heap_trigger_func\(\) to stdout
        And psql should print heap_ao_rule AS\n.*ON INSERT TO heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO ao_table \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_index_table\(column1, column2, column3\) to stdout
        And verify that the data of "4" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Full backup and restore for table names with multibyte (chinese) characters
        Given the test is initialized
        And the netbackup params have been parsed
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_table_name.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 1024" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char.sql bkdb"
        Then psql should print 1000 to stdout

    @nbupartI
    Scenario: Full Backup with option -T and Restore with exactly 1000 partitions
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" table "public.ao_part_table" in "bkdb" having "1000" partitions
        When the user runs "gpcrondump -a -x bkdb -T public.ao_part_table --netbackup-block-size 1024" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "1" tables in "bkdb" is validated after restore

    @nbupartI
    Scenario: Single table restore with shared sequence across multiple tables
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a sequence "shared_seq" in "bkdb"
        And the user runs "psql -c """CREATE TABLE table1 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" bkdb"
        And the user runs "psql -c """CREATE TABLE table2 (column1 INT4 DEFAULT nextval('shared_seq') NOT NULL, price NUMERIC);""" bkdb"
        And the user runs "psql -c 'CREATE TABLE table3 (column1 INT4)' bkdb"
        And the user runs "psql -c 'CREATE TABLE table4 (column1 INT4)' bkdb"
        And the user runs "psql -c 'INSERT INTO table1 (price) SELECT * FROM generate_series(1000,1009)' bkdb"
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2000,2009)' bkdb"
        And the user runs "psql -c 'INSERT INTO table3 SELECT * FROM generate_series(3000,3009)' bkdb"
        And the user runs "psql -c 'INSERT INTO table4 SELECT * FROM generate_series(4000,4009)' bkdb"
        When the user runs "gpcrondump -x bkdb -a --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2010,2019)' bkdb"
        When table "public.table1" is dropped in "bkdb"
        And the user runs gpdbrestore with the stored timestamp and options "-T public.table1 --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.table1" in "bkdb" with data
        And the user runs "psql -c 'INSERT INTO table2 (price) SELECT * FROM generate_series(2020,2029)' bkdb"
        And verify that there are no duplicates in column "column1" of table "public.table2" in "bkdb"

    @nbupartI
    Scenario: Full backup and restore using gpcrondump with pg_class lock
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" with compression "None" in "bkdb" with data and 1000000 rows
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a backupfile of tables "public.heap_table, public.ao_part_table" in "bkdb" exists for validation
        When the user runs the "gpcrondump -a -x bkdb" in a worker pool "w1" using netbackup
        And this test sleeps for "2" seconds
        And the "gpcrondump" has a lock on the pg_class table in "bkdb"
        And the worker pool "w1" is cleaned up
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data

    @nbupartI
    Scenario: Restore -T for full dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "bkdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "ao_index_table, co_index_table, heap_index_table" in "bkdb" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        When the user runs gpdbrestore with the stored timestamp and options "--table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 1000 to stdout 4 times
        And verify that there is a "ao" table "ao_index_table" in "bkdb" with data
        And verify that there is a "co" table "co_index_table" in "bkdb" with data
        And verify that there is a "heap" table "heap_index_table" in "bkdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

    @nbupartI
    Scenario: Full Backup and Restore with the master dump file missing
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -x bkdb -a --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Backup for given timestamp was performed using NetBackup. Querying NetBackup server to check for the dump file. to stdout

    @nbupartI
    Scenario: Full Backup and Restore with the master dump file missing without compression
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -x bkdb -z -a --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print Backup for given timestamp was performed using NetBackup. Querying NetBackup server to check for the dump file. to stdout

    @nbupartI
    Scenario: Uppercase Database Name Full Backup and Restore using timestamp
        Given the test is initialized
        And the netbackup params have been parsed
        And database "TESTING" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "TESTING" with data
        And there is a "ao" partition table "public.ao_part_table" in "TESTING" with data
        And all the data from "TESTING" is saved for verification
        When the user runs "gpcrondump -x TESTING -a --netbackup-block-size 1024" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And gpdbestore should not print Issue with analyze of to stdout
        And verify that there is a "heap" table "public.heap_table" in "TESTING" with data
        And verify that there is a "ao" table "public.ao_part_table" in "TESTING" with data

    @nbusmoke
    @nbupartI
    Scenario: Verify that metadata files get backed up to NetBackup server
        Given the test is initialized
        And the netbackup params have been parsed
        And there are no report files in "master_data_directory"
        And there are no status files in "segment_data_directory"
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And all the data from "bkdb" is saved for verification
        When the user runs "gpcrondump -x bkdb -a -g -G --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that report file with prefix " " under subdir " " has been backed up using netbackup
        And verify that cdatabase file with prefix " " under subdir " " has been backed up using netbackup
        And verify that state file with prefix " " under subdir " " has been backed up using netbackup
        And verify that config file with prefix " " under subdir " " has been backed up using netbackup
        And verify that global file with prefix " " under subdir " " has been backed up using netbackup
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_part_table" in "bkdb" with data

    @nbusmoke
    @nbupartI
    Scenario: Basic Incremental Backup and Restore with NBU
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And all netbackup objects containing "gp_dump" are deleted
        When the user runs "gpcrondump -a -x bkdb -g -G --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a --incremental -x bkdb -g -G --netbackup-block-size 2048" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options " --netbackup-block-size 4096 " using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_table" in "bkdb" with data

    @nbupartI
    Scenario: Increments File Check With Complicated Scenario
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And database "bkdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb2 --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        When the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under " "
        And verify that the incremental file has all the stored timestamps

    @nbusmoke
    @nbupartI
    Scenario: Incremental File Check With Different Directory
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And database "bkdb2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "bkdb2" with data
        And there is a "ao" table "public.ao_table" in "bkdb2" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        When the user runs "gpcrondump -a -x bkdb" using netbackup
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a --incremental -x bkdb -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under "/tmp"
        When the user runs "gpcrondump -a --incremental -x bkdb -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And "dirty_list" file should be created under "/tmp"
        And verify that the incremental file in "/tmp" has all the stored timestamps

    @nbupartI
    Scenario: Simple Plan File Test
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp is labeled "ts0"
        And the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts1"
        And "dirty_list" file should be created under " "
        And table "public.co_table" is assumed to be in dirty state in "bkdb"
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "bkdb" in schema "public"
        And the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp is labeled "ts2"
        And "dirty_list" file should be created under " "
        And the user runs gpdbrestore with the stored timestamp using netbackup
        And gpdbrestore should return a return code of 0
        Then "plan" file should be created under " "
        And the plan file is validated against "data/plan1"

    @nbupartII
    Scenario: Multiple Incremental backup and restore
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "testschema" exists in "bkdb"
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb" with data
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "heap" partition table "testschema.heap_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the full backup timestamp from gpcrondump is stored
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "bkdb" in schema "testschema"
        And table "testschema.heap_table" is dropped in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that the data of "60" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the plan file is created for the latest timestamp

    @nbusmoke
    @nbupartII
    Scenario: Non compressed incremental backup
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "testschema" exists in "bkdb"
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table" in "bkdb" with data
        And there is a list to store the incremental backup timestamps
        When the user runs "gpcrondump -a -x bkdb -z --netbackup-block-size 4096" using netbackup
        And the full backup timestamp from gpcrondump is stored
        And gpcrondump should return a return code of 0
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental -z --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And partition "1" of partition table "co_part_table" is assumed to be in dirty state in "bkdb" in schema "testschema"
        And table "testschema.heap_table" is dropped in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental -z --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored in a list
        And table "testschema.ao_table" is assumed to be in dirty state in "bkdb"
        And the user runs "gpcrondump -a -x bkdb --incremental -z --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "testschema.heap_table" in "bkdb"
        And verify that the data of "11" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"
        And verify that the plan file is created for the latest timestamp

    @nbupartII
    Scenario: Verify the gpcrondump -h option works with full and incremental backups
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "testschema" exists in "bkdb"
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "co" table "testschema.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -h" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that there is a "heap" table "gpcrondump_history" in "bkdb"
        And verify that the table "gpcrondump_history" in "bkdb" has dump info for the stored timestamp
        When the user runs "gpcrondump -a -x bkdb -h --incremental" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And verify that there is a "heap" table "gpcrondump_history" in "bkdb"
        And verify that the table "gpcrondump_history" in "bkdb" has dump info for the stored timestamp

    @nbupartII
    Scenario: gpdbrestore -u option with incremental backup timestamp
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb -u /tmp --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And table "public.ao_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb -u /tmp --incremental --netbackup-block-size 4096" using netbackup
        And gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        And the user runs gpdbrestore with the stored timestamp and options "-u /tmp --verbose" using netbackup
        And gpdbrestore should return a return code of 0
        Then verify that the data of "3" tables in "bkdb" is validated after restore
        And verify that the tuple count of all appendonly tables are consistent in "bkdb"

    @nbupartII
    Scenario: Incremental backup with -T option
        Given the test is initialized
        And the netbackup params have been parsed
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And table "ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_index_table" using netbackup
        And gpdbrestore should return a return code of 0
        Then verify that there is no table "ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_table" in "bkdb"
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data

    @nbupartII
    Scenario: Dirty File Scale Test
        Given the test is initialized
        And the netbackup params have been parsed
		And there are "240" "heap" tables "public.heap_table" with data in "bkdb"
		And there are "10" "ao" tables "public.ao_table" with data in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_table_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb" using netbackup
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_3, public.ao_table_4, public.ao_table_5, public.ao_table_6" in "bkdb" has no rows
        And verify that tables "public.ao_table_7, public.ao_table_8, public.ao_table_9, public.ao_table_10" in "bkdb" has no rows
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore

    @nbupartII
    Scenario: Dirty File Scale Test for partitions
        Given the test is initialized
        And the netbackup params have been parsed
		And there are "240" "heap" tables "public.heap_table" with data in "bkdb"
        And there is a "ao" partition table "public.ao_table" in "bkdb" with data
        Then data for partition table "ao_table" with partition level "1" is distributed across all segments on "bkdb"
        And verify that partitioned tables "ao_table" in "bkdb" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When table "public.ao_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the subdir from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb" using netbackup
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "--noplan" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_table_1_prt_p1_2_prt_3, public.ao_table_1_prt_p2_2_prt_1" in "bkdb" has no rows
        And verify that tables "public.ao_table_1_prt_p2_2_prt_2, public.ao_table_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that the data of the dirty tables under " " in "bkdb" is validated after restore

    @nbupartII
    Scenario: Incremental table filter gpdbrestore
        Given the test is initialized
        And the netbackup params have been parsed
        And there are "2" "heap" tables "public.heap_table" with data in "bkdb"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb"
        And verify that partitioned tables "ao_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "bkdb" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When table "public.ao_part_table_1_prt_p1_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_part_table_1_prt_p1_2_prt_2" is assumed to be in dirty state in "bkdb"
        And table "public.ao_part_table_1_prt_p2_2_prt_1" is assumed to be in dirty state in "bkdb"
        And table "public.ao_part_table_1_prt_p2_2_prt_2" is assumed to be in dirty state in "bkdb"
        And all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_part_table --netbackup-block-size 4096" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_3" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_2" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p1_2_prt_1" in "bkdb"
        And verify that there is no table "public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb"
        And verify that there is no table "public.heap_table_1" in "bkdb"
        And verify that there is no table "public.heap_table_2" in "bkdb"

    @nbupartII
    Scenario: Incremental table filter gpdbrestore with noplan option
        Given the test is initialized
        And the netbackup params have been parsed
        And there are "2" "heap" tables "public.heap_table" with data in "bkdb"
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table1" in "bkdb" with data
        Then data for partition table "ao_part_table" with partition level "1" is distributed across all segments on "bkdb"
        Then data for partition table "ao_part_table1" with partition level "1" is distributed across all segments on "bkdb"
        And verify that partitioned tables "ao_part_table" in "bkdb" have 6 partitions
        And verify that partitioned tables "ao_part_table1" in "bkdb" have 6 partitions
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When all the data from "bkdb" is saved for verification
        And the user runs "gpcrondump -a --incremental -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And database "bkdb" is dropped and recreated
        When the user runs gp_restore with the the stored timestamp and subdir for metadata only in "bkdb" using netbackup
        Then gp_restore should return a return code of 0
        When the user runs gpdbrestore with the stored timestamp and options "-T public.ao_part_table -T public.heap_table_1 --noplan" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_3, public.ao_part_table_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_2, public.ao_part_table_1_prt_p2_2_prt_2" in "bkdb" has no rows
        And verify that tables "public.ao_part_table_1_prt_p1_2_prt_1, public.ao_part_table_1_prt_p2_2_prt_1" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_3, public.ao_part_table1_1_prt_p2_2_prt_3" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_2, public.ao_part_table1_1_prt_p2_2_prt_2" in "bkdb" has no rows
        And verify that tables "public.ao_part_table1_1_prt_p1_2_prt_1, public.ao_part_table1_1_prt_p2_2_prt_1" in "bkdb" has no rows
        And verify that there is a "heap" table "public.heap_table_1" in "bkdb" with data

    @nbupartII
    Scenario: Multiple full and incrementals with and without prefix
        Given the test is initialized
        And the netbackup params have been parsed
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental --prefix=foo --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --netbackup-block-size 4096" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "12" tables in "bkdb" is validated after restore

    @nbupartII
    Scenario: Incremental Backup and Restore with --prefix and -u options
        Given the test is initialized
        And the netbackup params have been parsed
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental --prefix=foo -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo -u /tmp --netbackup-block-size 4096" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that the data of "12" tables in "bkdb" is validated after restore

    @nbupartII
    Scenario: Incremental Backup with table filter on Full Backup should update the tracker files
        Given the test is initialized
        And the netbackup params have been parsed
        And the prefix "foo" is stored
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "public.heap_table" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And there is a "ao" partition table "public.ao_part_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb --prefix=foo -t public.ao_index_table -t public.heap_table --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And "_filter" file should be created under " "
        And verify that the "filter" file in " " dir contains "public.ao_index_table"
        And verify that the "filter" file in " " dir contains "public.heap_table"
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --prefix=foo --incremental --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And "public.ao_index_table" is marked as dirty in dirty_list file
        And "public.heap_table" is marked as dirty in dirty_list file
        And verify that there is no "public.heap_index_table" in the "dirty_list" file in " "
        And verify that there is no "public.ao_part_table" in the "dirty_list" file in " "
        And verify that there is no "public.heap_index_table" in the "table_list" file in " "
        And verify that there is no "public.ao_part_table" in the "table_list" file in " "
        And all the data from "bkdb" is saved for verification
        When the user runs gpdbrestore with the stored timestamp and options "--prefix=foo --netbackup-block-size 4096" using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "public.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "public.ao_index_table" in "bkdb" with data
        And verify that there is no table "public.ao_part_table" in "bkdb"
        And verify that there is no table "public.heap_index_table" in "bkdb"

    @nbupartII
    Scenario: Incremental Backup and Restore of specified post data objects
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "testschema" exists in "bkdb"
        And there is a list to store the incremental backup timestamps
        And there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table" in "bkdb" with data
        And there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And there is a "ao" partition table "testschema.ao_part_table" in "bkdb" with data
        And there is a "co" table "public.co_table" in "bkdb" with data
        And there is a "co" partition table "public.co_part_table" in "bkdb" with data
        And there is a "heap" table "public.heap_table_ex" in "bkdb" with data
        And there is a "heap" partition table "public.heap_part_table_ex" in "bkdb" with data
        And there is a "co" table "testschema.co_table_ex" in "bkdb" with data
        And there is a "co" partition table "testschema.co_part_table_ex" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY testschema.heap_table ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY heap_table_ex ADD CONSTRAINT heap_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -c 'ALTER TABLE ONLY testschema.heap_table ADD CONSTRAINT heap_const_1 FOREIGN KEY (column1, column2, column3) REFERENCES heap_table_ex(column1, column2, column3);' bkdb"
        And the user runs "psql -c """create rule heap_co_rule as on insert to testschema.heap_table where column1=100 do instead insert into testschema.co_table_ex values(27, 'restore', '2013-08-19');""" bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And table "public.co_table" is assumed to be in dirty state in "bkdb"
        And partition "2" of partition table "ao_part_table" is assumed to be in dirty state in "bkdb" in schema "testschema"
        When the user runs "gpcrondump -a -x bkdb --incremental --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored in a list
        And all the data from "bkdb" is saved for verification
        And there is a file "restore_file" with tables "testschema.heap_table|testschema.ao_table|public.co_table|testschema.ao_part_table"
        And table "testschema.heap_table" is dropped in "bkdb"
        And table "testschema.ao_table" is dropped in "bkdb"
        And table "public.co_table" is dropped in "bkdb"
        And table "testschema.ao_part_table" is dropped in "bkdb"
        When the index "bitmap_co_index" in "bkdb" is dropped
        When the user runs gpdbrestore with the stored timestamp and options "--table-file restore_file --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        And verify that there is a "heap" table "testschema.heap_table" in "bkdb" with data
        And verify that there is a "ao" table "testschema.ao_table" in "bkdb" with data
        And verify that there is a "co" table "public.co_table" in "bkdb" with data
        And verify that there is a "ao" table "testschema.ao_part_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_table_ex" in "bkdb" with data
        And verify that there is a "co" table "testschema.co_table_ex" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_part_table_ex" in "bkdb" with data
        And verify that there is a "co" table "testschema.co_part_table_ex" in "bkdb" with data
        And verify that there is a "co" table "public.co_part_table" in "bkdb" with data
        And verify that there is a "heap" table "public.heap_part_table" in "bkdb" with data
        And verify that there is a "co" table "public.co_index_table" in "bkdb" with data
        And the user runs "psql -c '\d testschema.heap_table' bkdb"
        And psql should print \"heap_table_pkey\" PRIMARY KEY, btree \(column1, column2, column3\) to stdout
        And psql should print heap_co_rule AS\n.*ON INSERT TO testschema.heap_table\n.*WHERE new\.column1 = 100 DO INSTEAD  INSERT INTO testschema.co_table_ex \(column1, column2, column3\) to stdout
        And psql should print \"heap_const_1\" FOREIGN KEY \(column1, column2, column3\) REFERENCES heap_table_ex\(column1, column2, column3\) to stdout
        And the user runs "psql -c '\d public.co_index_table' bkdb"
        And psql should not print bitmap_co_index to stdout
        And verify that the data of "52" tables in "bkdb" is validated after restore

    @nbupartII
    Scenario: Restore -T for incremental dump should restore metadata/postdata objects for tablenames with English and multibyte (chinese) characters
        Given the test is initialized
        And the netbackup params have been parsed
        And there is schema "schema_heap" exists in "bkdb"
        And there is a "heap" table "schema_heap.heap_table" in "bkdb" with data
        And there is a "ao" table "public.ao_index_table" with index "ao_index" compression "None" in "bkdb" with data
        And there is a "co" table "public.co_index_table" with index "co_index" compression "None" in "bkdb" with data
        And there is a "heap" table "public.heap_index_table" with index "heap_index" compression "None" in "bkdb" with data
        And the user runs "psql -c 'ALTER TABLE ONLY public.heap_index_table ADD CONSTRAINT heap_index_table_pkey PRIMARY KEY (column1, column2, column3);' bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/create_multi_byte_char_tables.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/primary_key_multi_byte_char_table_name.sql bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/index_multi_byte_char_table_name.sql bkdb"
        When the user runs "gpcrondump -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/dirty_table_multi_byte_char.sql bkdb"
        When the user runs "gpcrondump --incremental -a -x bkdb --netbackup-block-size 4096" using netbackup
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_before"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_before"
        When there is a backupfile of tables "ao_index_table, co_index_table, heap_index_table" in "bkdb" exists for validation
        And table "public.ao_index_table" is dropped in "bkdb"
        And the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/drop_table_with_multi_byte_char.sql bkdb"
        When the user runs gpdbrestore with the stored timestamp and options "--table-file gppylib/test/behave/mgmt_utils/steps/data/include_tables_with_metadata_postdata --netbackup-block-size 4096" without -e option using netbackup
        Then gpdbrestore should return a return code of 0
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/select_multi_byte_char_tables.sql bkdb"
        Then psql should print 2000 to stdout 4 times
        And verify that there is a "ao" table "ao_index_table" in "bkdb" with data
        And verify that there is a "co" table "co_index_table" in "bkdb" with data
        And verify that there is a "heap" table "heap_index_table" in "bkdb" with data
        When the user runs "psql -f gppylib/test/behave/mgmt_utils/steps/data/describe_multi_byte_char.sql bkdb > /tmp/describe_multi_byte_char_after"
        And the user runs "psql -c '\d public.ao_index_table' bkdb > /tmp/describe_ao_index_table_after"
        Then verify that the contents of the files "/tmp/describe_multi_byte_char_before" and "/tmp/describe_multi_byte_char_after" are identical
        And verify that the contents of the files "/tmp/describe_ao_index_table_before" and "/tmp/describe_ao_index_table_after" are identical
        And the file "/tmp/describe_multi_byte_char_before" is removed from the system
        And the file "/tmp/describe_multi_byte_char_after" is removed from the system
        And the file "/tmp/describe_ao_index_table_before" is removed from the system
        And the file "/tmp/describe_ao_index_table_after" is removed from the system

