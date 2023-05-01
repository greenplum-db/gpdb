# gpsupport 

The `gpsupport` tool provides a set of diagnostic utilities to troubleshoot and resolve common supportability issues, along with a consistent method for gathering information required by VMware Support.

## <a id="syn"></a>Synopsis 

```
gpsupport <tool> [<tool_options> ...] 

gpsupport -hostfile <file>

gpsupport -help

gpsupport -verbose

```

## <a id="tool"></a>Tools 

**Greenplum**

[analyze_session](gpsupport-analyze_session.html)
:   Collect information from a hung Greenplum Database session for remote analysis.

catalogbackup
:   For VMware Support use only. Back up catalog prior to performing catalog repairs.

[gp_log_collector](gpsupport-gp_log_collector.html)
:   Basic Greenplum Database log collection utility.

[storage_rca_collector](gpsupport-gp_storage_rca_collector.html.md)
:   Collect storage-related artifacts.

gpcheckcat
:   For VMware Support use only. Greenplum Database gpcheckcat log analysis.

gpcheckup
:   For VMware Support use only. Greenplum Database Health Check.

[gpstatscheck](gpsupport-gpstatscheck.html)
:   Check for missing stats on objects used in a query.

[packcore](gpsupport-packcore.html)
:   Package core files into single tarball for remote analysis.

primarymirror_lengths
:   For VMware Support use only. Check whether primary and mirror AO and AOCO relfiles are the correct lengths.

tablecollect
:   For VMware Support use only. Collect data and index files for data corruption RCA.

**Miscellaneous**

hostfile
:   Generate hostfiles for use with other tools.

replcheck
:   Check whether tool is replicated to all hosts.

replicate
:   Replicate tool to all hosts.

version
:   Display the gpsupport version.

## <a id="globopts"></a>Global Options 

-hostfile
:   Limit the hosts where the tool will be run.

-help
:   Display the online help.

-verbose
:   Print verbose log messages.

## <a id="exs"></a>Examples 

**Display gpsupport version**

```
gpsupport version
```

**Collect a core file**

```
gpsupport packcore -cmd collect -core core.1234
```

**Show help for a specific tool**

```
gpsupport gp_log_collector -help
```

