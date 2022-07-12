# gpcrondump 

Writes out a database to SQL script files. The script files can be used to restore the database using the [gpdbrestore](gpdbrestore.html) utility. The `gpcrondump` utility can be called directly or from a `crontab` entry.

**Note:** This utility is deprecated and will not be supported after the end of Greenplum Database 5.x Support Life.

## Synopsis 

```
gpcrondump -x <database_name> 
   [-s <schema> | -S <schema> | -t <schema.table> | -T <schema.table>] 
   [--table-file=<filename> | --exclude-table-file=<filename>] 
   [--schema-file=<filename> | --exclude-schema-file=<filename>]
   [--dump-stats] 
   [-u <backup_directory>] [-R <post_dump_script>] [--incremental] 
   [-K <timestamp> [--list-backup-files] ]
   [--prefix <prefix_string> [--list-filter-tables]
   [-c [--cleanup-date <yyyymmdd> | --cleanup-total <n>] ]
   [-z] [-r] 
   [-f <free_space_percent>] [-b] [-h] [-H] [-j | -k] [-g] [-G] [-C] 
   [-d <master_data_directory>] [-B <parallel_processes>] [-a] [-q]
   [-y <reportfile>] [-l <logfile_directory>]
   [--email-file <path_to_file>] [-v]
   { [-E <encoding>] [--inserts | --column-inserts] [--oids] 
     [--no-owner | --use-set-session-authorization] [--no-privileges] 
     [--rsyncable]
     { [--ddboost [--replicate --max-streams <max_IO_streams>]
     [--ddboost-skip-ping] [--ddboost-storage-unit=<unit-ID>]] } | 
     { [--netbackup-service-host <netbackup_server>
     --netbackup-policy <netbackup_policy>
     --netbackup-schedule <netbackup_schedule> [--netbackup-block-size <size>] ]
     [--netbackup-keyword <keyword>] ] } }

gpcrondump --ddboost-host <ddboost_hostname>
   [--ddboost-host <ddboost_hostname> ... ]
   --ddboost-user <ddboost_user> --ddboost-backupdir <backup_directory> 
   [--ddboost-remote] [--ddboost-skip-ping]
   [--ddboost-storage-unit=<unit-ID>] 

gpcrondump --ddboost-show-config [--remote]

gpcrondump --ddboost-config-remove

gpcrondump -o [--cleanup-date <yyyymmdd> | --cleanup-total <n>] 

gpcrondump -? 

gpcrondump --version
```

## Description 

The `gpcrondump` utility dumps the contents of a database into SQL script files, which can then be used to restore the database schema and user data at a later time using `gpdbrestore`. During a dump operation, users will still have full access to the database.

By default, dump files are created in their respective master and segment data directories in a directory named `db_dumps/YYYYMMDD`. The data dump files are compressed by default using `gzip`.

The utility backs up the database-level settings for the server configuration parameters `gp_default_storage_options`, `optimizer`, and `search_path`. The settings are restored when you restore the database with the `gpdbrestore` utility and specify the `-e` option to create an empty target database before performing a restore operation.

If you specify an option to back up schemas, such as `-s`, or `-S`, all procedural languages that are defined in the database are also backed up even though they are not schema specific. The languages are backed up to support any functions that might be backed up. External items such as shared libraries that are used by a language are not backed up. Also, languages are not backed up if you specify an option to back up only tables, such as `-t`, `-T`.

After a backup operation completes, the utility checks the `gpcrondump` status file for SQL execution errors and displays a warning if an error is found. The default location of the backup status files are in the `db_dumps/date/` directory.

If you specify an option that includes or excludes tables or schemas, such as `-t`, `-T`, `-s`, or `-S`, the schema qualified names of the tables that are backed up are listed in the file `gp_dump_timestamp_table`. The file is stored in the backup directory of the master segment.

`gpcrondump` allows you to schedule routine backups of a Greenplum database using `cron` \(a scheduling utility for UNIX operating systems\). Cron jobs that call `gpcrondump` should be scheduled on the master host.

**Warning:** Backing up a database with `gpcrondump` while simultaneously running `ALTER TABLE` might cause `gpcrondump` to fail.

Backing up a database with `gpcrondump` while simultaneously running DDL commands might cause issues with locks. You might see either the DDL command or `gpcrondump` waiting to acquire locks.

**About Database, Schema, and Table Names**

You can specify names of databases, schemas, and tables that contain these special characters.

