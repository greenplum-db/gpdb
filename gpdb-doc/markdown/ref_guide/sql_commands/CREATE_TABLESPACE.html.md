---
title: CREATE TABLESPACE 
---

Defines a new tablespace.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TABLESPACE <tablespace_name>
   [OWNER <user_name>]
   LOCATION '<directory>' 
   [WITH (content<ID_1>='<directory>'[, content<ID_2>='<directory>' ... ] [, <tablespace_option = value [, ... ] ])]
```

## <a id="section3"></a>Description 

`CREATE TABLESPACE` registers and configures a new tablespace for your Greenplum Database system. The tablespace name must be distinct from the name of any existing tablespace in the system. A tablespace is a Greenplum Database system object \(a global object\), you can use a tablespace from any database if you have appropriate privileges.

A tablespace allows superusers to define an alternative host file system location where the data files containing database objects \(such as tables and indexes\) reside.

A user with appropriate privileges can pass a tablespace name to [CREATE DATABASE](CREATE_DATABASE.html), [CREATE TABLE](CREATE_TABLE.html), or [CREATE INDEX](CREATE_INDEX.html) to have the data files for these objects stored within the specified tablespace.

In Greenplum Database, the file system location must exist on all hosts including the hosts running the master, standby mirror, each primary segment, and each mirror segment.

## <a id="section4"></a>Parameters 

tablespace\_name
:   The name of a tablespace to be created. The name cannot begin with `pg_` or `gp_`, as such names are reserved for system tablespaces.

user\_name
:   The name of the user who will own the tablespace. If omitted, defaults to the user running the command. Only superusers can create tablespaces, but they can assign ownership of tablespaces to non-superusers.

LOCATION 'directory'
:   The absolute path to the directory \(host system file location\) that will be the root directory for the tablespace. When registering a tablepace, the directory should be empty and must be owned by the Greenplum Database system user. The directory must be specified by an absolute path name of no more than 100 characters. \(The location is used to create a symlink target in the pg\_tblspc directory, and symlink targets are truncated to 100 characters when sending to `tar` from utilities such as `pg_basebackup`.\)

:   For each segment instance, you can specify a different directory for the tablespace in the `WITH` clause.

contentID\_i='directory\_i'
:   The value ID\_i is the content ID for the segment instance. directory\_i is the absolute path to the host system file location that the segment instance uses as the root directory for the tablespace. You cannot specify the content ID of the master instance \(`-1`\). You can specify the same directory for multiple segments.

:   If a segment instance is not listed in the `WITH` clause, Greenplum Database uses the tablespace directory specified in the `LOCATION` clause.

:   The restrictions identified for the `LOCATION` directory also hold for directory\_i.

tablespace\_option
:   A tablespace parameter to set or reset. Currently, the only available parameters are seq\_page\_cost and random\_page\_cost. Setting either value for a particular tablespace will override the planner's usual estimate of the cost of reading pages from tables in that tablespace, as established by the configuration parameters of the same name \(see [seq_page_cost](../config_params/guc-list.html#seq_page_cost), [random_page_cost](../config_params/guc-list.html#random_page_cost)\). This may be useful if one tablespace is located on a disk which is faster or slower than the remainder of the I/O subsystem.

## <a id="section5"></a>Notes 

Because `CREATE TABLESPACE` creates symbolic links from the `pg_tblspc` directory in the master and segment instance data directory to the directories specified in the command, Greenplum Database supports tablespaces only on systems that support symbolic links.

`CREATE TABLESPACE` cannot be run inside a transaction block.

When creating tablespaces, ensure that file system locations have sufficient I/O speed and available disk space.

`CREATE TABLESPACE` creates symbolic links from the `pg_tblspc` directory in the master and segment instance data directory to the directories specified in the command.

The system catalog table `pg_tablespace` stores tablespace information. This command displays the tablespace OID values, names, and owner.

```
SELECT oid, spcname, spcowner FROM pg_tablespace;
```

The Greenplum Database built-in function `gp_tablespace_location(tablespace\_oid)` displays the tablespace host system file locations for all segment instances. This command lists the segment database IDs and host system file locations for the tablespace with OID `16385`.

```
SELECT * FROM gp_tablespace_location(16385) 
```

**Note:** Greenplum Database does not support different tablespace locations for a primary-mirror pair with the same content ID. It is only possible to configure different locations for different content IDs. Do not modify symbolic links under the `pg_tblspc` directory so that primary-mirror pairs point to different file locations; this will lead to erroneous behavior.

## <a id="section6"></a>Examples 

Create a new tablespace and specify the file system location for the master and all segment instances:

```
CREATE TABLESPACE mytblspace LOCATION '/gpdbtspc/mytestspace';
```

Create a new tablespace and specify a location for segment instances with content ID 0 and 1. For the master and segment instances not listed in the `WITH` clause, the file system location for the tablespace is specified in the `LOCATION` clause.

```
CREATE TABLESPACE mytblspace LOCATION '/gpdbtspc/mytestspace' WITH (content0='/temp/mytest', content1='/temp/mytest');
```

The example specifies the same location for the two segment instances. You can a specify different location for each segment.

## <a id="section7"></a>Compatibility 

`CREATE TABLESPACE` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE DATABASE](CREATE_DATABASE.html), [CREATE TABLE](CREATE_TABLE.html), [CREATE INDEX](CREATE_INDEX.html), [DROP TABLESPACE](DROP_TABLESPACE.html), [ALTER TABLESPACE](ALTER_TABLESPACE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

