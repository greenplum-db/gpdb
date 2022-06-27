# Restoring Greenplum Databases 

How you restore a database from parallel backup files depends on how you answer the following questions.

1.  **Where are your backup files?** If your backup files are on the segment hosts where `gpcrondump` created them, you can restore the database with `gpdbrestore`. If you moved your backup files away from the Greenplum array, for example to an archive server with `gpcrondump`, use `gpdbrestore`.
2.  **Are you recreating the Greenplum Database system, or just restoring your data?**If Greenplum Database is running and you are restoring your data, use `gpdbrestore`. If you lost your entire array and need to rebuild the entire system from backup, use `gpinitsystem`.
3.  **Are you restoring to a system with the same number of segment instances as your backup set?** If you are restoring to an array with the same number of segment hosts and segment instances per host, use `gpdbrestore`. If you are migrating to a different array configuration, you must do a non-parallel restore. See [Restoring to a Different Greenplum System Configuration](restore-diff-system.html).

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

