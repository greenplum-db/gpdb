# Migrating Data 

Greenplum Database provides utilities that you can use to migrate data between Greenplum Database systems.

The [gpcopy](../../utility_guide/admin_utilities/gpcopy.html) utility can migrate data between Greenplum Database systems running software version 5.9.0 or later. The utility can require less disk space to migrate between Greenplum Database systems that co-exist on the same hosts.

The `gpransfer` utility can migrate data between Greenplum Database installations running any 5.x version software.

**Note:** `gptransfer` is deprecated and will be removed in the next major release of Greenplum Database.

-   **[Migrating Data with gpcopy](../managing/gpcopy-migrate.html)**  
This topic describes how to use the `gpcopy` utility to transfer data between databases in different Greenplum Database clusters.
-   **[Migrating Data with gptransfer](../managing/gptransfer.html)**  
This topic describes how to use the `gptransfer` utility to transfer data between databases.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

