# gpmt 

GPMT \(Greenplum Magic Tool\) provides a set of diagnostic utilities to troubleshoot and resolve common supportability issues, along with a consistent method for gathering information required by VMware Support.

## Synopsis 

```
gpmt tool [<tool_options> ...] 
[<-hostfile FILE>] [<-help>] [<-verbose>]

```

## Tools 

[analyze\_session](gpmt-analyze_session.html)
:   Collect information from a hung Greenplum Database session for remote analysis.

catalogbackup
:   For VMware Support use only. Back up catalog prior to performing catalog repairs.

[gp\_log\_collector](gpmt-gp_log_collector.html)
:   Basic Greenplum Database log collection utility.

gpcheckcat
:   For VMware Support use only. Greenplum Database gpcheckcat log analysis.

gpcheckup
:   For VMware Support use only. Greenplum Database Health Check.

[gpstatscheck](gpmt-gpstatscheck.html)
:   Check for missing stats on objects used in a query.

[packcore](gpmt-packcore.html)
:   Package core files into single tarball for remote analysis.

primarymirror\_lengths
:   For VMware Support use only. Check whether primary and mirror AO and AOCO relfiles are the correct lengths.

tablecollect
:   For VMware Support use only. Collect data and index files for data corruption RCA.

## Options 

hostfile
:   Generate hostfiles for use with other tools.

replcheck
:   Check whether tool is replicated to all hosts.

replicate
:   Replicate tool to all hosts.

version
:   Display the GPMT version.

-hostfile
:   Limit the hosts where the tool will be run.

-help
:   Display the online help.

-verbose
:   Print verbose log messages.

