---
title: Migrating Data with gpcopy 
---

You can use the `gpcopy` utility to transfer data between databases in different Greenplum Database clusters.

> **Note** `gpcopy` is available only with the commercial release of VMware Greenplum.

`gpcopy` is a high-performance utility that can copy metadata and data from one Greenplum database to another Greenplum database. You can migrate the entire contents of a database, or just selected tables. The clusters can have different Greenplum Database versions. For example, you can use `gpcopy` to migrate data from a Greenplum Database version 4.3.26 \(or later\) system to a 5.9 \(or later\) or a 6.x Greenplum system, or from a Greenplum Database version 5.9+ system to a Greenplum 6.x system.

> **Note** The `gpcopy` utility is available as a separate download for the commercial release of VMware Greenplum. See the [VMware Greenplum Data Copy Utility Documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Data-Copy-Utility/index.html).

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

