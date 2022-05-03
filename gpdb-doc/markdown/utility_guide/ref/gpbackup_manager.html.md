---
title: gpbackup_manager 
---

Display information about existing backups, delete existing backups, or encrypt passwords for secure storage in plugin configuration files.

**Note:** The `gpbackup_manager` utility is available only in the commercial release of Tanzu Greenplum Backup and Restore.


## <a id="synopsis"></a>Synopsis 

```
gpbackup_manager [<command>]
```

where *command* is:

```
delete-backup <timestamp> [--plugin-config <config-file>]
| display-report <timestamp>
| encrypt-password --plugin-config <config-file>
| list-backups
| replicate-backup <timestamp> --plugin-config <config-file>
| help [<command>]
```

## <a id="cmds"></a>Commands 

`delete-backup timestamp`
:   Deletes the backup set with the specified timestamp.

`display-report timestamp`
:   Displays the backup report for a specified timestamp.

`encrypt-password`
:   Encrypts plain-text passwords for storage in the DD Boost plugin configuration file.

`list-backups`
:   Displays a list of backups that have been taken. If the backup history file does not exist, the command exits with an error message. See [Table 1](#table_yls_rgw_g3b) for a description of the columns in this list.

`replicate-backup timestamp`
:   For a backup on a Data Domain server, replicates the backup to a second \(remote\) Data Domain server. The timestamp is the timestamp of the backup on the Data Domain server. The `--plugin-config config-file` option specifies the DD Boost configuration file that contains the information to access the backup and the remote Data Domain server. For information about the configuration file, see [Using the DD Boost Storage Plugin with gpbackup, gprestore, and gpbackup\_manager](../../admin_guide/managing/backup-ddboost-plugin.html). For information about replicating backups, see [Replicating Backups](../../admin_guide/managing/replication-ddb.html)

`help command`
:   Displays a help message for the specified command.

## <a id="opts"></a>Options 

--plugin-config config-file
:   The `delete-backup` command requires this option if the backup is stored in S3 or a Data Domain system. The `encrypt-password` command requires this option.
    **Note:** When you delete backup sets stored in a Data Domain system, you must pass in the same configuration file that was passed in when the backups were created. Otherwise, the `gpbackup_manager delete-backup` command will exit with an error.

-h \| --help
:   Displays a help message for the `gpbackup_manager` command. For help on a specific `gpbackup_manager` command, enter `gpbackup_manager help command`. For example:
    ```
    $ gpbackup_manager help encrypt-password
    ```

## <a id="desc"></a>Description 

The `gpbackup_manager` utility manages backup sets created using the [`gpbackup`](gpbackup.html) utility. You can list backups, display a report for a backup, and delete a backup. `gpbackup_manager` can also encrypt passwords to store in a DD Boost plugin configuration file.

Greenplum Database must be running to use the `gpbackup_manager` utility.

Backup history is saved on the Greenplum Database master host in the file `$MASTER_DATA_DIRECTORY/gpbackup_history.yaml`. If no backups have been created yet, or if the backup history has been deleted, `gpbackup_manager` commands that depend on the file will display an error message and exit. If the backup history contains invalid YAML syntax, a yaml error message is displayed.

Versions of `gpbackup` earlier than v1.13.0 did not save the backup duration in the backup history file. The `list-backups` command duration column is empty for these backups.

The `encrypt-password` command is used to encrypt Data Domain user passwords that are saved in a DD Boost plug-In configuration file. To use this option, the `pgcrypto` extension must be enabled in the Greenplum Database `postgres` database. See the Tanzu Greenplum Backup and Restore installation instructions for help installing `pgcrypto`.

The `encrypt-password` command prompts you to enter and then re-enter the password to be encrypted. To maintain password secrecy, characters entered are echoed as asterisks. If replication is enabled in the specified DD Boost configuration file, the command also prompts for a password for the remote Data Domain account. You must then copy the output of the command into the DD Boost configuration file.

The following table describes the contents of the columns in the list that is output by the `gpbackup_manager list-backups` command.

<table cellpadding="4" cellspacing="0" summary="" id="topic1__table_yls_rgw_g3b" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 1. </span>Backup List Report</span></caption><colgroup><col style="width:33.33333333333333%" /><col style="width:66.66666666666666%" /></colgroup><thead class="thead" style="text-align:left;">
          <tr class="row">
            <th class="entry cellrowborder" style="vertical-align:top;" id="d1143e275">Column</th>

            <th class="entry cellrowborder" style="vertical-align:top;" id="d1143e278">Description</th>

          </tr>

        </thead>
<tbody class="tbody">
          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">timestamp</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Timestamp value (<code class="ph codeph">YYYYMMDDHHMMSS</code>) that specifies the time the
              backup was taken.</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">date</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Date the backup was taken.</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">status</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Status of the backup operation, <code class="ph codeph">Success</code> or
                <code class="ph codeph">Failure</code>.</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">database</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Name of the database backed up (specified on the <code class="ph codeph">gpbackup</code>
              command line with the <code class="ph codeph">--dbname</code> option).</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">type</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Which classes of data are included in the backup. Can be one of the following: <ul class="sl simple">
                <li class="sli"><strong class="ph b">full</strong> - contains all global and local metadata, and user data for the
                  database. This kind of backup can be the base for an incremental backup. Depending
                  on the <code class="ph codeph">gpbackup</code> options specified, some objects could have been
                  filtered from the backup.</li>

                <li class="sli"><strong class="ph b">incremental</strong> – contains all global and local metadata, and user data
                  changed since a previous <strong class="ph b">full</strong> backup.</li>

                <li class="sli"><strong class="ph b">metadata-only</strong> – contains only the global and local metadata for the
                  database. Depending on the <code class="ph codeph">gpbackup</code> options specified, some
                  objects could have been filtered from the backup.</li>

                <li class="sli"><strong class="ph b">data-only</strong> – contains only user data from the database. Depending on the
                    <code class="ph codeph">gpbackup</code> options specified, some objects could have been
                  filtered from the backup.</li>

              </ul>

            </td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">object filtering</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">The object filtering options that were specified at least once on the
                <code class="ph codeph">gpbackup</code> command line, or blank if no filtering operations were
              used. To see the object filtering details for a specific backup, run the
                <code class="ph codeph">gpbackup_manager report</code> command for the backgit st<ul class="sl simple">
                <li class="sli"><strong class="ph b">include-schema</strong> – at least one <code class="ph codeph">--include-schema</code> option
                  was specified.</li>

                <li class="sli"><strong class="ph b">exclude-schema</strong> – at least one <code class="ph codeph">--exclude-schema</code> option
                  was specified.</li>

                <li class="sli"><strong class="ph b">include-table</strong> – at least one <code class="ph codeph">--include-table</code> option was
                  specified.</li>

                <li class="sli"><strong class="ph b">exclude-table</strong> – at least one <code class="ph codeph">--exclude-table</code> option was
                  specified.</li>

              </ul>
