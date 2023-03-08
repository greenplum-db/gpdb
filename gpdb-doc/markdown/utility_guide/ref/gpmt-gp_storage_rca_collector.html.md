# storage_rca_collector 

This tool collects storage-related table data and generates an output file which can be provided to VMware Customer Support for diagnosis of storage-related errors or system failures.

## <a id="usage"></a>Usage 

```
storage_rca_collector [-db <database> ] [-t <table> ] | -c <ID1,ID2,...> ] [-dir <path> ] [-a] [-translog ]
```

## <a id="opts"></a>Options 

-db
:   The database name.

-t
:   The table name.

-c
:   Comma separated list of content IDs to collect logs from.

-dir
:   The output directory. Defaults to the current directory.

-a
:   Answer Yes to all prompts.

-translog
:   Specifies that the tool should collect transaction log data. 

The tool also collects the following information:

- Output from:

    - pg_class
    - pg_stat_last_operation
    - gp_distributed_log
    - pg_appendonly

- Data from transaction logs, if you call this tool with the `-translog` option

- AOCO data for a given appendonly, column-oriented table

- AO data for a given appendonly table

> **Note** some commands might not be able to be run if the user does not have correct permissions.

## <a id="exs"></a>Examples 

Collect storage root cause analysis artifacts only for coordinator, database `postgres`, and table `test_table`:

```
gpmt storage_rca_collector -db postgres -t test_table
```

Collect storage root cause analysis artifacts for primary segment with contentid [0,1], database `postgres`, and table `test_table`:

```
gpmt storage_rca_collector -db postgres -c 0,1 -t test_table
```

Collect storage root cause analysis artifacts for primary segment with contentid [0,1], database `postgres`, and table `test_table`, with prompt deactivated:

```
gpmt storage_rca_collector -db postgres -c 0,1 -t test_table -a
```

Collect storage rca artifacts for primary segment with contentid [0,1], database `postgres`, and table `test_table` and also collect transaction logs:

```
gpmt storage_rca_collector -db postgres -c 0,1 -t test_table -transLog
```

Collect storage rca artifacts for primary segment with contentid [0,1], database `postgres` and table `test_table` and output to a specified directory location.

```
gpmt storage_rca_collector -db postgres -c 0,1 -t test_table -dir <dir>
```

> **Note** Output files follow the naming convention \<database name>\_\<dbid>\_\<artifact name>.
