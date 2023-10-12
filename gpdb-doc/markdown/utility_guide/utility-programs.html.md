# Utility Reference 

The command-line utilities provided with Greenplum Database.

Greenplum Database uses the standard PostgreSQL client and server programs and provides additional management utilities for administering a distributed Greenplum Database DBMS.

Several utilities are installed when you install the Greenplum Database server. These utilities reside in `$GPHOME/bin`. Other utilities must be downloaded from VMware Tanzu Network and installed separately. These include:

-   The [VMware Greenplum Backup and Restore](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html) utilities.
-   The [VMware Greenplum Copy Utility](https://docs.vmware.com/en/VMware-Greenplum-Data-Copy-Utility/index.html).
-   The [VMware Greenplum Streaming Server](https://docs.vmware.com/en/VMware-Greenplum-Streaming-Server/index.html) utilities.

Additionally, the [VMware Greenplum Clients](/vmware/client_tool_guides/about.html) package is a separate download from VMware Tanzu Network that includes selected utilities from the Greenplum Database server installation that you can install on a client system.

Greenplum Database provides the following utility programs. Superscripts identify those utilities that require separate downloads, as well as those utilities that are also installed with the Client and Loader Tools Packages. \(See the Note following the table.\) All utilities are installed when you install the Greenplum Database server, unless specifically identified by a superscript.

- [analyzedb](ref/analyzedb.html)
- [clusterdb](ref/clusterdb.html)
- [createdb](ref/createdb.html)<sup>3</sup>
- [createuser](ref/createuser.html)<sup>3</sup>
- [dropdb](ref/dropdb.html)<sup>3</sup>
- [dropuser](ref/dropuser.html)<sup>3</sup>
- [gpactivatestandby](ref/gpactivatestandby.html)
- [gpaddmirrors](ref/gpaddmirrors.html)
- [gpbackup\_manager](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html)<sup>1</sup>
- [gpbackup](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html)<sup>1</sup>
- [gpcheckcat](ref/gpcheckcat.html)
- [gpcheckperf](ref/gpcheckperf.html)
- [gpconfig](ref/gpconfig.html)
- [gpcopy](ref/gpcopy.html)<sup>2</sup>
- [gpdeletesystem](ref/gpdeletesystem.html)
- [gpexpand](ref/gpexpand.html)
- [gpfdist](ref/gpfdist.html)<sup>3</sup>
- [gpinitstandby](ref/gpinitstandby.html)
- [gpinitsystem](ref/gpinitsystem.html)
- [gpkafka](https://docs.vmware.com/en/VMware-Greenplum-Streaming-Server/index.html)<sup>4</sup>
- [gpload](ref/gpload.html)<sup>3</sup>
- [gplogfilter](ref/gplogfilter.html)
- [gpmapreduce](ref/gpmapreduce.html)
- [gpmapreduce.yaml](ref/gpmapreduce-yaml.html)
- [gpmovemirrors](ref/gpmovemirrors.html)
- [gpmt](ref/gpmt.html)
- [gppkg](ref/gppkg.html)
- [gpdr](https://docs.vmware.com/en/VMware-Greenplum-Disaster-Recovery/1.0/greenplum-disaster-recovery/ref-gpdr.html)
- [gprecoverseg](ref/gprecoverseg.html)
- [gpreload](ref/gpreload.html)
- [gprestore](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html)<sup>1</sup>
- [gpscp](ref/gpscp.html)
- [gpss](https://docs.vmware.com/en/VMware-Greenplum-Streaming-Server/index.html)<sup>4</sup>
- [gpssh](ref/gpssh.html)
- [gpssh-exkeys](ref/gpssh-exkeys.html)
- [gpstart](ref/gpstart.html)
- [gpstate](ref/gpstate.html)
- [gpstop](ref/gpstop.html)
- [gpv](ref/gpv.html)
- [pg\_config](ref/pg_config.html)
- [pg\_dump](ref/pg_dump.html)<sup>3</sup>
- [pg\_dumpall](ref/pg_dumpall.html)<sup>3</sup>
- [pg\_restore](ref/pg_restore.html)
- [pgbouncer](ref/pgbouncer.html)
- [pgbouncer.ini](ref/pgbouncer-ini.html)
- [plcontainer](ref/plcontainer.html)
- [plcontainer Configuration File](ref/plcontainer-configuration.html)
- [psql](ref/psql.html)<sup>3</sup>
- [pxf](https://docs.vmware.com/en/VMware-Greenplum-Platform-Extension-Framework/6.6/greenplum-platform-extension-framework/ref-pxf.html)
- [pxf cluster](https://docs.vmware.com/en/VMware-Greenplum-Platform-Extension-Framework/6.6/greenplum-platform-extension-framework/ref-pxf-cluster.html)
- [reindexdb](ref/reindexdb.html)
- [vacuumdb](ref/vacuumdb.html)

> **Note** <sup>1</sup> The utility program can be obtained from the *Greenplum Backup and Restore* tile on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb-backup-restore).

> <sup>2</sup> The utility program can be obtained from the *Greenplum Data Copy Utility* tile on [VMware Tanzu Network](https://network.pivotal.io/products/gpdb-data-copy).

> <sup>3</sup> The utility program is also installed with the _Greenplum Client and Loader Tools Package_ for Linux and Windows. You can obtain these packages from the Greenplum Database _Greenplum Clients_ filegroup on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

> <sup>4</sup> The utility program is also installed with the _Greenplum Client and Loader Tools Package_ for Linux. You can obtain the most up-to-date version of the _Greenplum Streaming Server_ from [VMware Tanzu Network](https://network.pivotal.io/products/greenplum-streaming-server).

