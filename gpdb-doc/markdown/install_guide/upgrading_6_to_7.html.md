---
title: Upgrading from Greenplum 6 to Greenplum 7 
---

This topic walks you through upgrading from Greenplum 6 to Greenplum 7. The upgrade procedure varies, depending on whether you are upgrading the software while remaining on the same hardware or upgrading the software while moving to new hardware.

>**WARNING**
>There are a substantial number of changes between Greenplum 6 and Greenplum 7 that could potentially affect your existing application when you move to Greenplum 7. Before going any further, familiarize yourself with all of these changes [here](./changes-6-7-landing-page.html).


## <a id="same_hardware"></a>Upgrading to Greenplum 7 On the Same Hardware

Follow the steps below to upgrade to Greenplum 7 while remaining on the same hardware. You will move your data using the `gpbackup/gprestore` utilities. There are a number of caveats with respect to backing up and restoring your data. See [Backup adn Restore Caveats](#backup-and-restore-caveats) for details.

To upgrade while staying on the same hardware:

1. Run the `gpbackup` utility to back up your data to an external data source. For more infomation on `gpbackup`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

2. Delete the Greenplum 6 cluster by issuing the [`gpdeletesystem` command](../utility_guide/ref/gpdeletesystem.html).

3. Initialize a Greenplum 7 cluster by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

4. Run the `gprestore` utility to restore your data to the Greenplum 7 cluster from the external data source. For more infomation on `gprestore`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

## <a id="new_hardware"></a>Upgrading to Greenplum 7 While Moving to New Hardware

Follow the steps below to upgrade to Greenplum 7 while moving to new hardware. 

If you are upgrading from Greenplum 6 to Greenplum 7 and changing hardware you may move your data in one of two ways:

- By using the `gpbackup/gprestore` utilities
- By using the `gpcopy` utility

For help deciding which option is best for you, see [Options for Moving Data](#options-for-moving-data), below.

There are a number of caveats with respect to backing up and restoring your data. See [Backup and Restore Caveats](#backup-and-restore-caveats) for details.

To upgrade while moving to new hardware using `gpbackup/gprestore`:

1. Run the `gpbackup` utility to back up the date from your Greenplum 6 cluster to an external data source. For more infomation on `gpbackup`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

2. Initalize a Greenplum 7 cluster on the new hardware, by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

3. Run the `gprestore` utility to restore your data to the Greenplum 7 cluster from the external data source. For more infomation on `gprestore`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

To upgrade while moving to new hardware using `gpcopy`:

1. Initalize a Greenplum 7 cluster on the new hardware, by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

2. Verify network connectivity between your Greenplum 6 and your Greenplum 7 cluster. 

3. Run the `gpcopy` utility to migrate your data to the Greenplum 7 cluster. For more information on how to migrate your data with `gpcopy`, see [Migrating Data with gpcopy](https://docs.vmware.com/en/VMware-Greenplum-Data-Copy-Utility/2.6/greenplum-copy/gpcopy-migrate.html).

## <a id="data-movement-options"></a>Options for Moving Data

This section helps you decide whether to use `gpbackup/grestore`  or `gpcopy` for moving data during an upgrade that involves moving to new hardware. 


## <a id="br-caveats"></a>Backup and Restore Caveats

kdkd





**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)