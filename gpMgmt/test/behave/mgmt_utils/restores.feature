@restores
Feature: Validate command line arguments

    Scenario: gpdbrestore list_backup option with -e
        Given the backup test is initialized with no backup files
        When the user runs "gpdbrestore -a -e --list-backup -t 20160101010101"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Cannot specify --list-backup and -e together" to stdout

    Scenario: Funny characters in the table name or schema name for gpdbrestore
        Given the backup test is initialized with no backup files
        And database "testdb" exists
        And there is a "heap" table "public.table1" in "testdb" with data
        When the user runs "gpcrondump -a -x testdb"
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore -e with the stored timestamp and options "--table-file test/behave/mgmt_utils/steps/data/special_chars/funny_char_table.txt"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "-T pub\\t\\nlic.!,\\t\\n.1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "--redirect A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs "gpdbrestore -s "A\\t\\n.,!1""
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.table1 --change-schema A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "-S A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout

    @ddpartIII
    Scenario: gpdbrestore -b with Full timestamp
        Given the backup test is initialized with no backup files
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the subdir from gpcrondump is stored
        And the full backup timestamp from gpcrondump is stored
        And the timestamp from gpcrondump is stored
        And the user runs gpdbrestore -e with the date directory
        Then gpdbrestore should return a return code of 0

    Scenario: Output info gpdbrestore
        Given the backup test is initialized with no backup files
        And there is a "ao" table "public.ao_index_table" in "bkdb" with data
        When the user runs "gpcrondump -a -x bkdb"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Restore type               = Full Database" to stdout
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Restore type               = Table Restore" to stdout
        And table "public.ao_index_table" is assumed to be in dirty state in "bkdb"
        When the user runs "gpcrondump -a -x bkdb --incremental"
        Then gpcrondump should return a return code of 0
        And the timestamp from gpcrondump is stored
        When the user runs gpdbrestore -e with the stored timestamp
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Restore type               = Incremental Restore" to stdout
        When the user runs gpdbrestore without -e with the stored timestamp and options "-T public.ao_index_table"
        Then gpdbrestore should return a return code of 0
        And gpdbrestore should print "Restore type               = Incremental Table Restore" to stdout

    Scenario: Valid option combinations for gpdbrestore
        When the user runs "gpdbrestore -t 20140101010101 --truncate -a"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "--truncate can be specified only with -S, -T, or --table-file option" to stdout
        When the user runs "gpdbrestore -t 20140101010101 --truncate -e -T public.foo -a"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Cannot specify --truncate and -e together" to stdout
        And there is a table-file "/tmp/table_file_foo" with tables "public.ao_table, public.co_table"
        And the user runs "gpdbrestore -t 20140101010101 -T public.ao_table --table-file /tmp/table_file_foo"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Cannot specify -T and --table-file together" to stdout
        Then the file "/tmp/table_file_foo" is removed from the system
        When the user runs "gpdbrestore -u /tmp --ddboost -s bkdb"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "-u cannot be used with DDBoost parameters" to stdout

    Scenario: gpdbrestore with -d with invalid master data directory
        When the user runs "gpdbrestore -a -t 20140101010101 -d /tmp"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "gpdbrestore failed.* No such file or directory" to stdout

    Scenario: gpdbrestore with -l to log to /tmp directory
        Given the backup test is initialized with no backup files
        When the user runs "gpdbrestore -l /tmp"
        Then gpdbrestore should return a return code of 2
        And the "gpdbrestore" log file should exist under "/tmp"

    @ddonly
    @ddboostsetup
    Scenario: Cleanup DDBoost dump directories
        Given the DDBoost dump directory is deleted
