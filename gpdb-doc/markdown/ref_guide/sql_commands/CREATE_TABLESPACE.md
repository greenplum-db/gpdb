# CREATE TABLESPACE 

Defines a new tablespace.

## Synopsis 

``` {#sql_command_synopsis}
CREATE TABLESPACE <tablespace_name> [OWNER <username>] 
       FILESPACE <filespace_name>
```

## Description 

`CREATE TABLESPACE` registers a new tablespace for your Greenplum Database system. The tablespace name must be distinct from the name of any existing tablespace in the system.

A tablespace allows superusers to define an alternative location on the file system where the data files containing database objects \(such as tables and indexes\) may reside.

A user with appropriate privileges can pass a tablespace name to [CREATE DATABASE](CREATE_DATABASE.html), [CREATE TABLE](CREATE_TABLE.html), or [CREATE INDEX](CREATE_INDEX.html) to have the data files for these objects stored within the specified tablespace.

In Greenplum Database, there must be a file system location defined for the master, each primary segment, and each mirror segment in order for the tablespace to have a location to store its objects across an entire Greenplum system. This collection of file system locations is defined in a filespace object. A filespace must be defined before you can create a tablespace. See `gpfilespace` in the *Greenplum Database Utility Guide* for more information.

## Parameters 

tablespacename
:   The name of a tablespace to be created. The name cannot begin with `pg_` or `gp_`, as such names are reserved for system tablespaces.

OWNER username
:   The name of the user who will own the tablespace. If omitted, defaults to the user executing the command. Only superusers may create tablespaces, but they can assign ownership of tablespaces to non-superusers.

FILESPACE
:   The name of a Greenplum Database filespace that was defined using the `gpfilespace` management utility.

## Notes 

You must first create a filespace to be used by the tablespace. See `gpfilespace` in the *Greenplum Database Utility Guide* for more information.

Tablespaces are only supported on systems that support symbolic links.

`CREATE TABLESPACE` cannot be executed inside a transaction block.

## Examples 

Create a new tablespace by specifying the corresponding filespace to use:

```
CREATE TABLESPACE mytblspace FILESPACE myfilespace;
```

## Compatibility 

`CREATE TABLESPACE` is a Greenplum Database extension.

## See Also 

[CREATE DATABASE](CREATE_DATABASE.html), [CREATE TABLE](CREATE_TABLE.html), [CREATE INDEX](CREATE_INDEX.html), [DROP TABLESPACE](DROP_TABLESPACE.html), [ALTER TABLESPACE](ALTER_TABLESPACE.html), gpfilespace in the *Greenplum Database Utility Guide*

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

