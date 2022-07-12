# Management Utility Reference 

Describes the command-line management utilities provided with Greenplum Database.

Greenplum Database uses the standard PostgreSQL client and server programs and provides additional management utilities for administering a distributed Greenplum Database DBMS. Greenplum Database management utilities reside in `$GPHOME/bin`.

**Note:** When referencing IPv6 addresses in `gpfdist` URLs or when using numeric IP addresses instead of hostnames in any management utility, always enclose the IP address in brackets. For command prompt use, the best practice is to escape any brackets or put them inside quotation marks. For example, use either:`\[2620:0:170:610::11\]` or `'[2620:0:170:610::11]'`.

Greenplum Database provides the following management utility programs:

 [analyzedb](analyzedb.html)

 [gpactivatestandby](gpactivatestandby.html)

 [gpaddmirrors](gpaddmirrors.html)

 [gpbackup](gpbackup.html)

 [gpcheck](gpcheck.html)

 [gpcheckcat](gpcheckcat.html)

 [gpcheckperf](gpcheckperf.html)

 [gpconfig](gpconfig.html)

 [gpcopy](gpcopy.html)

 [gpcrondump](gpcrondump.html)

 [gpdbrestore](gpdbrestore.html)

 [gpdeletesystem](gpdeletesystem.html)

 gp\_dump \(deprecated\)

 [gpexpand](gpexpand.html)

 [gpfdist](gpfdist.html)

 [gpfilespace](gpfilespace.html)

 [gpinitstandby](gpinitstandby.html)

 [gpinitsystem](gpinitsystem.html)

 [gpkafka](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Streaming-Server/1.7/greenplum-streaming-server/GUID-kafka-gpkafka.html)

 [gpkafka-v2.yaml](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Streaming-Server/1.7/greenplum-streaming-server/GUID-kafka-gpkafka-yaml-v2.html)

 [gpkafka.yaml](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Streaming-Server/1.7/greenplum-streaming-server/GUID-kafka-gpkafka-yaml.html)

 [gpload](gpload.html)

 [gplogfilter](gplogfilter.html)

|[gpmapreduce](gpmapreduce.html)

 [gpmfr](gpmfr.html)

 [gpmovemirrors](gpmovemirrors.html)

 [gpmt](gpmt.html)

 [gpperfmon\_install](gpperfmon_install.html)

 [gppkg](gppkg.html)

 [gprecoverseg](gprecoverseg.html)

 [gpreload](gpreload.html)

 [gprestore](gprestore.html)

 gp\_restore \(deprecated\)

 [gpscp](gpscp.html)

 [gpseginstall](gpseginstall.html)

 [gpssh](gpssh.html)

 [gpssh-exkeys](gpssh-exkeys.html)

 [gpstart](gpstart.html)

 [gpstate](gpstate.html)

 [gpstop](gpstop.html)

 [gpsys1](gpsys.html)

 [gptransfer](gptransfer.html) \(deprecated\)

 [pgbouncer](pgbouncer.html)

 [pgbouncer.ini](pgbouncer-ini.html)

 [pgbouncer-admin](pgbouncer-admin.html)

 [pxf](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Platform-Extension-Framework/6.3/tanzu-greenplum-platform-extension-framework/GUID-ref-pxf.html)

 [pxf cluster](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Platform-Extension-Framework/6.3/tanzu-greenplum-platform-extension-framework/GUID-ref-pxf-cluster.html)

## Backend Server Programs 

The following standard PostgreSQL server management programs are provided with Greenplum Database and reside in `$GPHOME/bin`. They are modified to handle the parallelism and distribution of a Greenplum Database system. You access these programs only through the Greenplum Database management tools and utilities.

|Program Name|Description|Use Instead|
|------------|-----------|-----------|
|`initdb`|This program is called by `gpinitsystem` when initializing a Greenplum Database array. It is used internally to create the individual segment instances and the master instance.|[gpinitsystem](gpinitsystem.html)|
|`ipcclean`|Not used in Greenplum Database|N/A|
|`pg_basebackup`|This program makes a binary copy of a single database instance. Greenplum Database uses it for tasks such as creating a standby master instance, or recovering a mirror segment when a full copy is needed. Do not use this utility to back up Greenplum Database segment instances because it does not produce MPP-consistent backups.|[gpinitstandby](gpinitstandby.html), [gprecoverseg](gprecoverseg.html)|
|`pg_controldata`|Not used in Greenplum Database|[gpstate](gpstate.html)|
|`pg_ctl`|This program is called by `gpstart` and `gpstop` when starting or stopping a Greenplum Database array. It is used internally to stop and start the individual segment instances and the master instance in parallel and with the correct options.|[gpstart](gpstart.html), [gpstop](gpstop.html)|
|`pg_resetxlog`|DO NOT USE **Warning:** This program might cause data loss or cause data to become unavailable. If this program is used, the Tanzu Greenplum cluster is not supported. The cluster must be reinitialized and restored by the customer.|N/A|
|`postgres`|The `postgres` executable is the actual PostgreSQL server process that processes queries.|The main `postgres` process \(postmaster\) creates other `postgres` subprocesses and `postgres` session as needed to handle client connections.|
|`postmaster`|`postmaster` starts the `postgres` database server listener process that accepts client connections. In Greenplum Database, a `postgres` database listener process runs on the Greenplum master Instance and on each Segment Instance.|In Greenplum Database, you use [gpstart](gpstart.html) and [gpstop](gpstop.html) to start all postmasters \(`postgres` processes\) in the system at once in the correct order and with the correct options.|

