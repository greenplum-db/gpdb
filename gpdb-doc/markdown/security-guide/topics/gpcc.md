# Greenplum Command Center Security 

Greenplum Command Center \(GPCC\) is a web-based application for monitoring and managing Greenplum clusters. GPCC works with data collected by agents running on the segment hosts and saved to the `gpperfmon` database. The `gpperfmon` database is created by running the `gpperfmon_install` utility, which also creates the `gpmon` database role that GPCC uses to access the `gpperfmon` database.

## The gpmon User 

The `gpperfmon_install` utility creates the `gpmon` database role and adds the role to the `pg_hba.conf` file with the following entries:

```
local    gpperfmon   gpmon         md5
host     all         gpmon         127.0.0.1/28    md5
host     all         gpmon         ::1/128         md5
```

These entries allow `gpmon` to establish a local socket connection to the `gpperfmon` database and a TCP/IP connection to any database.

The `gpmon` database role is a superuser. In a secure or production environment, it may be desirable to restrict the `gpmon` user to just the `gpperfmon` database. Do this by editing the `gpmon` host entry in the `pg_hba.conf` file and changing `all` in the database field to `gpperfmon`:

```
local   gpperfmon   gpmon                        md5
host    gpperfmon   gpmon    127.0.0.1/28        md5
host    gpperfmon   gpmon    ::1/128             md5
```

The password used to authenticate the `gpmon` user is set by the `gpperfmon_install` utility and is stored in the `gpadmin` home directory in the `~/.pgpass` file. The `~/.pgpass` file must be owned by the `gpadmin` user and be RW-accessible only by the `gpadmin` user. To change the `gpmon` password, use the `ALTER ROLE` command to change the password in the database, change the password in the `~/.pgpass` file, and then restart GPCC with the `gpcmdr --restart instance\_name` command.

**Note:** The GPCC web server can be configured to encrypt connections with SSL. Two-way authentication with public keys can also be enabled for GPCC users. However, the `gpmon` user always uses md5 authentication with the password saved in the `~/.pgpass` file.

GPCC does not allow logins from any role configured with trust authentication, including the `gpadmin` user.

The `gpmon` user can log in to the Command Center Console and has access to all of the application's features. You can allow other database roles access to GPCC so that you can secure the `gpmon` user and restrict other users' access to GPCC features. Setting up other GPCC users is described in the next section.

## Greenplum Command Center Users 

GPCC has the following types of users:

-   *Self Only* users can view metrics and view and cancel their own queries. Any Greenplum Database user successfully authenticated through the Greenplum Database authentication system can access Greenplum Command Center with Self Only permission. Higher permission levels are required to view and cancel others' queries and to access the System and Admin Control Center features.
-   *Basic* users can view metrics, view all queries, and cancel their own queries. Users with Basic permission are members of the Greenplum Database `gpcc_basic` group.
-   *Operator Basic* users can view metrics, view their own and others’ queries, cancel their own queries, and view the System and Admin screens. Users with Operator Basic permission are members of the Greenplum Database `gpcc_operator_basic` group.
-   *Operator* users can view their own and others’ queries, cancel their own and others' queries, and view the System and Admin screens. Users with Operator permission are members of the Greenplum Database `gpcc_operator` group.
-   *Admin* users can access all views and capabilities in the Command Center. Greenplum Database users with the `SUPERUSER` privilege have Admin permissions in Command Center.

To log in to the GPCC web application, a user must be allowed access to the `gpperfmon` database in `pg_hba.conf`. For example, to make `user1` a regular GPCC user, edit the `pg_hba.conf` file and either add or edit a line for the user so that the `gpperfmon` database is included in the database field. For example:

```
host     gpperfmon,accounts   user1     127.0.0.1/28    md5
```

To designate a user as an operator, grant the `gpcc_operator` role to the user:

```
=# GRANT gpcc_operator TO <user>;
```

You can also grant `gpcc_operator` to a group role to make all members of the group GPCC operators.

See the `gpperfmon_install` reference in *Greenplum Database Utility Guide* for more information about managing the `gpperfmon` database.

## Enabling SSL for Greenplum Command Center 

The GPCC web server can be configured to support SSL so that client connections are encrypted. A server certificate can be generated when the Command Center instance is created or you can supply an existing certificate.

Two-way authentication with public key encryption can also be enabled for GPCC. See the *Greenplum Command Center Administration Guide* for instructions.

## Enabling Kerberos Authentication for Greenplum Command Center Users 

If Kerberos authentication is enabled for Greenplum Database, Command Center users can also authenticate with Kerberos. Command Center supports three Kerberos authentication modes: *strict*, *normal*, and *gpmon-only*.

Strict
:   Command Center has a Kerberos keytab file containing the Command Center service principal and a principal for every Command Center user. If the principal in the client’s connection request is in the keytab file, the web server grants the client access and the web server connects to Greenplum Database using the client’s principal name. If the principal is not in the keytab file, the connection request fails.

Normal
:   The Command Center Kerberos keytab file contains the Command Center principal and may contain principals for Command Center users. If the principal in the client’s connection request is in Command Center’s keytab file, it uses the client’s principal for database connections. Otherwise, Command Center uses the `gpmon` user for database connections.

gpmon-only
:   The Command Center uses the `gpmon` database role for all Greenplum Database connections. No client principals are needed in the Command Center’s keytab file.

See the [Greenplum Command Center documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Text/3.9/tanzu-greenplum-text/GUID-welcome.html) for instructions to enable Kerberos authentication with Greenplum Command Center

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

