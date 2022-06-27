# Creating and Managing Tablespaces 

Tablespaces allow database administrators to have multiple file systems per machine and decide how to best use physical storage to store database objects. They are named locations within a filespace in which you can create objects. Tablespaces allow you to assign different storage for frequently and infrequently used database objects or to control the I/O performance on certain database objects. For example, place frequently-used tables on file systems that use high performance solid-state drives \(SSD\), and place other tables on standard hard drives.

A tablespace requires a file system location to store its database files. In Greenplum Database, the master and each segment \(primary and mirror\) require a distinct storage location. The collection of file system locations for all components in a Greenplum system is a *filespace*. Filespaces can be used by one or more tablespaces.

**Parent topic:** [Defining Database Objects](../ddl/ddl.html)

## Creating a Filespace 

A filespace sets aside storage for your Greenplum system. A filespace is a symbolic storage identifier that maps onto a set of locations in your Greenplum hosts' file systems. To create a filespace, prepare the logical file systems on all of your Greenplum hosts, then use the `gpfilespace` utility to define the filespace. You must be a database superuser to create a filespace.

**Note:** Greenplum Database is not directly aware of the file system boundaries on your underlying systems. It stores files in the directories that you tell it to use. You cannot control the location on disk of individual files within a logical file system.

### To create a filespace using gpfilespace 

1.  Log in to the Greenplum Database master as the `gpadmin` user.

    ```
    $ su - `gpadmin`
    ```

2.  Create a filespace configuration file:

    ```
    $ gpfilespace -o gpfilespace_config
    ```

3.  At the prompt, enter a name for the filespace, the primary segment file system locations, the mirror segment file system locations, and a master file system location. Primary and mirror locations refer to directories on segment hosts; the master location refers to a directory on the master host and standby master, if configured. For example, if your configuration has 2 primary and 2 mirror segments per host:

    ```
    Enter a name for this filespace> fastdisk
    primary location 1> /gpfs1/seg1
    primary location 2> /gpfs1/seg2
    mirror location 1> /gpfs2/mir1
    mirror location 2> /gpfs2/mir2
    master location> /gpfs1/master
    
    ```

4.  `gpfilespace` creates a configuration file. Examine the file to verify that the `gpfilespace` configuration is correct.
5.  Run `gpfilespace` again to create the filespace based on the configuration file:

    ```
    $ gpfilespace -c gpfilespace_config
    ```


## Moving the Location of Temporary or Transaction Files 

You can move temporary or transaction files to a specific filespace to improve database performance when running queries, creating backups, and to store data more sequentially.

**Warning:** Do not move transaction files to a non-default location. Moving transaction files might cause data loss.

The dedicated filespace for temporary and transaction files is tracked in two separate flat files called `gp_temporary_files_filespace` and `gp_transaction_files_filespace`. These are located in the `pg_system` directory on each primary and mirror segment, and on master and standby. You must be a superuser to move temporary or transaction files. Only the `gpfilespace` utility can write to this file.

### About Temporary and Transaction Files 

Unless otherwise specified, temporary and transaction files are stored together with all user data. The default location of temporary files, *<filespace\_directory\>*/*<tablespace\_oid\>*/*<database\_oid\>*/pgsql\_tmp is changed when you use `gpfilespace --movetempfiles` for the first time.

Also note the following information about temporary or transaction files:

-   You can dedicate only one filespace for temporary or transaction files, although you can use the same filespace to store other types of files.
-   You cannot drop a filespace if it used by temporary files.
-   You must create the filespace in advance. See [Creating a Filespace](#topic10).

#### To move temporary files using gpfilespace 

1.  Check that the filespace exists and is different from the filespace used to store all other user data.
2.  Issue smart shutdown to bring the Greenplum Database offline.

    If any connections are still in progess, the `gpfilespace --movetempfiles` utility will fail.

3.  Bring Greenplum Database online with no active session and run the following command:

    ```
    gpfilespace --movetempfilespace filespace_name
    ```

    The location of the temporary files is stored in the segment configuration shared memory \(PMModuleState\) and used whenever temporary files are created, opened, or dropped.


## Creating a Tablespace 

After you create a filespace, use the `CREATE TABLESPACE` command to define a tablespace that uses that filespace. For example:

```
=# CREATE TABLESPACE fastspace FILESPACE fastdisk;

```

Database superusers define tablespaces and grant access to database users with the `GRANT CREATE` command. For example:

```
=# GRANT CREATE ON TABLESPACE fastspace TO admin;

```

## Using a Tablespace to Store Database Objects 

Users with the `CREATE` privilege on a tablespace can create database objects in that tablespace, such as tables, indexes, and databases. The command is:

```
CREATE TABLE tablename(options) TABLESPACE spacename

```

For example, the following command creates a table in the tablespace *space1*:

```
CREATE TABLE foo(i int) TABLESPACE space1;

```

You can also use the `default_tablespace` parameter to specify the default tablespace for `CREATE TABLE` and `CREATE INDEX` commands that do not specify a tablespace:

```
SET default_tablespace = space1;
CREATE TABLE foo(i int);

```

The tablespace associated with a database stores that database's system catalogs, temporary files created by server processes using that database, and is the default tablespace selected for tables and indexes created within the database, if no `TABLESPACE` is specified when the objects are created. If you do not specify a tablespace when you create a database, the database uses the same tablespace used by its template database.

You can use a tablespace from any database if you have appropriate privileges.

## Viewing Existing Tablespaces and Filespaces 

Every Greenplum Database system has the following default tablespaces.

-   `pg_global` for shared system catalogs.
-   `pg_default`, the default tablespace. Used by the *template1* and *template0* databases.

These tablespaces use the system default filespace, `pg_system`, the data directory location created at system initialization.

To see filespace information, look in the *pg\_filespace* and *pg\_filespace\_entry* catalog tables. You can join these tables with *pg\_tablespace* to see the full definition of a tablespace. For example:

```
=# SELECT spcname as tblspc, fsname as filespc, 
          fsedbid as seg_dbid, fselocation as datadir 
   FROM   pg_tablespace pgts, pg_filespace pgfs, 
          pg_filespace_entry pgfse 
   WHERE  pgts.spcfsoid=pgfse.fsefsoid 
          AND pgfse.fsefsoid=pgfs.oid 
   ORDER BY tblspc, seg_dbid;

```

## Dropping Tablespaces and Filespaces 

To drop a tablespace, you must be the tablespace owner or a superuser. You cannot drop a tablespace until all objects in all databases using the tablespace are removed.

Only a superuser can drop a filespace. A filespace cannot be dropped until all tablespaces using that filespace are removed.

The `DROP TABLESPACE` command removes an empty tablespace.

The `DROP FILESPACE` command removes an empty filespace.

**Note:** You cannot drop a filespace if it stores temporary or transaction files.

