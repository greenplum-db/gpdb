# Backup Process and Locks 

During backups, the `gpcrondump` utility locks the *pg\_class* system table and the tables that are backed up. Locking the *pg\_class* table with an `EXCLUSIVE` lock ensures that no tables are added, deleted, or altered until `gpcrondump` locks tables that are to be backed up with `ACCESS SHARE` locks.

The steps below describe the process `gpcrondump` follows to dump databases, including what happens before locks are placed, while locks are held, and after locks are removed.

If more than one database is specified, this process is executed for each database in sequence.

-   `gpcrondump` parses the command-line arguments and verifies that options and arguments are properly specified.
-   If any of the following filter options are specified, `gpcrondump` prepares filters to determine the set of tables to back up. Otherwise, all tables are included in the backup.
    -   `-s <schema_name>` – Includes all the tables qualified by the specified schema.
    -   `-S <schema_name>` – Excludes all tables qualified by the specified schema.
    -   `--schema-file=<filename>` – Includes all the tables qualified by the schema listed in *filename*.
    -   `--exclude-schema-file=<filename>` – Excludes all tables qualified by the schema listed in *filename*.
    -   `-t <schema.table_name>` – Dumps the specified table.
    -   `-T <schema.table_name>` – Excludes the specified table.
    -   `--table-file=<filename>` – Dumps the tables specified in *filename*.
    -   `--exclude-table-file=<filename>` – Dumps all tables except tables specified in *filename*.
-   `gpcrondump` verifies the backup targets:
    -   Verifies that the database exists.
    -   Verifies that specified tables or schemas to be dumped exist.
    -   Verifies that the current primary for each segment is up.
    -   Verifies that the backup directory exists and is writable for the master and each segment.
    -   Verifies that sufficient disk space is available to back up each segment. Note that if more than one segment is backed up to the same disk volume, this disk space check cannot ensure there is space available for all segments.
-   Places an `EXCLUSIVE` lock on the catalog table `pg_class`. The lock permits only concurrent read operations on a table. While the lock is on the catalog table, relations such as tables, indexes, and views cannot be created, altered, or dropped in the database.

    `gpcrondump` starts a thread to watch for a lock file \(`<dump_dir>/gp_lockfile_<timestamp>`\) to appear, signaling the parallel backup on the segments has completed.

-   `gpcrondump` locks tables that are to be backed up with an `ACCESS SHARE` lock.

    An `ACCESS SHARE` lock only conflicts with an `ACCESS EXCLUSIVE` lock. The following SQL statements acquire an `ACCESS EXCLUSIVE` lock:

    -   `ALTER TABLE`
    -   `CLUSTER`
    -   `DROP TABLE`
    -   `REINDEX`
    -   `TRUNCATE`
    -   `VACUUM FULL`
-   Threads are created and dispatched for the master and segments to perform the database dump.
-   When the threads have acquired `ACCESS SHARED` locks on all the required tables, the `<dump_dir>/gp_lockfile_<timestamp>` lock file is created, signaling `gpcrondump` to release the `EXCLUSIVE` lock on the *pg\_class* catalog table, while tables are being backed up.
-   `gpcrondump` checks the status files for each primary segment for any errors to report. If a dump fails and the `-r` flag was specified, the backup is rolled back.
-   The `ACCESS SHARE` locks on the target tables are released.
-   If the backup succeeded and a post-dump script was specified with the -R option, `gpcrondump` runs the script now.
-   `gpcrondump` reports the backup status, sends email if configured, and saves the backup status in the `public.gpcrondump_history` table in the database.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

