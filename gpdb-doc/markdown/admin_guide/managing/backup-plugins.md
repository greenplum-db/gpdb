# Using gpbackup Storage Plugins 

You can configure the Greenplum Database [`gpbackup`](../../utility_guide/admin_utilities/gpbackup.html) and [`gprestore`](../../utility_guide/admin_utilities/gprestore.html) utilities to use a storage plugin to process backup files during a backup or restore operation. For example, during a backup operation, the plugin sends the backup files to a remote location. During a restore operation, the plugin retrieves the files from the remote location.

You can also develop a custom storage plugin with the Greenplum Database Backup/Restore Storage Plugin API \(Beta\). See [Backup/Restore Storage Plugin API \(Beta\)](backup-plugin-api.html).

**Note:** Only the Backup/Restore Storage Plugin API is a Beta feature. The storage plugins are supported features.

-   **[Using the S3 Storage Plugin with gpbackup and gprestore](../managing/backup-s3-plugin.html)**  


**Parent topic:** [Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

