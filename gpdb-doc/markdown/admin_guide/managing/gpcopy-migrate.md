# Migrating Data with gpcopy 

This topic describes how to use the `gpcopy` utility to transfer data between databases in different Greenplum Database clusters.

[gpcopy](../../utility_guide/admin_utilities/gpcopy.html) is a high-performance utility that can copy metadata and data from one Greenplum database to another Greenplum database. You can migrate the entire contents of a database, or just selected tables. The clusters can have different Greenplum Database versions. For example, you can use `gpcopy` to migrate data from Greenplum 5 to Greenplum 6.

**Note:** The `gpcopy` utility is available as a separate download for the commercial release of Tanzu Greenplum. See the [Tanzu Greenplum Data Copy Utility Documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Data-Copy-Utility/index.html).

**Parent topic:** [Migrating Data](../managing/migrating-data.html)

