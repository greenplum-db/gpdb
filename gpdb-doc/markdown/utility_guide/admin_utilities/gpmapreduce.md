# gpmapreduce 

Runs Greenplum MapReduce jobs as defined in a YAML specification document.

## Synopsis 

```
gpmapreduce -f <yaml_file> [<dbname> [<username>]] 
     [-k <name=value> | --key <name=value>] 
     [-h <hostname> | --host <hostname>] [-p <port>| --port <port>] 
     [-U <username> | --username <username>] [-W] [-v]

gpmapreduce -x | --explain 

gpmapreduce -X | --explain-analyze

gpmapreduce -V | --version 

gpmapreduce -h | --help 
```

## Prerequisites 

The following are required prior to running this program:

-   You must have your MapReduce job defined in a YAML file. See the [Greenplum MapReduce Specification](../../ref_guide/yaml_spec.html) in the *Greenplum Database Reference Guide* for more information.
-   You must be a Greenplum Database superuser to run MapReduce jobs written in untrusted Perl or Python.
-   You must be a Greenplum Database superuser to run MapReduce jobs with `EXEC` and `FILE` inputs.
-   You must be a Greenplum Database superuser to run MapReduce jobs with `GPFDIST` input unless the the user has the appropriate rigths granted. See the *Greenplum Database Reference Guide* for more information.

## Description 

[MapReduce](https://en.wikipedia.org/wiki/MapReduce) is a programming model developed by Google for processing and generating large data sets on an array of commodity servers. Greenplum MapReduce allows programmers who are familiar with the MapReduce paradigm to write map and reduce functions and submit them to the Greenplum Database parallel engine for processing.

In order for Greenplum to be able to process MapReduce functions, the functions need to be defined in a YAML document, which is then passed to the Greenplum MapReduce program, `gpmapreduce`, for execution by the Greenplum Database parallel engine. The Greenplum system takes care of the details of distributing the input data, executing the program across a set of machines, handling machine failures, and managing the required inter-machine communication.

## Options 

-f yaml\_file
:   Required. The YAML file that contains the Greenplum MapReduce job definitions. See the *Greenplum Database Reference Guide*.

-? \| --help
:   Show help, then exit.

-V \| --version
:   Show version information, then exit.

-v \| --verbose
:   Show verbose output.

-x \| --explain
:   Do not run MapReduce jobs, but produce explain plans.

-X \| --explain-analyze
:   Run MapReduce jobs and produce explain-analyze plans.

-k \| --keyname=value
:   Sets a YAML variable. A value is required. Defaults to "key" if no variable name is specified.

**Connection Options**

-h host \| --host host
:   Specifies the host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   Specifies the TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name.

-W \| --password
:   Force a password prompt.

## Examples 

Run a MapReduce job as defined in `my_yaml.txt` and connect to the database `mydatabase`:

```
gpmapreduce -f my_yaml.txt mydatabase
```

## See Also 

Greenplum MapReduce specification in the *Greenplum Database Reference Guide*