</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">plugin</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">The name of the binary plugin file that was used to configure the backup
              destination, excluding path information.</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">duration</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">The amount of time (<code class="ph codeph">hh:mm:ss</code> format) taken to complete the
              backup.</td>

          </tr>

          <tr class="row">
            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e275 ">date deleted</td>

            <td class="entry cellrowborder" style="vertical-align:top;" headers="d1143e278 ">Indicates the status of the deletion. If blank, the backup still exists. Other possible values include: 
              <ul class="sl simple">
                <li class="sli"><strong class="ph b">In progress</strong> - the deletion is in progress.</li>

                <li class="sli"><strong class="ph b">Plugin Backup Delete Failed</strong> - Last delete attempt failed to delete
                  backup from plugin storage.</li>

                <li class="sli"><strong class="ph b">Local Delete Failed</strong> - Last delete attempt failed to delete backup from
                  local storage.</li>

                <li class="sli"><strong class="ph b">A timestamp indicating the date deleted.</strong></li>

              </ul>
</td>

          </tr>

        </tbody>
</table>

## <a id="examples"></a>Examples 

1.  Display a list of the existing backups.

    ```
    gpadmin@mdw:$ gpbackup_manager list-backups
                timestamp        date                       status    database   type            object filtering   plugin                    duration   date deleted
                20210721191330   Wed Jul 21 2021 19:13:30   Success   sales      full                               gpbackup_ddboost_plugin   00:20:25   In progress
                20210721191201   Wed Jul 21 2021 19:12:01   Success   sales      full                               gpbackup_ddboost_plugin   00:15:21   Plugin Backup Delete Failed
                20210721191041   Wed Jul 21 2021 19:10:41   Success   sales      full                               gpbackup_ddboost_plugin   00:10:25   Local Delete Failed
                20210721191022   Wed Jul 21 2021 19:10:22   Success   sales      full            include-schema                               00:02:35   Wed Jul 21 2021 19:24:59
                20210721190942   Wed Jul 21 2021 19:09:42   Success   sales      full            exclude-schema                               00:01:11
                20210721190826   Wed Jul 21 2021 19:08:26   Success   sales      data-only                                                    00:05:17
                20210721190818   Wed Jul 21 2021 19:08:18   Success   sales      metadata-only                                                00:01:01
                20210721190727   Wed Jul 21 2021 19:07:27   Success   sales      full                                                         00:07:22
    
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
    
    Are you sure you want to delete-backup 20190620145126? (y/n)y
    Deletion of 20190620145126 in progress.
    
    Deletion of 20190620145126 complete.
    ```

4.  Delete a backup stored on a Data Domain system. The DD Boost plugin configuration file must be specified with the `--plugin-config` option.

    ```
    $ gpbackup_manager delete-backup 20190620160656 --plugin-config ~/ddboost_config.yaml
    
    Are you sure you want to delete-backup 20190620160656? (y/n)y
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

**Parent topic:**[Backup Utility Reference](../../backup-utilities.html)

