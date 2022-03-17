---
title: Using gpbackup and gprestore with BoostFS 
---

You can use the Greenplum Database `gpbackup` and `gprestore` utilities with the Data Domain DD Boost File System Plug-In \(BoostFS\) to access a Data Domain system. BoostFS leverages DD Boost technology and helps reduce bandwidth usage, can improve backup-times, offers load-balancing and in-flight encryption, and supports the Data Domain multi-tenancy feature set.

You install the BoostFS plug-in on the Greenplum Database host systems to provide access to a Data Domain system as a standard file system mount point. With direct access to a BoostFS mount point, `gpbackup` and `gprestore` can leverage the storage and network efficiencies of the DD Boost protocol for backup and recovery.

For information about configuring BoostFS, you can download the *BoostFS for Linux Configuration Guide* from the Dell support site [https://www.dell.com/support](https://www.dell.com/support) \(requires login\). After logging into the support site, you can find the guide by searching for "BoostFS for Linux Configuration Guide". You can limit your search results by choosing to list only Manuals & Documentation as resources.

To back up or restore with BoostFS, you include the option `--backup-dir` with the `gpbackup` or `gprestore` command to access the Data Domain system.

-   **[Installing BoostFS](../managing/backup-boostfs.html)**  

-   **[Backing Up and Restoring with BoostFS](../managing/backup-boostfs.html)**  


**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_zp4_mvv_bdb"></a>Installing BoostFS 

Download the latest BoostFS RPM from the Dell support site [https://www.dell.com/support](https://www.dell.com/support) \(requires login\).

After logging into the support site, you can find the RPM by searching for "boostfs". You can limit your search results by choosing to list only Downloads & Drivers as resources. To list the most recent RPM near the top of your search results, sort your results by descending date.

The RPM supports both RHEL and SuSE.

These steps install BoostFS and create a mounted directory that accesses a Data Domain system.

Perform the steps on all Greenplum Database hosts. The mounted directory you create must be the same on all hosts.

1.  Copy the BoostFS RPM to the host and install the RPM.

    After installation, the DDBoostFS package files are located under `/opt/emc/boostfs`.

2.  Set up the BoostFS lockbox with the storage unit with the `boostfs` utility. Enter the Data Domain user password at the prompts.

    ```
    /opt/emc/boostfs/bin/boostfs lockbox set -d <Data_Domain_IP> -s <Storage_Unit> -u <Data_Domain_User>
    ```

    The `<Storage\_Unit>` is the Data Domain storage unit ID. The <Data\_Domain\_User\> is a Data Domain user with access to the storage unit.

3.  Create the directory in the location you want to mount BoostFS.

    ```
    mkdir <path_to_mount_directory>
    ```

4.  Mount the Data Domain storage unit with the `boostfs` utility. Use the `mount` option `-allow-others=true` to allow other users to write to the BoostFS mounted file system.

    ```
    /opt/emc/boostfs/bin/boostfs mount <path_to_mount_directory> -d $<Data_Domain_IP> -s <Storage_Unit> -o allow-others=true
    ```

5.  Confirm that the mount was successful by running this command.

    ```
    mountpoint <mounted_directory>
    ```

    The command lists the directory as a mount point.

    ```
    <mounted_directory> is a mountpoint
    ```


You can now run `gpbackup` and `gprestore` with the `--backup-dir` option to back up a database to `<mounted\_directory>` on the Data Domain system and restore data from the Data Domain system.

**Parent topic:**[Using gpbackup and gprestore with BoostFS](../managing/backup-boostfs.html)

## <a id="topic_t4z_tvv_bdb"></a>Backing Up and Restoring with BoostFS 

These are required `gpbackup` options when backing up data to a Data Domain system with BoostFS.

-   `--backup-dir` - Specify the mounted Data Domain storage unit.
-   `--no-compression` - Disable compression. Data compression interferes with DD Boost data de-duplication.
-   `--single-data-file` - Create a single data file on each segment host. A single data file avoids a BoostFS stream limitation.

When you use `gprestore` to restore a backup from a Data Domain system with BoostFS, you must specify the mounted Data Domain storage unit with the option `--backup-dir`.

When you use the `gpbackup` option `--single-data-file`, you cannot specify the `--jobs` option to perform a parallel restore operation with `gprestore`.

This example `gpbackup` command backs up the database `test`. The example assumes that the directory `/boostfs-test` is the mounted Data Domain storage unit.

```
$ gpbackup --dbname test --backup-dir /boostfs-test/ --single-data-file --no-compression
```

These commands drop the database `test` and restore the database from the backup.

```
$ dropdb test
$ gprestore --backup-dir /boostfs-test/ --timestamp 20171103153156 --create-db
```

The value `20171103153156` is the timestamp of the `gpbackup` backup set to restore. For information about how `gpbackup` uses timesamps when creating backups, see [Parallel Backup with gpbackup and gprestore](backup-gpbackup.html). For information about the `-timestamp` option, see [gprestore](../../utility_guide/ref/gprestore.html).

**Parent topic:**[Using gpbackup and gprestore with BoostFS](../managing/backup-boostfs.html)