`" ' ` ~ # $ % ^ & * ( ) _ - + [ ]  > < \ | ; : / ?` and the space character.

**Note:** The characters `!`, comma \(`,`\), and period \(`.`\) are not supported. Also, the tab \(`\t`\) and newline \(`\n`\) characters are not supported.

When the name contains special characters and is specified on the command line, the name must be enclosed in double quotes \(`"`\). Double quotes are optional for names that do not contain special characters. For example, either use of quotes is valid on the command line `"my#1schema".mytable` or `"my#1schema"."mytable"`. Within the name, these special characters must be escaped with a backslash \(`\`\) : `" ` $ \` .

When the name is specified in an input file, the name must *not* be enclosed in double quotes. Special characters do not require escaping.

## Backup Filenames 

The utility creates backup files with this file name format.

```
<prefix_>gp_dump_<content>_<dbid>_<timestamp>
```

The `content` and `dbid` are identifiers for the Greenplum Database segment instances that are assigned by Greenplum Database. For information about the identifiers, see the Greenplum Database system catalog table *gp\_segment\_configuration* in the *Greenplum Database Reference Guide*.

**Using Data Domain Boost**

The `gpcrondump` utility is used to schedule Data Domain Boost \(DD Boost\) backup operations. The utility is also used to set, change, or remove one-time credentials and storage unit ID for DD Boost. The `gpcrondump`, `gpdbrestore`, and `gpmfr` utilities use the DD Boost credentials to access Data Domain systems. DD Boost information is stored in these files.

-   `DDBOOST_CONFIG` is used by `gpdbrestore` and `gpcrondump` for backup and restore operations with the Data Domain system. The `gpdbrestore` utility creates or updates the file when you specify Data Domain information with the `--ddboost-host` option.
-   `DDBOOST_MFR_CONFIG` is used by `gpmfr` for remote replication operations with the remote Data Domain system. The `gpdbrestore` utility creates or updates the file when you specify Data Domain information with the `--ddboost-host` option and the `--ddboost-remote` option.

The configuration files are created in the current user \(`gpadmin`\) home directory on the Greenplum Database master and segment hosts. The path and file name cannot be changed.

When you use DD Boost to perform a backup operation, the operation uses a storage unit on a Data Domain system. You can specify the storage unit ID when you perform these operations:

-   When you set the DD Boost credentials with the `--ddboost-host` option. If you specify the `--ddboost-storage-unit` option, the storage unit ID is written to the Greenplum Database DD Boost configuration file `DDBOOST_CONFIG`. If the storage unit ID is not specified, the default value is `GPDB`.
-   When you perform a backup operation with the `--ddboost` option. When you specify the `--ddboost-storage-unit` option, the utility uses the specified Data Domain storage unit for the operation. The value in the configuration file is not changed.

When performing a full backup operation \(not an incremental backup\), the storage unit is created on the Data Domain system if it does not exist.

A storage unit is not created if these `gpcrondump` options are specified: `--incremental`, `--list-backup-file`, `--list-filter-tables`, `-o`, or `--ddboost-config-remove`.

Use the `gpcrondump` option `--ddboost-show-config` to display the current DD Boost configuration information from the master configuration file. Specify the `--remote` option to display the configuration information for the remote Data Domain system.

For information about using DD Boost and Data Domain systems with Greenplum Database, see "Backing Up and Restoring Databases" in the *Greenplum Database Administrator Guide*.

**Using NetBackup**

Veritas NetBackup integration is included with Tanzu Greenplum. Greenplum Database must be configured to communicate with the Veritas NetBackup master server that is used to backup the database.

When backing up a large amount of data, set the NetBackup `CLIENT_READ_TIMEOUT` option to a value that is at least twice the expected duration of the operation \(in seconds\). The `CLIENT_READ_TIMEOUT` default value is `300` seconds \(5 minutes\).

See the *Greenplum Database Administrator Guide* for information on configuring Greenplum Database and NetBackup and backing up and restoring with NetBackup.

**About Return Codes**

The following is a list of the codes that `gpcrondump` returns.

-   **0** – Dump completed with no problems
-   **1** – Dump completed, but one or more warnings were generated
-   **2** – Dump failed with a fatal error

**Email Notifications**

To have `gpcrondump` send out status email notifications after a back up operation completes, you must place a file named `mail_contacts` in the home directory of the Greenplum superuser \(`gpadmin`\) or in the same directory as the `gpcrondump` utility \(`$GPHOME/bin`\). This file should contain one email address per line. `gpcrondump` will issue a warning if it cannot locate a `mail_contacts` file in either location. If both locations have a `mail_contacts` file, then the one in `$HOME` takes precedence.

You can customize the email Subject and From lines of the email notifications that `gpcrondump` sends after a back up completes for a database. You specify the option `--email-file` with the location of a YAML file that contains email Subject and From lines that `gpcrondump` uses. for information about the format of the YAML file, see [File Format for Customized Emails](#email_yaml).

**Note:** The UNIX mail utility must be running on Greenplum Database host and must be configured to allow the Greenplum superuser \(`gpadmin`\) to send email.

**Limitations**

Dell EMC DD Boost is integrated with Tanzu Greenplum and requires a DD Boost license. Open source Greenplum Database cannot use the DD Boost software, but can back up to a Dell EMC Data Domain system mounted as an NFS share on the Greenplum master and segment hosts.

NetBackup is not compatible with DD Boost. Both NetBackup and DD Boost cannot be used in a single back up operation.

For incremental back up sets, a full backup and associated incremental backups, the backup set must be on a single device. For example, a backup set must all be on a file system. The backup set cannot have some backups on the local file system and others on a Data Domain system or a NetBackup system.

For external tables, the table definition is backed up, however the data is not backed up. For leaf child partition of a partitioned table that is a readable external table, the leaf child partition data is not backed up.

## Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-b \(bypass disk space check\)
:   Bypass disk space check. The default is to check for available disk space, unless `--ddboost` is specified. When using Data Domain Boost, this option is always enabled.

    **Note:** Bypassing the disk space check generates a warning message. With a warning message, the return code for `gpcrondump` is `1` if the dump is successful. \(If the dump fails, the return code is `2`, in all cases.\)

-B parallel\_processes
:   The number of segments to check in parallel for pre/post-dump validation. If not specified, the utility will start up to 60 parallel processes depending on how many segment instances it needs to dump.

-c \(clear old dump files first\)
:   Specify this option to delete old backups before performing a back up. In the `db_dumps` directory, the directory where the name is the oldest date is deleted. If the directory name is the current date, the directory is not deleted. The default is to not delete old backup files.

:   The deleted directory might contain files from one or more backups.

    **Warning:** Before using this option, ensure that incremental backups required to perform the restore are not deleted. The `gpdbrestore` utility option `--list-backup` lists the backup sets required to perform a backup.

:   If `--ddboost` is specified, only the old files on Data Domain Boost are deleted.

:   You can specify the option `--cleanup-date` or `--cleanup-total` to specify backup sets to delete.

:   This option is not supported with the `-u` option.

-C \(clean catalog before restore\)
:   Clean out the catalog schema prior to restoring database objects. `gpcrondump` adds the `DROP` command to the SQL script files when creating the backup files. When the script files are used by the `gpdbrestore` utility to restore database objects, the `DROP` commands remove existing database objects before restoring them.

:   If `--incremental` is specified and the files are on NFS storage, the `-C` option is not supported. The database objects are not dropped if the `-C` option is specified.

--cleanup-date=yyyymmdd
:   Remove backup sets for the date yyyy-mm-dd. The date format is yyyymmdd. If multiple backup sets were created on the date, all the backup sets for that date are deleted. If no backup sets are found, `gpcrondump` returns a warning message and no backup sets are deleted. If the `-c` option is specified, the backup process continues.

:   Valid only with the `-c` or `-o` option.

    **Warning:** Before using this option, ensure that incremental backups required to perform the restore are not deleted. The `gpdbrestore` utility option `--list-backup` lists the backup sets required to perform a backup.

--cleanup-total=n
:   Remove the n oldest backup sets based on the backup timestamp.

:   If there are fewer than n backup sets, `gpcrondump` returns a warning message and no backup sets are deleted. If the `-c` option is specified, the backup process continues.

:   Valid only with the `-c` or `-o` option.

    **Warning:** Before using this option, ensure that incremental backups required to perform the restore are not deleted. The `gpdbrestore` utility option `--list-backup` lists the backup sets required to perform a backup.

--column-inserts
:   Dump data as `INSERT` commands with column names.

:   If `--incremental` is specified, this option is not supported.

-d master\_data\_directory
:   The master host data directory. If not specified, the value set for `$MASTER_DATA_DIRECTORY` will be used.

--ddboost \[--replicate --max-streams max\_IO\_streams \] \[--ddboost-skip-ping\]
:   Use Data Domain Boost for this backup. Before using Data Domain Boost, set up the Data Domain Boost credential with the `--ddboost-host`option. Also, see [Using Data Domain Boost](#ddboost).

    If `--ddboost` is specified, the `-z` option \(uncompressed\) is recommended.

    Backup compression \(turned on by default\) should be turned off with the `-z` option. Data Domain Boost will deduplicate and compress the backup data before sending it to the Data Domain system.

    `--replicate --max-streamsmax\_IO\_streams` is optional. If you specify this option, `gpcrondump` replicates the backup on the remote Data Domain server after the backup is complete on the primary Data Domain server. `max_IO_streams` specifies the maximum number of Data Domain I/O streams that can be used when replicating the backup set on the remote Data Domain server from the primary Data Domain server.

    You can use `gpmfr` to replicate a backup if replicating a backup with `gpcrondump` takes a long time and prevents other backups from occurring. Only one instance of `gpcrondump` can be running at a time. While `gpcrondump` is being used to replicate a backup, it cannot be used to create a backup.

    You can run a mixed backup that writes to both a local disk and Data Domain. If you want to use a backup directory on your local disk other than the default, use the `-u` option. Mixed backups are not supported with incremental backups. For more information about mixed backups and Data Domain Boost, see "Backing Up and Restoring Databases" in the *Greenplum Database Administrator Guide*.

    **Important:** Never use the Greenplum Database default backup options with Data Domain Boost.

    To maximize Data Domain deduplication benefits, retain at least 30 days of backups.

    **Note:** The `-b`, `-c`, `-f`, `-G`, `-g`, `-R`, and `-u` options change if `--ddboost` is specified. See the options for details.

:   The DDBoost backup options are not supported if the NetBackup options are specified.

--ddboost-host ddboost\_hostname \[--ddboost-host ddboost\_hostname ...\]
--ddboost-user ddboost\_user --ddboost-backupdir backup\_directory
\[--ddboost-remote\] \[--ddboost-skip-ping\]
:   Sets the Data Domain Boost credentials. Do not combine this options with any other `gpcrondump` options. Do not enter just one part of this option.

:   ddboost\_hostname is the IP address \(or hostname associated to the IP\) of the host. There is a 30-character limit. If you use two or more network connections to connect to the Data Domain system, specify each connection with the `--ddboost-host` option.

:   ddboost\_user is the Data Domain Boost user name. There is a 30-character limit.

:   backup\_directory is the location for the backup files, configuration files, and global objects on the Data Domain system. The location on the system is `GPDB/`backup\_directory.

:   `--ddboost-remote` is optional. It indicates that the configuration parameters are for the remote Data Domain system used for backup replication and Data Domain Boost managed file replication. Credentials for the remote Data Domain system must be configured to use the `--replicate` option or the `gpmfr` management utility.

:   For example:

:   ```
gpcrondump --ddboost-host 192.0.2.230 --ddboost-user ddboostusername --ddboost-backupdir gp_production
```

    **Note:** When setting Data Domain Boost credentials, the `--ddboost-backupdir` option is ignored if the `--ddboost-remote` option is specified for a Data Domain system that is used for the replication of backups. The `--ddboost-backupdir` value is for backup operations with a Data Domain system, not for backup replication.

:   After running `gpcrondump` with these options, the system verifies the limits on the host and user names and prompts for the Data Domain Boost password. Enter the password when prompted; the password is not echoed on the screen. There is a 40-character limit on the password that can include lowercase letters \(a-z\), uppercase letters \(A-Z\), numbers \(0-9\), and special characters \($, %, \#, +, etc.\).

:   The system verifies the password. After the password is verified, the system creates encrypted `DDBOOST_CONFIG` files in the user's home directory.

:   In the example, the `--ddboost-backupdir`option specifies the backup directory `gp_production` in the Data Domain Storage Unit GPDB.

    **Note:** If there is more than one operating system user using Data Domain Boost for backup and restore operations, repeat this configuration process for each of those users.

    **Important:** Set up the Data Domain Boost credential before running any Data Domain Boost backups with the `--ddboost` option, described above.

--ddboost-config-remove
:   Removes all Data Domain Boost credentials from the master and all segments on the system. Do not enter this option with any other `gpcrondump` option.

--ddboost-show-config \[--remote\]
:   Optional. Displays the DD Boost configuration file information for the Data Domain server. Specify this option with the `--remote` option to display the configuration file information for remote Data Domain server. No backup is performed.

--ddboost-skip-ping
:   Specify this option to skip the ping of a Data Domain system. When working with a Data Domain system, ping is used to ensure that the Data Domain system is reachable. If the Data Domain system is configured to block ICMP ping probes, specify this option.

--ddboost-storage-unit=unit-ID
:   Optional. Specify a valid storage unit name for the Data Domain system that is used for backup and restore operations. The default storage unit ID is `GPDB`. See [Using Data Domain Boost](#ddboost).

    -   Specify this option with the `--ddboost-host` option to create or update the storage unit ID in the DD Boost credentials file.
    -   Specify this option with the `--ddboost` option to override the storage unit ID in the DD Boost credentials file when performing a backup operation.

:   When performing a full backup operation \(not an incremental backup\), the storage unit is created on the Data Domain system if it does not exist.

:   A replication operation uses the same storage unit ID on both local and remote Data Domain systems.

--dump-stats
:   Specify this option to back up database statistics. The data is written to an SQL file and can be restored manually or with `gpdbrestore` utility.

:   The statistics are written in the master data directory to `db_dumps/`YYYYMMDD`/prefix\_string\_gp_statistics_-1_1_`timestamp.

:   If this option is specified with options that include or exclude tables or schemas, the utility backs up only the statistics for the tables that are backed up.

-E encoding
:   Character set encoding of dumped data. Defaults to the encoding of the database being dumped. See the *Greenplum Database Reference Guide* for the list of supported character sets.

-email-file path\_to\_file
:   Specify the fully-qualified location of the YAML file that contains the customized Subject and From lines that are used when `gpcrondump` sends notification emails about a database back up.

:   For information about the format of the YAML file, see [File Format for Customized Emails](#email_yaml).

-f free\_space\_percent
:   When checking that there is enough free disk space to create the dump files, specifies a percentage of free disk space that should remain after the dump completes. The default is 10 percent.

    This is option is not supported if `--ddboost` or `--incremental` is specified.

-g \(copy config files\)
:   Secure a copy of the master and segment configuration files `postgresql.conf`, `pg_ident.conf`, and `pg_hba.conf`. These configuration files are dumped in the master or segment data directory to `db_dumps/`YYYYMMDD`/config_files_`timestamp`.tar.`

:   If `--ddboost` is specified, the backup is located on the default storage unit in the directory specified by `--ddboost-backupdir` when the Data Domain Boost credentials were set.

-G \(dump global objects\)
:   Back up database metadata information that is not associated with any particular schema or table such as roles and tablespaces. Global objects are dumped in the master data directory to `db_dumps/`YYYYMMDD`/prefix\_string\_gp_global_-1_1_`timestamp.

:   If `--ddboost` is specified, the backup is located on the default storage unit in the directory specified by `--ddboost-backupdir` when the Data Domain Boost credentials were set.

-h \(record dump details\)
:   Record details of the database dump in database table `public.gpcrondump_history` in the database supplied via `-x` option. The `gpcrondump` utility will create the table if it does not currently exist. The `public` schema must exist in the database so that `gpcrondump` can create the `public.gpcrondump_history` table. The default is to record the database dump details.

:   This option will be deprecated in a future release.

-H \(disable recording dump details\)
:   Disable recording details of database dump in database table `public.gpcrondump_history` in the database supplied via -x option. If not specified, the utility will create/update the history table. The `-H` option cannot be selected with the `-h` option.

    **Note:** The `gpcrondump` utility creates the `public.gpcrondump_history` table by default. If the `public` schema has been deleted from the database, you must specify the `-H` option to prevent `gpcrondump` from returning an error when it attempts to create the table.

--incremental \(backup changes to append-optimized tables\)
:   Adds an incremental backup to a backup set. When performing an incremental backup, the complete backup set created prior to the incremental backup must be available. The complete backup set includes the following backup files:

    -   The last full backup before the current incremental backup
    -   All incremental backups created between the time of the full backup the current incremental backup

    An incremental backup is similar to a full back up except for append-optimized tables, including column-oriented tables. An append-optimized table is backed up only if one of the following operations was performed on the table after the last backup.

    ```
    ALTER TABLE 
    INSERT 
    DELETE 
    UPDATE 
    TRUNCATE 
    DROP and then re-create the table
    ```

    For partitioned append-optimized tables, only the changed table partitions are backed up.

    The `-u` option must be used consistently within a backup set that includes a full and incremental backups. If you use the `-u` option with a full backup, you must use the `-u` option when you create incremental backups that are part of the backup set that includes the full backup.

    You can create an incremental backup for a full backup of set of database tables. When you create the full backup, specify the `--prefix` option to identify the backup. To include a set of tables in the full backup, use either the `-t` option or `--table-file` option. To exclude a set of tables, use either the `-T` option or the `--exclude-table-file` option. See the description of the option for more information on its use.

    To create an incremental backup based on the full backup of the set of tables, specify the option -`-incremental` and the `--prefix` option with the string specified when creating the full backup. The incremental backup is limited to only the tables in the full backup.

    **Warning:** `gpcrondump` does not check for available disk space prior to performing an incremental backup.

    **Important:** An incremental back up set, a full backup and associated incremental backups, must be on a single device. For example, a the backups in a backup set must all be on a file system or must all be on a Data Domain system.

--inserts
:   Dump data as `INSERT`, rather than `COPY` commands.

:   If `--incremental` is specified, this option is not supported.

-j \(vacuum before dump\)
:   Run `VACUUM` before the dump starts.

-K timestamp \[--list-backup-files\]
:   Specify the `timestamp` that is used when creating a backup. The timestamp is 14-digit string that specifies a date and time in the format yyyymmddhhmmss. The date is used for backup directory name. The date and time is used in the backup file names. If `-K`timestamp is not specified, a timestamp is generated based on the system time.

:   When adding a backup to set of backups, `gpcrondump` returns an error if the `timestamp` does not specify a date and time that is more recent than all other backups in the set.

:   `--list-backup-files` is optional. When you specify both this option and the `-K `timestamp` option, `gpcrondump` does not perform a backup. `gpcrondump` creates two text files that contain the names of the files that will be created when `gpcrondump` backs up a Greenplum database. The text files are created in the same location as the backup files.

:   The file names use the `timestamp` specified by the `-K `timestamp` option and have the suffix `_pipes` and `_regular_files`. For example:

:   ```
gp_dump_20130514093000_pipes
gp_dump_20130514093000_regular_files
```

:   The `_pipes` file contains a list of file names that be can be created as named pipes. When `gpcrondump` performs a backup, the backup files will generate into the named pipes. The `_regular_files` file contains a list of backup files that must remain regular files. `gpcrondump` and `gpdbrestore` use the information in the regular files during backup and restore operations. To backup a complete set of Greenplum Database backup files, the files listed in the `_regular_files` file must also be backed up after the completion of the backup job.

:   To use named pipes for a backup, you need to create the named pipes on all the Greenplum Database hosts and make them writable before running `gpcrondump`.

:   If `--ddboost` is specified, `-K timestamp [--list-backup-files]` is not supported.

-k \(vacuum after dump\)
:   Run `VACUUM` after the dump has completed successfully.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

--netbackup-block-size size
:   Specify the block size, in bytes, of data being transferred to the Veritas NetBackup server. The default is 512 bytes.

:   NetBackup options are not supported if DDBoost backup options are specified.

--netbackup-keyword keyword
:   Specify a keyword for the backup that is transferred to the Veritas NetBackup server. NetBackup adds the keyword property and the specified keyword value to the NetBackup .img files that are created for the backup.

:   The maximum length of this parameter is 127 characters.

:   NetBackup options are not supported if DDBoost backup options are specified.

--netbackup-policy netbackup\_policy
:   The name of the NetBackup policy created for backing up Greenplum Database.

:   NetBackup options are not supported if DDBoost backup options are specified.

:   The maximum length of this parameter is 127 characters.

--netbackup-service-host netbackup\_server
:   The NetBackup master server that Greenplum Database connects to when backing up to NetBackup.

:   NetBackup options are not supported if DDBoost backup options are specified.

:   The maximum length of this parameter is 127 characters.

--netbackup-schedule netbackup\_schedule
:   The name of the NetBackup schedule created for backing up Greenplum Database.

:   NetBackup options are not supported if DDBoost backup options are specified

:   The maximum length of this parameter is 127 characters.

--no-owner
:   Do not output commands to set object ownership.

--no-privileges
:   Do not output commands to set object privileges \(`GRANT`/`REVOKE` commands\).

-o \(clear old dump files only\)
:   Clear out old dump files only, but do not run a dump. This will remove the oldest dump directory except the current date's dump directory. All dump sets within that directory will be removed.

:   **Warning:** Before using this option, ensure that incremental backups required to perform the restore are not deleted. The `gpdbrestore` utility option `--list-backup` lists the backup sets required to perform a backup.

:   If `--ddboost` is specified, only the old files on Data Domain Boost are deleted.

:   You can specify the option `--cleanup-date` or `--cleanup-total` to specify backup sets to delete.

:   If `--incremental` is specified, this option is not supported. If this option is specified, `-u` will be ignored.

--oids
:   Include object identifiers \(oid\) in dump data.

:   If `--incremental` is specified, this option is not supported.

--prefix prefix\_string \[--list-filter-tables\]
:   Prepends `prefix_string` followed by an underscore character \(\_\) to the names of all the backup files created during a backup.

:   `--list-filter-tables` is optional. When you specify both options, `gpcrondump` does not perform a backup. For the full backup created by `gpcrondump` that is identified by the `prefix-string`, the tables that were included or excluded for the backup are listed. You must also specify the `--incremental` option if you specify the `--list-filter-tables` option.

:   If `--ddboost` is specified, `--prefix`prefix\_string `[--list-filter-tables]` is not supported.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-r \(rollback on failure\)
:   Rollback the dump files \(delete a partial dump\) if a failure is detected. The default is to not rollback.

    **Note:** This option is not supported if `--ddboost` is specified.

-R post\_dump\_script
:   The absolute path of a script to run after a successful dump operation. For example, you might want a script that moves completed dump files to a backup host. This script must reside in the same location on the master and all segment hosts.

--rsyncable
:   Passes the `--rsyncable` flag to the `gzip` utility to synchronize the output occasionally, based on the input during compression. This synchronization increases the file size by less than 1% in most cases. When this flag is passed, the `rsync(1)` program can synchronize compressed files much more efficiently. The `gunzip` utility cannot differentiate between a compressed file created with this option, and one created without it.

-s schema\_name
:   Dump all the tables that are qualified by the specified schema in the database. The `-s` option can be specified multiple times. System catalog schemas are not supported. If you want to specify multiple schemas, you can also use the `--schema-file=`filename option in order not to exceed the maximum token limit.

:   Only a set of tables or set of schemas can be specified. For example, the `-s` option cannot be specified with the `-t` option.

:   If `--incremental` is specified, this option is not supported.

-S schema\_name
:   A schema name to *exclude* from the database dump. The `-S` option can be specified multiple times. If you want to specify multiple schemas, you can also use the `--exclude-schema-file=`filename option in order not to exceed the maximum token limit.

:   Only a set of tables or set of schemas can be specified. For example, this option cannot be specified with the `-t` option.

:   If `--incremental` is specified, this option is not supported.

-t schema.table\_name
:   Dump only the named table in this database. The `-t` option can be specified multiple times. If you want to specify multiple tables, you can also use the `--table-file=`filename option in order not to exceed the maximum token limit.

:   Only a set of tables or set of schemas can be specified. For example, this option cannot be specified with the `-s` option.

:   If `--incremental` is specified, this option is not supported.

-T schema.table\_name
:   A table name to *exclude* from the database dump. The `-T` option can be specified multiple times. If you want to specify multiple tables, you can also use the `--exclude-table-file=`filename option in order not to exceed the maximum token limit.

:   Only a set of tables or set of schemas can be specified. For example, this option cannot be specified with the `-s` option.

:   If `--incremental` is specified, this option is not supported.

--exclude-schema-file=filename
:   Excludes all the tables that are qualified by the specified schemas listed in the filename from the database dump. The file filename contains any number of schemas, listed one per line.

:   Only a set of tables or set of schemas can be specified. For example, this option cannot be specified with the `-t` option.

:   If `--incremental` is specified, this option is not supported.

--exclude-table-file=filename
:   Excludes all tables listed in the filename from the database dump. The file filename contains any number of tables, listed one per line.

:   Only a set of tables or set of schemas can be specified. For example, this cannot be specified with the `-s` option.

:   If `--incremental` is specified, this option is not supported.

--schema-file=filename
:   Dumps only the tables that are qualified by the schemas listed in the filename. The file filename contains any number of schemas, listed one per line.

:   Only a set of tables or set of schemas can be specified. For example, this option cannot be specified with the `-t` option.

:   If `--incremental` is specified, this option is not supported.

--table-file=filename
:   Dumps only the tables listed in the filename. The file filename contains any number of tables, listed one per line.

:   Only a set of tables or set of schemas can be specified. For example, this cannot be specified with the `-s` option.

:   If `--incremental` is specified, this option is not supported.

-u backup\_directory
:   Specifies the absolute path where the backup files will be placed on each host. If the path does not exist, it will be created, if possible. If not specified, defaults to the data directory of each instance to be backed up. Using this option may be desirable if each segment host has multiple segment instances as it will create the dump files in a centralized location rather than the segment data directories.

:   This option is not supported if `--ddboost` is specified. This option is ignored if `-o` is specified.

--use-set-session-authorization
:   Use `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to set object ownership.

-v \| --verbose
:   Specifies verbose mode.

--version \(show utility version\)
:   Displays the version of this utility.

-x database\_name
:   Required. The name of the Greenplum database to dump.

-y reportfile
:   This option is deprecated and will be removed in a future release. If specified, a warning message is returned stating that the `-y` option is deprecated.

:   Specifies the full path name where a copy of the backup job log file is placed on the master host. The job log file is created in the master data directory or if running remotely, the current working directory.

-z \(no compression\)
:   Do not use compression. Default is to compress the dump files using `gzip`.

:   Use this option \(`-z`\) for NFS and Data Domain Boost backups.

-? \(help\)
:   Displays the online help.

## File Format for Customized Emails 

You can configure `gpcrondump` to send an email notification after a back up operation completes for a database. To customize the From and Subject lines of the email that are sent for a database, you create a YAML file and specify the location of the file with the option `--email-file`. In the YAML file, you can specify a different From and Subject line for each database that `gpcrondump` backs up. This is the format of the YAML file to specify a custom From and Subject line for a database:

```

EMAIL_DETAILS:
    -
        DBNAME: <database_name>
        FROM: <from_user>
        SUBJECT: <subject_text>

```

When email notification is configured for `gpcrondump`, the from\_user and the subject\_text are the strings that `gpcrondump` uses in the email notification after completing the back up for database\_name.

This example YAML file specifies different From and Subject lines for the databases `testdb100` and `testdb200`.

```

EMAIL_DETAILS:
    -
        DBNAME: testdb100
        FROM: RRP_MPE2_1
        SUBJECT: backup completed for Database 'testdb100'
    -
        DBNAME: testdb200
        FROM: Report_from_DCDDEV_host
        SUBJECT: Completed backup for database 'testdb200'
```

## Examples 

Call `gpcrondump` directly and dump `mydatabase` \(and global objects\):

```
gpcrondump -x mydatabase -c -g -G
```

A `crontab` entry that runs a backup of the `sales` database \(and global objects\) nightly at one past midnight:

```
01 0 * * * /home/gpadmin/gpdump.sh >> gpdump.log
```

The content of dump script gpdump.sh is:

```
#!/bin/bash
  export GPHOME=/usr/local/greenplum-db
  export MASTER_DATA_DIRECTORY=/data/gpdb_p1/gp-1
  . $GPHOME/greenplum_path.sh  
  gpcrondump -x sales -c -g -G -a -q 
```

This example creates two text files, one with the suffix `_pipes` and the other with `_regular_files`. The `_pipes` file contain the file names that can be named pipes when you backup the Greenplum database mytestdb.

```
gpcrondump -x mytestdb -K 20131030140000 --list-backup-files
```

To use incremental backup with a set of database tables, you must create a full backup of the set of tables and specify the `--prefix` option to identify the backup set. The following example uses the --table-file option to create a full backup of the set of files listed in the file `user-tables`. The prefix `user_backup` identifies the backup set.

```
gpcrondump -x mydatabase --table-file=user-tables
  --prefix user_backup
```

To create an incremental backup for the full backup created in the previous example, specify the `--incremental` option and the option `--prefix user_backup` to identify backup set. This example creates an incremental backup.

```
gpcrondump -x mydatabase --incremental --prefix user_backup
```

This command lists the tables that were included or excluded for the full backup.

```
gpcrondump -x mydatabase --incremental --prefix user_backup 
  --list-filter-tables
```

This command backs up the database *customer* and specifies a NetBackup policy and schedule that are defined on the NetBackup master server`nbu_server1`. A block size of 1024 bytes is used to transfer data to the NetBackup server.

```
gpcrondump -x customer --netbackup-service-host=nbu_server1
  --netbackup-policy=gpdb_cust --netbackup-schedule=gpdb_backup
  --netbackup-block-size=1024
```

## See Also 

[gpdbrestore](gpdbrestore.html)

