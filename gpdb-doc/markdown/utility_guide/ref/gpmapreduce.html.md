# gpmapreduce 

Runs Greenplum MapReduce jobs as defined in a YAML specification document.

## <a id="section2"></a>Synopsis 

```
gpmapreduce -f <config.yaml> [dbname [<username>]] 
     [-k <name=value> | --key <name=value>] 
     [-h <hostname> | --host <hostname>] [-p <port>| --port <port>] 
     [-U <username> | --username <username>] [-W] [-v]

gpmapreduce -x | --explain 

gpmapreduce -X | --explain-analyze

gpmapreduce -V | --version 

gpmapreduce -h | --help 
```

## <a id="section3"></a>Requirements 

The following are required prior to running this program:

-   You must have your MapReduce job defined in a YAML file. See [gpmapreduce.yaml](gpmapreduce-yaml.html) for more information about the format of, and keywords supported in, the Greenplum MapReduce YAML configuration file.
-   You must be a Greenplum Database superuser to run MapReduce jobs written in untrusted Perl or Python.
-   You must be a Greenplum Database superuser to run MapReduce jobs with `EXEC` and `FILE` inputs.
-   You must be a Greenplum Database superuser to run MapReduce jobs with `GPFDIST` input unless the user has the appropriate rights granted.

## <a id="section4"></a>Description 

[MapReduce](https://en.wikipedia.org/wiki/MapReduce) is a programming model developed by Google for processing and generating large data sets on an array of commodity servers. Greenplum MapReduce allows programmers who are familiar with the MapReduce paradigm to write map and reduce functions and submit them to the Greenplum Database parallel engine for processing.

`gpmapreduce` is the Greenplum MapReduce program. You configure a Greenplum MapReduce job via a YAML-formatted configuration file that you pass to the program for execution by the Greenplum Database parallel engine. The Greenplum Database system distributes the input data, runs the program across a set of machines, handles machine failures, and manages the required inter-machine communication.

## <a id="section5"></a>Options 

-f config.yaml
:   Required. The YAML file that contains the Greenplum MapReduce job definitions. Refer to [gpmapreduce.yaml](gpmapreduce-yaml.html) for the format and content of the parameters that you specify in this file.

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
:   Specifies the host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   Specifies the TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name.

-W \| --password
:   Force a password prompt.

## <a id="section7"></a>Examples 

Run a MapReduce job as defined in `my_mrjob.yaml` and connect to the database `mydatabase`:

```
gpmapreduce -f my_mrjob.yaml mydatabase
```

## <a id="section8"></a>See Also 

[gpmapreduce.yaml](gpmapreduce-yaml.html)

