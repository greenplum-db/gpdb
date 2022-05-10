---
title: Greenplum Command Center Security 
---

Greenplum Command Center is a web-based application for monitoring and managing Greenplum clusters. Command Center works with data collected by agents running on the segment hosts and saved to the gpperfmon database. Installing Command Center creates the gpperfmon database and the `gpmon` database role if they do not already exist. It creates the `gpmetrics` schema in the gpperfmon database, which contains metrics and query history tables populated by the Greenplum Database metrics collector module.

**Note:** The `gpperfmon_install` utility also creates the gpperfmon database and `gpmon` role, but Command Center no longer requires the history tables it creates in the database. Do not use `gpperfmon_install` unless you need the old query history tables for some other purpose. `gpperfmon_install` enables the `gpmmon` and `gpsmon` agents, which add unnecessary load to the Greenplum Database system if you do not need the old history tables.

## <a id="gpmon"></a>The gpmon User 

The Command Center installer creates the `gpmon` database role and adds the role to the `pg_hba.conf` file with the following entries:

```
local    gpperfmon   gpmon         md5
host     all         gpmon         127.0.0.1/28    md5
host     all         gpmon         ::1/128         md5
```

These entries allow `gpmon` to establish a local socket connection to the gpperfmon database and a TCP/IP connection to any database.

The `gpmon` database role is a superuser. In a secure or production environment, it may be desirable to restrict the `gpmon` user to just the gpperfmon database. Do this by editing the `gpmon` host entry in the `pg_hba.conf` file and changing `all` in the database field to `gpperfmon`:

```
local   gpperfmon   gpmon                        md5
host    gpperfmon   gpmon    127.0.0.1/28        md5
host    gpperfmon   gpmon    ::1/128             md5
```

You must run `gpstop -u` to load the changes.

The password used to authenticate the `gpmon` user is stored in the `gpadmin` home directory in the `~/.pgpass` file. The `~/.pgpass` file must be owned by the `gpadmin` user and be RW-accessible only by the `gpadmin` user. The Command Center installer creates the `gpmon` role with the default password "changeme". Be sure to change the password immediately after you install Command Center. Use the `ALTER ROLE` command to change the password in the database, change the password in the `~/.pgpass` file, and then restart Command Center with the `gpcc start` command.

Because the `.pgpass` file contains the plain-text password of the `gpmon` user, you may want to remove it and supply the `gpmon` password using a more secure method. The `gpmon` password is needed when you run the `gpcc start`, `gpcc stop`, or `gpcc status` commands. You can add the `-W` option to the `gpcc` command to have the command prompt you to enter the password. Alternatively, you can set the `PGPASSWORD` environment variable to the gpmon password before you run the `gpcc` command.

Command Center does not allow logins from any role configured with trust authentication, including the `gpadmin` user.

The `gpmon` user can log in to the Command Center Console and has access to all of the application's features. You can allow other database roles access to Command Center so that you can secure the `gpmon` user and restrict other users' access to Command Center features. Setting up other Command Center users is described in the next section.

## <a id="gpccusers"></a>Greenplum Command Center Users 

To log in to the Command Center web application, a user must be allowed access to the gpperfmon database in `pg_hba.conf`. For example, to make `user1` a regular Command Center user, edit the `pg_hba.conf` file and either add or edit a line for the user so that the gpperfmon database is included in the database field. For example:

```
host     gpperfmon,accounts   user1     127.0.0.1/28    md5
```

The Command Center web application includes an Admin interface to add, remove, and edit entries in the `pg_hba.conf` file and reload the file into Greenplum Database.

Command Center has the following types of users:

-   *Self Only* users can view metrics and view and cancel their own queries. Any Greenplum Database user successfully authenticated through the Greenplum Database authentication system can access Greenplum Command Center with Self Only permission. Higher permission levels are required to view and cancel other’s queries and to access the System and Admin Control Center features.
-   *Basic* users can view metrics, view all queries, and cancel their own queries. Users with Basic permission are members of the Greenplum Database `gpcc_basic` group.
-   *Operator Basic* users can view metrics, view their own and others’ queries, cancel their own queries, and view the System and Admin screens. Users with Operator Basic permission are members of the Greenplum Database `gpcc_operator_basic` group.
-   *Operator* users can view their own and others’ queries, cancel their own and other’s queries, and view the System and Admin screens. Users with Operator permission are members of the Greenplum Database `gpcc_operator` group.
-   *Admin* users can access all views and capabilities in the Command Center. Greenplum Database users with the `SUPERUSER` privilege have Admin permissions in Command Center.

The Command Center web application has an Admin interface you can use to change a Command Center user's access level.

## <a id="sslgpcc"></a>Enabling SSL for Greenplum Command Center 

The Command Center web server can be configured to support SSL so that client connections are encrypted. To enable SSL, install a `.pem` file containing the web server's certificate and private key on the web server host and then enter the full path to the `.pem` file when prompted by the Command Center installer.

## <a id="kerbgpcc"></a>Enabling Kerberos Authentication for Greenplum Command Center Users 

If Kerberos authentication is enabled for Greenplum Database, Command Center users can also authenticate with Kerberos. Command Center supports three Kerberos authentication modes: *strict*, *normal*, and *gpmon-only*.

Strict
:   Command Center has a Kerberos keytab file containing the Command Center service principal and a principal for every Command Center user. If the principal in the client’s connection request is in the keytab file, the web server grants the client access and the web server connects to Greenplum Database using the client’s principal name. If the principal is not in the keytab file, the connection request fails.

Normal
:   The Command Center Kerberos keytab file contains the Command Center principal and may contain principals for Command Center users. If the principal in the client’s connection request is in Command Center’s keytab file, it uses the client’s principal for database connections. Otherwise, Command Center uses the `gpmon` user for database connections.

gpmon-only
:   The Command Center uses the `gpmon` database role for all Greenplum Database connections. No client principals are needed in the Command Center’s keytab file.

See the [Greenplum Command Center documentation](http://gpcc.docs.pivotal.io) for instructions to enable Kerberos authentication with Greenplum Command Center

**Parent topic:**[Greenplum Database Security Configuration Guide](../topics/preface.html)

