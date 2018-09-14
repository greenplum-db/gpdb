@persistent_rebuild
Feature: persistent rebuild tests

    Scenario: Persistent rebuild tools should not error out when mirror is marked down and files are missing in the data directory for single node
        Given the database is running
        And the information of a "mirror" segment on any host is saved
        When user kills a mirror process with the saved information
        And user temporarily moves the data directory of the killed mirror
        And wait until the mirror is down
        Then run gppersistent_rebuild with the saved content id
        And gppersistent_rebuild should return a return code of 0
        And the user runs command "gpcheckcat -R persistent -A"
        And gpcheckcat should return a return code of 0
        And user returns the data directory to the default location of the killed mirror
        And the user runs command "gprecoverseg -a"
        And gprecoverseg should return a return code of 0
        And the segments are synchronized

    Scenario: persistent_rebuild on correctly mirrored systems should succeed if asked to rebuild persistent tables with mirrors
        Given the database is running
        And there is a "ao" table "public.ao_table" in "bkdb" with data
        And the information of a "mirror" segment on any host is saved
        Then run gppersistent_rebuild with the saved content id
        And gppersistent_rebuild should return a return code of 0
        And verify that mirror_existence_state of segment "0" is "3"

    Scenario Outline: Run persistent rebuild on different kinds of segments
        Given the database is running
        And the information of a "<segment>" segment on any host is saved
        Then run gppersistent_rebuild with the saved content id
        And gppersistent_rebuild should return a return code of 0
        And the user runs command "gpcheckcat -R persistent -A"
        And gpcheckcat should return a return code of 0
        And the segments are synchronized
        Examples:
          | segment |
          | primary |
          | master  |

    Scenario: persistent_rebuild after transaction files have been moved to another filespace
        Given the database is running
        And a filespace_config_file for filespace "tempfs" is created using config file "tempfs_config" in directory "/tmp"
        And a filespace is created using config file "tempfs_config" in directory "/tmp"
        And transaction files are moved to the filespace "tempfs"
        And the information of a "primary" segment on any host is saved
        Then run gppersistent_rebuild with the saved content id
        And gppersistent_rebuild should return a return code of 0
        And transaction files are moved to the filespace "pg_system"

    Scenario: Persistent rebuild should work on small shared_buffers value
        Given the database is running
        And there is a "ao" table "public.ao_part_table" in "bkdb" having "1000" partitions
        And a checkpoint is taken
        And the user runs "gpconfig -c shared_buffers -v 512kB"
        And gpconfig should return a return code of 0
        And the database is restarted
        And the information of a "primary" segment on any host is saved
        When run gppersistent_rebuild with the saved content id
        Then gppersistent_rebuild should return a return code of 0

    Scenario: persistent_rebuild starts database in restricted mode
        Given the database is running
        And the information of a "primary" segment on any host is saved
        And gpAdminLogs directory has no "gpstart" files
        Then run gppersistent_rebuild with the saved content id
        And gppersistent_rebuild should return a return code of 0
        And gpstart should print "[INFO]:-Starting gpstart with args: -a -R" to logfile
