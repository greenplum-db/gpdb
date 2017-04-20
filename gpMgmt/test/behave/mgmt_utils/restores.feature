@restores
Feature: Validate command line arguments

    Scenario: gpdbrestore list_backup option with -e
        Given the backup test is initialized with no backup files
        When the user runs "gpdbrestore -a -e --list-backup -t 20160101010101"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Cannot specify --list-backup and -e together" to stdout

    Scenario: gpdbrestore, -s option with special chars
        Given the backup test is initialized with no backup files
        When the user runs command "gpdbrestore -s " DB\`~@#\$%^&*()_-+[{]}|\\;:.;\n\t \\'/?><;2 ""
        Then gpdbrestore should print "Name has an invalid character" to stdout

    Scenario: Funny characters in the table name or schema name for gpdbrestore
        Given the backup test is initialized with no backup files
        And database "testdb" exists
        And there is a "heap" table "public.table1" in "testdb" with data
        When the user runs command "gpcrondump -a -x testdb"
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
        When the user runs command "gpdbrestore -s "A\\t\\n.,!1""
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "-T public.table1 --change-schema A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout
        When the user runs gpdbrestore -e with the stored timestamp and options "-S A\\t\\n.,!1"
        Then gpdbrestore should return a return code of 2
        And gpdbrestore should print "Name has an invalid character" to stdout

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
