---
title: Greenplum Database Client Applications 
---

Greenplum Database comes installed with a number of client utility applications located in the `$GPHOME/bin` directory of your Greenplum Database master host installation. The following are the most commonly used client utility applications:

|Name|Usage|
|----|-----|
|`createdb`|create a new database|
|`createuser`|define a new database role|
|`dropdb`|remove a database|
|`dropuser`|remove a role|
|`psql`|PostgreSQL interactive terminal|
|`reindexdb`|reindex a database|
|`vacuumdb`|garbage-collect and analyze a database|

When using these client applications, you must connect to a database through the Greenplum master instance. You will need to know the name of your target database, the host name and port number of the master, and what database user name to connect as. This information can be provided on the command-line using the options `-d`, `-h`, `-p`, and `-U` respectively. If an argument is found that does not belong to any option, it will be interpreted as the database name first.

All of these options have default values which will be used if the option is not specified. The default host is the local host. The default port number is 5432. The default user name is your OS system user name, as is the default database name. Note that OS user names and Greenplum Database user names are not necessarily the same.

If the default values are not correct, you can set the environment variables `PGDATABASE`, `PGHOST`, `PGPORT`, and `PGUSER` to the appropriate values, or use a `psql` `~/.pgpass` file to contain frequently-used passwords.

For information about Greenplum Database environment variables, see the *Greenplum Database Reference Guide*. For information about psql, see the *Greenplum Database Utility Guide*.

**Parent topic:**[Accessing the Database](../../access_db/topics/g-accessing-the-database.html)

