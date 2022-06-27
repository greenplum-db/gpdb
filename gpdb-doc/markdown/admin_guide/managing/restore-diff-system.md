# Restoring to a Different Greenplum System Configuration 

To perform a parallel restore operation using `gpdbrestore`, the system you are restoring to must have the same configuration as the system that was backed up. To restore your database objects and data into a different system configuration, for example, to expand into a system with more segments, restore your parallel backup files by loading them through the Greenplum master. To perform a non-parallel restore, you must have:

-   A full backup set created by a `gpcrondump` operation. The backup file of the master contains the DDL to recreate your database objects. The backup files of the segments contain the data.
-   A running Greenplum Database system.
-   The database you are restoring to exists in the system.

Segment dump files contain a `COPY` command for each table followed by the data in delimited text format. Collect all of the dump files for all of the segment instances and run them through the master to restore your data and redistribute it across the new system configuration.

## To restore a database to a different system configuration 

1.  Ensure that you have a complete backup set, including dump files of the master \(`gp_dump_-1_1_timestamp`, `gp_dump_-1_1_timestamp_post_data`\) and one for each segment instance \(for example, `gp_dump_0_2_timestamp`, `gp_dump_1_3_timestamp`, `gp_dump_2_4_timestamp`, and so on\). Each dump file must have the same timestamp key. `gpcrondump` creates the dump files in each segment instance's data directory. You must collect all the dump files and move them to one location on the master host. You can copy each segment dump file to the master, load it, and then delete it after it loads successfully.
2.  Ensure that the database you are restoring to is created in the system. For example:

    ```
    $ createdb <database_name>
    ```

3.  Load the master dump file to restore the database objects. For example:

    ```
    $ psql <database_name> -f /gpdb/backups/gp_dump_-1_1_20160714
    ```

4.  Load each segment dump file to restore the data. For example:

    ```
    $ psql <database_name> -f /gpdb/backups/gp_dump_0_2_20160714
    $ psql <database_name> -f /gpdb/backups/gp_dump_1_3_20160714
    $ psql <database_name> -f /gpdb/backups/gp_dump_2_4_20160714
    $ psql <database_name> -f /gpdb/backups/gp_dump_3_5_20160714
    ...
    ```

5.  Load the post data file to restore database objects such as indexes, triggers, primary key constraints, etc.

    ```
    $ psql <database_name> -f /gpdb/backups/gp_dump_0_5_20160714_post_data
    ```

6.  Update the database sequences based on the values from the original database.

    You can use the system utilities `gunzip` and `egrep` to extract the sequence value information from the original Greenplum Database master dump file `gp_dump_-1_1_timestamp.gz` into a text file. This command extracts the information into the file `schema_path_and_seq_next_val`.

    ```
    gunzip -c <path_to_master_dump_directory>/gp_dump_-1_1_<timestamp>.gz | egrep "SET search_path|SELECT pg_catalog.setval"  
       > schema_path_and_seq_next_val
    ```

    This example command assumes the original Greenplum Database master dump file is in `/data/gpdb/master/gpseg-1/db_dumps/20150112`.

    ```
    gunzip -c /data/gpdb/master/gpseg-1/db_dumps/20150112/gp_dump_-1_1_20150112140316.gz 
      | egrep "SET search_path|SELECT pg_catalog.setval" > schema_path_and_seq_next_val
    ```

    After extracting the information, use the Greenplum Database `psql` utility to update the sequences in the database. This example command updates the sequence information in the database *test\_restore*:

    ```
    psql test_restore -f schema_path_and_seq_next_val
    ```


**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

