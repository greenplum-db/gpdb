# gpdeletesystem 

Deletes a Greenplum Database system that was initialized using `gpinitsystem`.

## <a id="section2"></a>Synopsis 

```
gpdeletesystem [-d <coordinator_data_directory>] [-B <parallel_processes>] 
   [-f] [-l <logfile_directory>] [-D]

gpdeletesystem -? 

gpdeletesystem -v
```

## <a id="section3"></a>Description 

The `gpdeletesystem` utility performs the following actions:

-   Stop all `postgres` processes \(the segment instances and coordinator instance\).
-   Deletes all data directories.

Before running `gpdeletesystem`:

-   Move any backup files out of the coordinator and segment data directories.
-   Make sure that Greenplum Database is running.
-   If you are currently in a segment data directory, change directory to another location. The utility fails with an error when run from within a segment data directory.

This utility will not uninstall the Greenplum Database software.

## <a id="section4"></a>Options 

-d coordinator\_data\_directory
:   Specifies the coordinator host data directory. If this option is not specified, the setting for the environment variable `COORDINATOR_DATA_DIRECTORY` is used. If this option is specified, it overrides any setting of `COORDINATOR_DATA_DIRECTORY`. If coordinator\_data\_directory cannot be determined, the utility returns an error.

-B parallel\_processes
:   The number of segments to delete in parallel. If not specified, the utility will start up to 60 parallel processes depending on how many segment instances it needs to delete.

-f \(force\)
:   Force a delete even if backup files are found in the data directories. The default is to not delete Greenplum Database instances if backup files are present.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-D \(debug\)
:   Sets logging level to debug.

-? \(help\)
:   Displays the online help.

-v \(show utility version\)
:   Displays the version, status, last updated date, and check sum of this utility.

## <a id="section5"></a>Examples 

Delete a Greenplum Database system:

```
gpdeletesystem -d /gpdata/gp-1
```

Delete a Greenplum Database system even if backup files are present:

```
gpdeletesystem -d /gpdata/gp-1 -f
```

## <a id="seealso"></a>See Also 

[gpinitsystem](gpinitsystem.html)

