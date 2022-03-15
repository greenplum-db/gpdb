---
title: About Parallel Data Loading 
---

This topic provides a short introduction to Greenplum Database data loading features.

In a large scale, multi-terabyte data warehouse, large amounts of data must be loaded within a relatively small maintenance window. Greenplum supports fast, parallel data loading with its external tables feature. Administrators can also load external tables in single row error isolation mode to filter bad rows into a separate error log while continuing to load properly formatted rows. Administrators can specify an error threshold for a load operation to control how many improperly formatted rows cause Greenplum to cancel the load operation.

By using external tables in conjunction with Greenplum Database's parallel file server \(`gpfdist`\), administrators can achieve maximum parallelism and load bandwidth from their Greenplum Database system.

![](../graphics/ext_tables.jpg "External Tables Using Greenplum Parallel File Server (gpfdist)")

Another Greenplum utility, `gpload`, runs a load task that you specify in a YAML-formatted control file. You describe the source data locations, format, transformations required, participating hosts, database destinations, and other particulars in the control file and `gpload` runs the load. This allows you to describe a complex task and run it in a controlled, repeatable fashion.

**Parent topic:**[Greenplum Database Concepts](../intro/partI.html)

