# gpmt gp_storage_rca_collector 

This tool collects storage-related artificats and generates a file which can be provided to VMware Customer Support for diagnosis of storage-related errors or system failures.

## <a id="usage"></a>Usage 

```
**gpmt** **gp\_storage_rca_collector** [-db <database> ] [-t <table> ] | -c <ID1,ID2,...> ] [-dir <PATH> ] [-a] [-translog ]
```

## <a id="opts"></a>Options 

-db
:   The database name.

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

-a
:   Answer Yes to all prompts.

-dir
:   Working directory \(defaults to current directory\).

-segdir
:   Segment temporary directory \(defaults to /tmp\).

-skip-master
:   When running gp\_log\_collector, the generated tarball can be very large. Use this option to skip Greenplum Master log collection when only Greenplum Segment logs are required.

-with-gptext
:   Collect all GPText logs along with the Greenplum logs.

-with-gptext-only
:   Only Collect GPText logs.

**Note**: Hostnames provided through `-hostfile` or `-h` must match the hostname column in`gp_segment_configuration`.

The tool also collects the following information:

|Source|Files and outputs|
|------|-----------------|
|Database parameters|-   `version`
-   `uptime`
-   `pg_db_role_setting`
-   `pg_resqueue`
-   `pg_resgroup_config`
-   `pg_database`
-   `gp_segment_configuration`
-   `gp_configuration_history`

|
|Segment servers parameters|-   `uname -a`
-   `sysctl -a`
-   `psaux`
-   `netstat -rn`
-   `netstat -i`
-   `lsof`
-   `ifconfig`
-   `free`
-   `df -h`

|
|System files from all hosts|-   `/etc/redhat-release`
-   `/etc/sysctl.conf`
-   `/etc/sysconfig/network`
-   `/etc/security/limits.conf`

|
|Database related files from all hosts|-   `$SEG_DIR/pg_hba.conf`
-   `$SEG_DIR/pg_log/`
-   `$SEG_DIRE/postgresql.conf`
-   `~/gpAdminLogs`

|
|GPText files|-   Installation configuration file `$GPTXTHOME/lib/python/gptextlib/consts.py`
-   `gptext-state -D`
-   `<gptext data dir>/solr*/solr.in`
-   `<gptext data dir>/solr*/log4j.properties`
-   `<gptext data dir>/zoo*/logs/*`
-   `commands/bash/-c_echo $PATH`
-   `commands/bash/-c_ps -ef | grep solr`
-   `commands/bash/-c_ps -ef | grep zookeeper`

|

Note: some commands might not be able to be run if user does not have enough permission.

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

