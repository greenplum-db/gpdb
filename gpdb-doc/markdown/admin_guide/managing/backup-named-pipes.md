# Using Named Pipes 

Greenplum Database allows the use of named pipes with `gpcrondump` and `gpdbrestore` to back up and restore Greenplum databases. When backing up a database with regular files, the files that contain the backup information are placed in directories on the Greenplum Database segments. If segment hosts do not have enough local disk space available to backup to files, you can use named pipes to back up to non-local storage, such as storage on another host on the network or to a backup appliance.

Backing up with named pipes is not supported if the `--ddboost` option is specified.

**To back up a Greenplum database using named pipes**

1.  Run the `gpcrondump` command with options `-K timestamp` and `--list-backup-files`.

    This creates two text files that contain the names of backup files, one per line. The file names include the `timestamp` you specified with the `-K timestamp` option and have the suffixes `_pipes` and `_regular_files`. For example:

    ```
    gp_dump_20150519160000_pipes 
    gp_dump_20150519160000_regular_files
    ```

    The file names listed in the `_pipes` file are to be created as named pipes. The file names in the `_regular_files` file should not be created as named pipes. `gpcrondump` and `gpdbrestore` use the information in these files during backup and restore operations.

2.  Create named pipes on all Greenplum Database segments using the file names in the generated `_pipes` file.
3.  Redirect the output of each named pipe to the destination process or file object.
4.  Run `gpcrondump` to back up the database using the named pipes.

    To create a complete set of Greenplum Database backup files, the files listed in the `_regular_files` file must also be backed up.


**To restore a database that used named pipes during backup**

1.  Direct the contents of each backup file to the input of its named pipe, for example `cat filename > pipename`, if the backup file is accessible as a local file object.
2.  Run the `gpdbrestore` command to restore the database using the named pipes.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

## Example 

This example shows how to back up a database over the network using named pipes and the netcat \(`nc`\) Linux command. The segments write backup files to the inputs of the named pipes. The outputs of the named pipes are piped through `nc` commands, which make the files available on TCP ports. Processes on other hosts can then connect to the segment hosts at the designated ports to receive the backup files. This example requires that the `nc` package is installed on all Greenplum hosts.

1.  Enter the following `gpcrondump` command to generate the lists of backup files for the `testdb` database in the `/backups` directory.

    ```
    $ gpcrondump -x testdb -K 20150519160000 --list-backup-files -u /backups
    
    ```

2.  View the files that `gpcrondump` created in the /backup directory:

    ```
    $ ls -lR /backups
    /backups:
    total 4
    drwxrwxr-x 3 gpadmin gpadmin 4096 May 19 21:49 db_dumps
    
    /backups/db_dumps:
    total 4
    drwxrwxr-x 2 gpadmin gpadmin 4096 May 19 21:49 20150519
    
    /backups/db_dumps/20150519:
    total 8
    -rw-rw-r-- 1 gpadmin gpadmin 256 May 19 21:49 gp_dump_20150519160000_pipes
    -rw-rw-r-- 1 gpadmin gpadmin 391 May 19 21:49 gp_dump_20150519160000_regular_files
    ```

3.  View the contents of the \_pipes file.

    ```
    $ cat /backups/db_dumps/20150519/gp_dump_20150519160000_pipes 
    sdw1:/backups/db_dumps/20150519/gp_dump_0_2_20150519160000.gz
    sdw2:/backups/db_dumps/20150519/gp_dump_1_3_20150519160000.gz
    mdw:/backups/db_dumps/20150519/gp_dump_-1_1_20150519160000.gz
    mdw:/backups/db_dumps/20150519/gp_dump_-1_1_20150519160000_post_data.gz
    
    ```

4.  Create the specified named pipes on the Greenplum Database segments. Also set up a reader for the named pipe.

    ```
    gpssh -h sdw1
    [sdw1] mkdir -p /backups/db_dumps/20150519/
    [sdw1] mkfifo /backups/db_dumps/20150519/gp_dump_0_2_20150519160000.gz
    [sdw1] cat /backups/db_dumps/20150519/gp_dump_0_2_20150519160000.gz | nc -l 21000 
    [sdw1] exit
    
    ```

    Complete these steps for each of the named pipes listed in the `_pipes` file. Be sure to choose an available TCP port for each file.

5.  On the destination hosts, receive the backup files with commands like the following:

    ```
    nc sdw1 21000 > gp_dump_0_2_20150519160000.gz
    
    ```

6.  Run `gpcrondump` to begin the backup:

    ```
    gpcrondump -x testdb -K 20150519160000 -u /backups
    
    ```


To restore a database with named pipes, reverse the direction of the backup files by sending the contents of the backup files to the inputs of the named pipes and run the `gpdbrestore` command:

```
gpdbrestore -x testdb -t 20150519160000 -u /backups

```

`gpdbrestore` reads from the named pipes' outputs.

