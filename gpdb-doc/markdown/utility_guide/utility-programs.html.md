---
title: Utility Reference 
---

The command-line utilities provided with Greenplum Database.

Greenplum Database uses the standard PostgreSQL client and server programs and provides additional management utilities for administering a distributed Greenplum Database DBMS.

Several utilities are installed when you install the Greenplum Database server. These utilities reside in `$GPHOME/bin`. Other utilities must be downloaded from VMware Tanzu Network and installed separately. These include:

-   The [Tanzu Greenplum Backup and Restore](http://gpdb.docs.pivotal.io/backup-restore/latest/index.html) utilities.
-   The [Tanzu Greenplum Copy Utility](https://gpdb.docs.pivotal.io/gpcopy/latest/index.html).
-   The [Tanzu Greenplum Streaming Server](http://greenplum.docs.pivotal.io/streaming-server/1-5/ref/gpss-ref.html) utilities.

Additionally, the [Tanzu Clients](../client_tool_guides/about.html) package is a separate download from VMware Tanzu Network that includes selected utilities from the Greenplum Database server installation that you can install on a client system.

Greenplum Database provides the following utility programs. Superscripts identify those utilities that require separate downloads, as well as those utilities that are also installed with the Client and Loader Tools Packages. \(See the Note following the table.\) All utilities are installed when you install the Greenplum Database server, unless specifically identified by a superscript.

|[analyzedb](ref/analyzedb.html)

 [clusterdb](ref/clusterdb.html)

 [createdb](ref/createdb.html)3

 [createuser](ref/createuser.html)3

 [dropdb](ref/dropdb.html)3

 [dropuser](ref/dropuser.html)3

 [gpactivatestandby](ref/gpactivatestandby.html)

 [gpaddmirrors](ref/gpaddmirrors.html)

 [gpbackup\_manager](ref/gpbackup_manager.html)1

 [gpbackup](ref/gpbackup.html)1

 [gpcheckcat](ref/gpcheckcat.html)

 [gpcheckperf](ref/gpcheckperf.html)

 [gpconfig](ref/gpconfig.html)

 [gpcopy](ref/gpcopy.html)2

|[gpdeletesystem](ref/gpdeletesystem.html)

 [gpexpand](ref/gpexpand.html)

 [gpfdist](ref/gpfdist.html)3

 [gpinitstandby](ref/gpinitstandby.html)

 [gpinitsystem](ref/gpinitsystem.html)

 [gpkafka](https://greenplum.docs.pivotal.io/streaming-server/1-5/ref/gpss-ref.html)4

 [gpload](ref/gpload.html)3

 [gplogfilter](ref/gplogfilter.html)

 [gpmapreduce](ref/gpmapreduce.html)

 [gpmapreduce.yaml](ref/gpmapreduce-yaml.html)

 [gpmovemirrors](ref/gpmovemirrors.html)

 [gpmt](ref/gpmt.html)

 [gppkg](ref/gppkg.html)

 [gprecoverseg](ref/gprecoverseg.html)

 [gpreload](ref/gpreload.html)

 [gprestore](ref/gprestore.html)1

 [gpscp](ref/gpscp.html)

 [gpss](https://greenplum.docs.pivotal.io/streaming-server/1-5/ref/gpss-ref.html)4

 [gpssh](ref/gpssh.html)

 [gpssh-exkeys](ref/gpssh-exkeys.html)

|[gpstart](ref/gpstart.html)

 [gpstate](ref/gpstate.html)

 [gpstop](ref/gpstop.html)

 [pg\_config](ref/pg_config.html)

 [pg\_dump](ref/pg_dump.html)3

 [pg\_dumpall](ref/pg_dumpall.html)3

 [pg\_restore](ref/pg_restore.html)

 [plcontainer](ref/plcontainer.html)

 [plcontainer Configuration File](ref/plcontainer-configuration.html)

 [psql](ref/psql.html)3

 [pxf](../../pxf/latest/ref/pxf.html)

 [pxf cluster](../../pxf/latest/ref/pxf-cluster.html)

 [reindexdb](ref/reindexdb.html)

 [vacuumdb](ref/vacuumdb.html)

|

**Note:**

1 The utility program can be obtained from the *Greenplum Backup and Restore* tile on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb-backup-restore).

2 The utility program can be obtained from the *Greenplum Data Copy Utility* tile on [VMware Tanzu Network](https://network.pivotal.io/products/gpdb-data-copy).

3 The utility program is also installed with the *Greenplum Client and Loader Tools Package*s for Linux and Windows. You can obtain these packages from the Greenplum Database *Greenplum Clients* filegroup on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

4The utility program is also installed with the *Greenplum Client and Loader Tools Package* for Linux. You can obtain the most up-to-date version of the *Greenplum Streaming Server* from [VMware Tanzu Network](https://network.pivotal.io/products/greenplum-streaming-server).

