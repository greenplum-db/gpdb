---
title: Replicating Backups 
---

You can use [gpbackup](../../utility_guide/ref/gpbackup.html) or [gpbackup\_manager](../../utility_guide/ref/gpbackup_manager.html) with the DD Boost storage plugin to replicate a backup from one Data Domain system to a second, remote, Data Domain system for disaster recovery. You can replicate a backup as part of the backup process, or replicate an existing backup set as a separate operation. Both methods require a [DD Boost configuration file](backup-ddboost-plugin.html#ddb-plugin-config) that includes options that specify Data Domain system locations and DD Boost configuration. The DD Boost storage plugin replicates the backup set on the remote Data Domain system with DD Boost managed file replication.

When replicating a backup, the Data Domain system where the backup is stored must have access to the remote Data Domain system where the replicated backup is stored.

To restore data from a replicated backup, use [gprestore](../../utility_guide/ref/gprestore.html) with the DD Boost storage plugin. In the configuration file, specify the location of the backup in the DD Boost configuration file.

For example configuration files, see [Examples](backup-ddboost-plugin.html#ddb_examples) in [Using the DD Boost Storage Plugin with gpbackup, gprestore, and gpbackup\_manager](backup-ddboost-plugin.html).

## <a id="repback"></a>Replicate a Backup as Part of the Backup Process 

Use the `gpbackup` utility to replicate a backup set as part of the backup process.

To enable replication during a back up, add the backup replication options to the configuration file. Set the configuration file `replication` option to `on` and add the options that the plugin uses to access the remote Data Domain system that stores the replicated backup.

When using `gpbackup`, the `replication` option must be set to `on`.

The configuration file `replication_streams` option is ignored, the default value is used.

Performing a backup operation with replication increases the time required to perform a backup. The backup set is copied to the local Data Domain system, and then replicated on the remote Data Domain system using DD Boost managed file replication. The backup operation completes after the backup set is replicated on the remote system.

## <a id="repexis"></a>Replicate an Existing Backup 

Use the `gpbackup_manager replicate-backup` command to replicate an existing backup set that is on a Data Domain system and was created by `gpbackup`.

When you run `backup_manager replicate-backup`, specify a DD Boost configuration file that contains the same type of information that is in the configuration file used to replicate a backup set with `gpbackup`.

When using the `gpbackup_manager replicate-backup` command, the configuration file `replication` option is ignored. The command always attempts to replicate a back up.

