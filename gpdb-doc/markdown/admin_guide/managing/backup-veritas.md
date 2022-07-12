# Backing Up Databases with Veritas NetBackup 

For Greenplum Database on Red Hat Enterprise Linux, you can configure Greenplum Database to perform backup and restore operations with Veritas NetBackup. You configure Greenplum Database and NetBackup and then run a Greenplum Database `gpcrondump` or `gpdbrestore` command. The following topics describe how to set up NetBackup and back up or restore Greenplum Databases.

-   [About NetBackup Software](#topic_lpr_bcz_3p)
-   [Limitations](#topic_nq4_m1z_3p)
-   [Configuring Greenplum Database Hosts for NetBackup](#topic_mlb_1bz_3p)
-   [Configuring NetBackup for Greenplum Database](#topic_vwm_v1z_3p)
-   [Performing a Back Up or Restore with NetBackup](#topic_t1d_qbz_3p)
-   [Example NetBackup Back Up and Restore Commands](#topic_myv_lbz_3p)

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

## About NetBackup Software 

NetBackup includes the following server and client software:

-   The NetBackup master server manages NetBackup backups, archives, and restores. The master server is responsible for media and device selection for NetBackup.
-   NetBackup Media servers are the NetBackup device hosts that provide additional storage by allowing NetBackup to use the storage devices that are attached to them.
-   NetBackup client software that resides on the Greenplum Database hosts that contain data to be backed up.

See the *Veritas NetBackup Getting Started Guide* for information about NetBackup.

See the Release Notes for information about compatible NetBackup versions for this release of Tanzu Greenplum.

## Limitations 

-   NetBackup is not compatible with DDBoost. Both NetBackup and DDBoost cannot be used in a single back up or restore operation.
-   For incremental back up sets, a full backup and associated incremental backups, the backup set must be on a single device. For example, a backup set must all be on a NetBackup system. The backup set cannot have some backups on a NetBackup system and others on the local file system or a Data Domain system.

## Configuring Greenplum Database Hosts for NetBackup 

You install and configure NetBackup client software on the Greenplum Database master host and all segment hosts. The NetBackup client software must be able to communicate with the NetBackup server software.

1.  Install the NetBackup client software on Greenplum Database hosts. See the NetBackup installation documentation for information on installing NetBackup clients on UNIX systems.
2.  Set parameters in the NetBackup configuration file `/usr/openv/netbackup/bp.conf` on the Greenplum Database master and segment hosts. Set the following parameters on each Greenplum Database host.

    |Parameter|Description|
    |---------|-----------|
    |`SERVER`|Host name of the NetBackup Master Server|
    |`MEDIA_SERVER`|Host name of the NetBackup Media Server|
    |`CLIENT_NAME`|Host name of the Greenplum Database Host|

    See the *Veritas NetBackup Administrator's Guide* for information about the bp.conf file.

3.  Set the `LD_LIBRARY_PATH` environment variable for Greenplum Database hosts to use NetBackup client. Greenplum Database installs NetBackup SDK library files that are used with the NetBackup client. To configure Greenplum Database to use the library files that correspond to the version of the NetBackup client that is installed on the hosts, add the following line to the file `$GPHOME/greenplum_path.sh`:

    ```
    LD_LIBRARY_PATH=$GPHOME/lib/nbu<NN>/lib:$LD_LIBRARY_PATH 
    ```

    Replace the NN with the NetBackup client version that is installed on the host \(for example, use 77 for NetBackup 7.7.x\).

    The `LD_LIBRARY_PATH` line should be added before this line in `$GPHOME/greenplum_path.sh`:

    ```
    export LD_LIBRARY_PATH
    ```

4.  Execute this command to remove the current `LD_LIBRARY_PATH` value:

    ```
    unset LD_LIBRARY_PATH
    ```

5.  Execute this command to update the environment variables for Greenplum Database:

    ```
    source $GPHOME/greenplum_path.sh
    ```


See the *Veritas NetBackup Administrator's Guide* for information about configuring NetBackup servers.

1.  Ensure that the Greenplum Database hosts are listed as NetBackup clients for the NetBackup server.

    In the NetBackup Administration Console, the information for the NetBackup clients, Media Server, and Master Server is in the **NetBackup Management** node within the **Host Properties** node.

2.  Configure a NetBackup storage unit. The storage unit must be configured to point to a writable disk location.

    In the NetBackup Administration Console, the information for NetBackup storage units is in **NetBackup Management** node within the **Storage** node.

3.  Configure a NetBackup backup policy and schedule within the policy.

    In the NetBackup Administration Console, the **Policy** node below the **Master Server** node is where you create a policy and a schedule for the policy.

    -   In the **Policy Attributes** tab, these values are required for Greenplum Database:

        The value in the **Policy type** field must be DataStore

        The value in the **Policy storage** field is the storage unit created in the previous step.

        The value in **Limit jobs per policy** field must be at least 3.

    -   In the **Policy Schedules** tab, create a NetBackup schedule for the policy.

## Configuring NetBackup for Greenplum Database 

See the *Veritas NetBackup Administrator's Guide* for information about configuring NetBackup servers.

1.  Ensure that the Greenplum Database hosts are listed as NetBackup clients for the NetBackup server.

    In the NetBackup Administration Console, the information for the NetBackup clients, Media Server, and Master Server is in **NetBackup Management** node within the **Host Properties** node.

2.  Configure a NetBackup storage unit. The storage unit must be configured to point to a writable disk location.

    In the NetBackup Administration Console, the information for NetBackup storage units is in **NetBackup Management** node within the **Storage** node.

3.  Configure a NetBackup backup policy and schedule within the policy.

    In the NetBackup Administration Console, the **Policy** node below the **Master Server** node is where you create a policy and a schedule for the policy.

    -   In the **Policy Attributes** tab, these values are required for Greenplum Database:

        The value in the **Policy type** field must be DataStore

        The value in the **Policy storage** field is the storage unit created in the previous step.

        The value in **Limit jobs per policy** field must be at least 3.

    -   In the **Policy Schedules** tab, create a NetBackup schedule for the policy.

## Performing a Back Up or Restore with NetBackup 

The Greenplum Database `gpcrondump` and `gpdbrestore` utilities support options to back up or restore data to a NetBackup storage unit. When performing a back up, Greenplum Database transfers data files directly to the NetBackup storage unit. No backup data files are created on the Greenplum Database hosts. The backup metadata files are both stored on the hosts and backed up to the NetBackup storage unit.

When performing a restore, the files are retrieved from the NetBackup server, and then restored.

Following are the `gpcrondump` utility options for NetBackup:

```
--netbackup-service-host netbackup\_master\_server
--netbackup-policy policy\_name
--netbackup-schedule schedule\_name
--netbackup-block-size size (optional)
--netbackup-keyword keyword (optional) 
```

The `gpdbrestore` utility provides the following options for NetBackup:

```
--netbackup-service-host netbackup\_master\_server
--netbackup-block-size size (optional)
```

**Note:** When performing a restore operation from NetBackup, you must specify the backup timestamp with the `gpdbrestore` utility `-t` option.

The policy name and schedule name are defined on the NetBackup master server. See [Configuring NetBackup for Greenplum Database](#topic_vwm_v1z_3p) for information about policy name and schedule name. See the *Greenplum Database Utility Guide* for information about the Greenplum Database utilities.

**Note:** You must run the `gpcrondump` or `gpdbrestore` command during a time window defined for the NetBackup schedule.

During a back up or restore operation, a separate NetBackup job is created for the following types of Greenplum Database data:

-   Segment data for each segment instance
-   C database data
-   Metadata
-   Post data for the master
-   State files Global objects \(`gpcrondump -G` option\)
-   Configuration files for master and segments \(`gpcrondump -g` option\)
-   Report files \(`gpcrondump -h` option\)

In the NetBackup Administration Console, the Activity Monitor lists NetBackup jobs. For each job, the job detail displays Greenplum Database backup information.

**Note:** When backing up or restoring a large amount of data, set the NetBackup `CLIENT_READ_TIMEOUT` option to a value that is at least twice the expected duration of the operation \(in seconds\). The `CLIENT_READ_TIMEOUT` default value is `300` seconds \(5 minutes\).

For example, if a backup takes 3 hours, set the `CLIENT_READ_TIMEOUT` to `21600` \(2 x 3 x 60 x 60\). You can use the NetBackup `nbgetconfig` and `nbsetconfig` commands on the NetBackup server to view and change the option value.

For information about `CLIENT_READ_TIMEOUT` and the `nbgetconfig`, and `nbsetconfig` commands, see the NetBackup documentation.

## Example NetBackup Back Up and Restore Commands 

This `gpcrondump` command backs up the database *customer* and specifies a NetBackup policy and schedule that are defined on the NetBackup master server `nbu_server1`. A block size of 1024 bytes is used to transfer data to the NetBackup server.

```
gpcrondump -x customer --netbackup-service-host=nbu_server1 \
   --netbackup-policy=gpdb_cust --netbackup-schedule=gpdb_backup \
   --netbackup-block-size=1024
```

This `gpdbrestore` command restores Greenplum Database data from the data managed by NetBackup master server `nbu_server1`. The option `-t 20170530090000` specifies the timestamp generated by `gpcrondump` when the backup was created. The `-e` option specifies that the target database is dropped before it is restored.

```
gpdbrestore -t 20170530090000 -e --netbackup-service-host=nbu_server1
```

