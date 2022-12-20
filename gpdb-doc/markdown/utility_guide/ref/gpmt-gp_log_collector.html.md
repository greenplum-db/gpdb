# gpmt gp_log_collector 

This tool collects Greenplum and system log files, along with the relevant configuration parameters, and generates a file which can be provided to VMware Customer Support for diagnosis of errors or system failures.

## <a id="usage"></a>Usage 

```
gpmt gp_log_collector [-failed-segs | -c <ID1,ID2,...>| -hostfile <file> | -h <host1, host2,...>]
[ -start <YYYY-MM-DD> ] [ -end <YYYY-MM-DD> ]
[ -dir <path> ] [ -segdir <path> ] [ -a ] [-skip-master] [-with-gpbackup] [-with-gptext] [-with-gptext-only] [-with-gpcc] [-with-gpss] [-gpss_logdir <gpss_log_directory>] [-with-pxf] [-with-pxf-only] [-with-gpupgrade]
```

## <a id="opts"></a>Options 

-failed-segs
:   The tool scans `gp_configuration_history` to identify when a segment fails over to their mirrors or simply fails without explanation. The relevant content ID logs will be collected.

-free-space
:   Free space threshold which will exit log collection if reached. Default value is 10%.

-c
:   Comma separated list of content IDs to collect logs from.

-hostfile
:   Hostfile with a list of hostnames to collect logs from.

-h
:   Comma separated list of hostnames to collect logs from.

-start
:   Start date for logs to collect \(defaults to current date\).

-end
:   End date for logs to collect \(defaults to current date\).

-dir
:   Working directory \(defaults to current directory\).

-segdir
:   Segment temporary directory \(defaults to /tmp\).

-a
:   Answer Yes to all prompts.

-skip-master
:   When running `gp_log_collector`, the generated tarball can be very large. Use this option to skip Greenplum Master log collection when only Greenplum Segment logs are required.

-with-gpbackup 
:   Beginning with Greenplum 6.22, this option enables you to collect logs related to backup and restore. 

With this option, `gpmt` collects these log files from `$GPADMIN_HOME/gpAdminLogs`:

- `gpbackup_.log`
- `gpbackup_helper_.log`
- `gpbackup_ plugin .log`
- `gprestore_.log`

These are collected from the provided `--backup-dir` or default backup directory:

- `gpbackup__config.yaml`
- `gpbackup__metadata.sql`
- `gpbackup__report`
- `gpbackup__toc.yaml`
- `gprestore___report`
- `gpbackup___report`

Also, the `pg_log` file is collected from the master and segment hosts.

-with-gptext
:   Collect all GPText logs along with Greenplum logs.

-with-gptext-only
:   Collect only GPText logs.

-with-gpcc
:   Collect log files related to Greenplum Command Center. Log files are collected from the following locations:

- `$GPCC_HOME/logs/*`
- `$GPCC_HOME/conf/app.conf`
- `$HOME/gpmetrics/*` (Greenplum Command Center 6.8.0 and later on Greenplum Database 6.x or Greenplum Command Center 4.16.0 and later on Greenplum Database 5.x)
- `$MASTER_DATA_DIRECTORY/gpmetrics` (Greenplum Command Center 6.7.0 and earlier on Greenplum Database 6.x or Greenplum Command Center 4.15.0 and earlier on Greenplum Database 5.x)
- The output of the `gppkg -q --all` command

-with-gpss 
:  Collect log files related to Greenplum Streaming Server. If you do not specify a directory with the `-gpsslogdir` option, gpmt collects logs from the `gpAdminLogs` directory. Log files are of the format `gpss_<date>.log`.

-with-pxf
:   Collect all PXF logs along with Greenplum logs.

-with-pxf-only
:   Collect only PXF logs.

-with-gpupgrade
:   Collect all `gpupgrade` logs along with Greenplum logs.


**Note**: Hostnames provided through `-hostfile` or `-h` must match the hostname column in `gp_segment_configuration`.

The tool also collects the following information:

| Source | Files and outputs |
| ------ | ----------------- |
| Database parameters | <ul><li>`version`</li><li>`uptime`</li><li>`pg_resqueue`</li><li>`pg_resgroup_config`</li><li>`pg_database`</li><li>`gp_segment_configuration`</li><li>`gp_configuration_history`</li><li>Initialization timestamp</li></ul> |
| Segment server parameters | <ul><li>`uname -a`</li><li>`sysctl -a`</li><li>`psaux`</li><li>`netstat -rn`</li><li>`netstat -i`</li><li>`lsof`</li><li>`ifconfig`</li><li>`free`</li><li>`df -h`</li><li>`top`</li><li>`sar`</li></ul> |
| System files from all hosts | <ul><li>`/etc/redhat-release`</li><li>`/etc/sysctl.conf`</li><li>`/etc/sysconfig/network`</li><li>`/etc/security/limits.conf`</li><li>`/var/log/dmesg`</li></ul> |
| Database-related files from all hosts | <ul><li>`$SEG_DIR/pg_hba.conf`</li><li>`$SEG_DIR/pg_log/`</li><li>`$SEG_DIRE/postgresql.conf`</li><li>`~/gpAdminLogs`</li></ul> |
| GPText files | <ul><li>Installation configuration file: `$GPTXTHOME/lib/python/gptextlib/consts.py` </li><li>`gptext-state -D`</li><li>`<gptext data dir>/solr*/solr.in`</li><li>`<gptext data dir>/solr*/log4j.properties`</li><li>`<gptext data dir>/zoo*/logs/*`</li><li>`commands/bash/-c_echo $PATH`</li><li>`commands/bash/-c_ps -ef | grep solr`</li><li>`commands/bash/-c_ps -ef | grep zookeeper`</li></ul> |
| PXF files | <ul><li>`pxf cluster status`</li><li>`pxf status`</li><li>PXF version</li><li>`Logs/`</li><li>`CONF/`</li><li>`Run/`</li></ul> |
| gpupgrade files | <ul><li>`~/gpAdminLogs` on all hosts</li><li>`$HOME/gpupgrade` on master host</li><li>`$HOME/.gpupgrade` on all hosts</li><li>Source cluster's `pg_log` files located in `$MASTER_DATA_DIRECTORY/pg_log` on master host</li><li>Target cluster's `pg_log` files located in `$(gpupgrade config show --target-datadir)/pg_log` on master host</li><li>Target cluster's master data directory</li></ul> |

**NOTE**: Some commands might not be able to be run if user does not have the correct permissions.

## <a id="exs"></a>Examples 

Collect Greenplum master and segment logs listed in a hostfile from today:

```
gpmt gp_log_collector -hostfile ~/gpconfig/hostfile
```

Collect logs for any segments marked down from 21-03-2016 until today:

```
gpmt gp_log_collector -failed-segs -start 2016-03-21
```

Collect logs from host `sdw2.gpdb.local` between 2016-03-21 and 2016-03-23:

```
gpmt gp_log_collector -failed-segs -start 2016-03-21 -end 2016-03-21
```

Collect only GPText logs for all segments, without any Greenplum logs:

```
gpmt gp_log_collector -with-gptext-only
```

