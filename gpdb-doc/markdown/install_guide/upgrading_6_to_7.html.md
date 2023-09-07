---
title: Upgrading from Greenplum 6 to Greenplum 7 
---

This topic walks you through upgrading from Greenplum 6 to Greenplum 7. The upgrade procedure varies, depending on whether you are upgrading the software while remaining on the same hardware or upgrading the software while moving to new hardware.

>**WARNING**
>There are a substantial number of changes between Greenplum 6 and Greenplum 7 that could potentially affect your existing application when you move to Greenplum 7. Before going any further, familiarize yourself with all of these changes [Key Changes between Greenplum 6 and Greenplum 7](./changes-6-7-landing-page.html).

To begin, choose one of these two paths:

[Upgrading to Greenplum 7 on the Same Hardware](#upgrading-to-greenplum-7-on-the-same-hardware)
[Upgrading to Greenplum 7 While Moving to New Hardware](#upgrading-to-greenplum-7-while-moving-to-new-hardware)

## <a id="same_hardware"></a>Upgrading to Greenplum 7 On the Same Hardware

Follow the steps below to upgrade to Greenplum 7 while remaining on the same hardware. You will move your data using the `gpbackup/gprestore` utilities. There are a number of caveats with respect to backing up and restoring your data. See [Known Issues with Restoring a Greenplum 6 Backup to a Greenplum 7 Cluster](#known-issues-with-restoring-a-greenplum-6-backup-to-a-greenplum-7-cluster) for details.

>**Note**
>When installing VMware Greenplum 7 on the same hardware as your 6 system, you will need enough disk space to accommodate over five times the original data set (two full copies of the primary and mirror data sets, plus the original backup data in ASCII format) in order to migrate data with `gpbackup` and `gprestore`. Keep in mind that the ASCII backup data will require more disk space than the original data, which may be stored in compressed binary format. Offline backup solutions such as Dell EMC Data Domain can reduce the required disk space on each host.

To upgrade while staying on the same hardware:

1. If not already installed, install the latest release of the Greenplum Backup and Restore utilities, available to download from [VMware Tanzu Network](https://network.pivotal.io/products/greenplum-backup-restore) or [github](https://github.com/greenplum-db/gpbackup/releases).

2. Run the `gpbackup` utility to back up your data to an external data source. For more infomation on `gpbackup`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

3. Delete the Greenplum 6 cluster by issuing the [`gpdeletesystem` command](../utility_guide/ref/gpdeletesystem.html).

4. Initialize a Greenplum 7 cluster by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

5. Run the `gprestore` utility to restore your data to the Greenplum 7 cluster from the external data source. For more infomation on `gprestore`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

>**Note** 
>`gprestore` only supports restoring data to a cluster that has an identical number of hosts and an identical number of segments per host, with each segment having the same `content_id` as the segment in the original cluster. 

## <a id="new_hardware"></a>Upgrading to Greenplum 7 While Moving to New Hardware

Follow the steps below to upgrade to Greenplum 7 while moving to new hardware. 

If you are upgrading from Greenplum 6 to Greenplum 7 and changing hardware you may move your data in one of two ways:

- By using the `gpbackup/gprestore` utilities
- By using the `gpcopy` utility

There are a number of caveats with respect to backing up and restoring your data. See [Backup and Restore Caveats](#backup-and-restore-caveats) for details.

To upgrade while moving to new hardware using `gpbackup/gprestore`:

1. If not already installed, install the latest release of the Greenplum Backup and Restore utilities, available to download from [VMware Tanzu Network](https://network.pivotal.io/products/greenplum-backup-restore) or [github](https://github.com/greenplum-db/gpbackup/releases).

2. Run the `gpbackup` utility to back up the date from your Greenplum 6 cluster to an external data source. For more infomation on `gpbackup`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

3. Initalize a Greenplum 7 cluster on the new hardware, by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

4. Run the `gprestore` utility to restore your data to the Greenplum 7 cluster from the external data source. For more infomation on `gprestore`, see the [VMware Greenplum Backup and Restore guide](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/backup-restore.html).

>**Note** 
>`gprestore` only supports restoring data to a cluster that has an identical number of hosts and an identical number of segments per host, with each segment having the same `content_id` as the segment in the original cluster. 

To upgrade while moving to new hardware using `gpcopy`:

1. Review the information in [Migrating Data with gpcopy](https://docs.vmware.com/en/VMware-Greenplum-Data-Copy-Utility/2.6/greenplum-copy/gpcopy-migrate.html).

2. Initalize a Greenplum 7 cluster on the new hardware, by issuing the [`gpinitsystem` command](../utility_guide/ref/gpinitsystem.html).

3. Verify network connectivity between your Greenplum 6 and your Greenplum 7 cluster. 

4. Run the `gpcopy` utility to migrate your data to the Greenplum 7 cluster. 




## <a id="br-caveats"></a>Known Issues with Restoring a Greenplum 6 Backup to a Greenplum 7 Cluster

There are a number of known issues when restoring a Greenplum 6 Backup to a Greenplum 7 cluster:

- 
-
-







**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)