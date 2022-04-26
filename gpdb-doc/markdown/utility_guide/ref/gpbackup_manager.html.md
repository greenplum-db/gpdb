---
title: gpbackup_manager 
---

Display information about existing backups, delete existing backups, or encrypt passwords for secure storage in plugin configuration files.

**Note:** `gpbackup_manager` is available only with the commercial release of Tanzu Greenplum Backup and Restore.

## <a id="syn"></a>Synopsis 

```
gpbackup_manager [<command>]
```

where *command* is:

```
delete-backup <timestamp> [--plugin-config <config-file>]
| display-report <timestamp>  
| encrypt-password --plugin-config> <config-file>  
| list-backups
| help [command]
```

## <a id="commands"></a>Commands 

`delete-backup timestamp`
:   Deletes the backup set with the specified timestamp.

`display-report timestamp`
:   Displays the backup report for a specified timestamp.

`encrypt-password`
:   Encrypts plain-text passwords for storage in the DD Boost plugin configuration file.

`list-backups`
:   Displays a list of backups that have been taken. If the backup history file does not exist, the command exits with an error message. See [Table 1](#table_yls_rgw_g3b) for a description of the columns in this list.

`help <command>`
:   Displays a help message for the specified command.

## <a id="opts"></a>Options 

--plugin-config config-file
:   The `delete-backup` command requires this option if the backup Is stored in s3 or a Data Domain system. The `encrypt-password` command requires this option.

-h \| --help
:   Displays a help message for the `gpbackup_manager` command. For help on a specific `gpbackup_manager` command, enter `gpbackup_manager help <command>`. For example:
    ```
    $ gpbackup_manager help encrypt-password
    ```

## <a id="desc"></a>Description 

The `gpbackup_manager` utility manages backup sets created using the [`gpbackup`](gpbackup.html) utility. You can list backups, display a report for a backup, and delete a backup. `gpbackup_manager` can also encrypt passwords to store in a DD Boost plugin configuration file.

Greenplum Database must be running to use the `gpbackup_manager` utility.

Backup history is saved on the Greenplum Database coordinator host in the file `$COORDINATOR_DATA_DIRECTORY/gpbackup_history.yaml`. If no backups have been created yet, or if the backup history has been deleted, `gpbackup_manager` commands that depend on the file will display an error message and exit. If the backup history contains invalid YAML syntax, a yaml error message is displayed.

Versions of `gpbackup` earlier than v1.13.0 did not save the backup duration in the backup history file. The `list-backups` command duration column is empty for these backups.

The `encrypt-password` command is used to encrypt Data Domain user passwords that are saved in a DD Boost plug-In configuration file. To use this option, the `pgcrypto` extension must be enabled in the Greenplum Database `postgres` database. See the Tanzu Greenplum Backup and Restore installation instructions for help installing `pgcrypto`.

The `encrypt-password` command prompts you to enter and then re-enter the password to be encrypted. To maintain password secrecy, characters entered are echoed as asterisks. If replication is enabled in the specified DD Boost configuration file, the command also prompts for a password for the remote Data Domain account. You must then copy the output of the command into the DD Boost configuration file.

The following table describes the contents of the columns in the list that is output by the `gpbackup_manager list-backups` command.

|Column|Description|
|------|-----------|
|timestamp|Timestamp value \(`YYYYMMDDHHMMSS`\) that specifies the time the backup was taken.|
|date|Date the backup was taken.|
|database|Name of the database backed up \(specified on the `gpbackup` command line with the `--dbname` option\).|
|type|Which classes of data are included in the backup. Can be one of the following: -   **full** - contains all global and local metadata, and user data for the database. This kind of backup can be the base for an incremental backup. Depending on the `gpbackup` options specified, some objects could have been filtered from the backup.
-   **incremental** – contains all global and local metadata, and user data changed since a previous **full** backup.
-   **metadata-only** – contains only the global and local metadata for the database. Depending on the `gpbackup` options specified, some objects could have been filtered from the backup.
-   **data-only** – contains only user data from the database. Depending on the `gpbackup` options specified, some objects could have been filtered from the backup.

|
|object filtering|The object filtering options that were specified at least once on the `gpbackup` command line, or blank if no filtering operations were used. To see the object filtering details for a specific backup, run the `gpbackup_manager report` command for the backgit st-   **include-schema** – at least one `--include-schema` option was specified.
-   **exclude-schema** – at least one `--exclude-schema` option was specified.
-   **include-table** – at least one `--include-table` option was specified.
-   **exclude-table** – at least one `--exclude-table` option was specified.

|
|plugin|The name of the binary plugin file that was used to configure the backup destination, excluding path information.|
|duration|The amount of time \(`hh:mm:ss` format\) taken to complete the backup.|
|date deleted|The date the backup was deleted, or blank if the backup still exists.|

## <a id="exs"></a>Examples 

1.  Display a list of the existing backups.

    ```
    gpadmin@cdw:$ gpbackup_manager list-backups
      timestamp        date                       database   type            object filtering   plugin   duration   date deleted
      20190719092809   Fri Jul 19 2019 09:28:09   sales      full            include-schema              01:49:38   Fri Jul 19 2019 09:30:34
      20190719092716   Fri Jul 19 2019 09:27:16   sales      full            exclude-schema              01:38:45
      20190719092609   Fri Jul 19 2019 09:26:09   sales      data-only                                   01:07:22
      20190719092557   Fri Jul 19 2019 09:25:57   sales      metadata-only                               00:00:19
      20190719092530   Fri Jul 19 2019 09:25:30   sales      full                                        01:50:27
    
    ```

2.  Display the backup report for the backup with timestamp 20190612154608.

    ```
    $ gpbackup_manager display-report 20190612154608
      
    Greenplum Database Backup Report
      
    Timestamp Key: 20190612154608
    GPDB Version: 5.14.0+dev.8.gdb327b2a3f build commit:db327b2a3f6f2b0673229e9aa164812e3bb56263
    gpbackup Version: 1.11.0
    Database Name: sales
    Command Line: gpbackup --dbname sales
    Compression: gzip
    Plugin Executable: None
    Backup Section: All Sections
    Object Filtering: None
    Includes Statistics: No
    Data File Format: Multiple Data Files Per Segment
    Incremental: False
    Start Time: 2019-06-12 15:46:08
    End Time: 2019-06-12 15:46:53
    Duration: 0:00:45
      
    Backup Status: Success
    Database Size: 3306 MB
      
    Count of Database Objects in Backup:
    Aggregates                   12
    Casts                        4
    Constraints                  0
    Conversions                  0
    Database GUCs                0
    Extensions                   0
    Functions                    0
    Indexes                      0
    Operator Classes             0
    Operator Families            1
    Operators                    0
    Procedural Languages         1
    Protocols                    1
    Resource Groups              2
    Resource Queues              6
    Roles                        859
    Rules                        0
    Schemas                      185
    Sequences                    207
    Tables                       431
    Tablespaces                  0
    Text Search Configurations   0
    Text Search Dictionaries     0
    Text Search Parsers          0
    Text Search Templates        0
    Triggers                     0
    Types                        2
    Views                        0
    ```

3.  Delete the local backup with timestamp 20190620145126.

    ```
    $ gpbackup_manager delete-backup 20190620145126
    
    Are you sure you want to delete-backup 20190620145126? (y/n)**y**
    Deletion of 20190620145126 in progress.
    
    Deletion of 20190620145126 complete.
    ```

4.  Delete a backup stored on a Data Domain system. The DD Boost plugin configuration file must be specified with the `--plugin-config` option.

    ```
    $ gpbackup_manager delete-backup 20190620160656 --plugin-config ~/ddboost_config.yaml
    
    Are you sure you want to delete-backup 20190620160656? (y/n)**y**
    Deletion of 20190620160656 in progress.
          
    Deletion of 20190620160656 done.
    ```

5.  Encrypt a password. A DD Boost plugin configuration file must be specified with the `--plugin-config` option.

    ```
    $ gpbackup_manager encrypt-password --plugin-config ~/ddboost_rep_on_config.yaml
              
    Please enter your password ******
    Please verify your password ******
    Please enter your remote password ******
    Please verify your remote password ******
              
    Please copy/paste these lines into the plugin config file:
              
    password: "c30d04090302a0ff861b823d71b079d23801ac367a74a1a8c088ed53beb62b7e190b7110277ea5b51c88afcba41857d2900070164db5f3efda63745dfffc7f2026290a31e1a2035dac"
    password_encryption: "on"
    remote_password: "c30d04090302c764fd06bfa1dade62d2380160a8f1e4d1ff0a4bb25a542fb1d31c7a19b98e9b2f00e7b1cf4811c6cdb3d54beebae67f605e6a9c4ec9718576769b20e5ebd0b9f53221"
    remote_password_encryption: "on"
    
    ```


## <a id="section9"></a>See Also 

[gprestore](gprestore.html), [Parallel Backup with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup.html) and [Using the S3 Storage Plugin with gpbackup and gprestore](../../admin_guide/managing/backup-s3-plugin.html)

