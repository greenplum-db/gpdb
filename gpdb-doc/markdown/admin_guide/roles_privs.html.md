---
title: Managing Roles and Privileges 
---

The Greenplum Database authorization mechanism stores roles and permissions to access database objects in the database and is administered using SQL statements or command-line utilities.

Greenplum Database manages database access permissions using *roles*. The concept of roles subsumes the concepts of *users* and *groups*. A role can be a database user, a group, or both. Roles can own database objects \(for example, tables\) and can assign privileges on those objects to other roles to control access to the objects. Roles can be members of other roles, thus a member role can inherit the object privileges of its parent role.

Every Greenplum Database system contains a set of database roles \(users and groups\). Those roles are separate from the users and groups managed by the operating system on which the server runs. However, for convenience you may want to maintain a relationship between operating system user names and Greenplum Database role names, since many of the client applications use the current operating system user name as the default.

In Greenplum Database, users log in and connect through the master instance, which then verifies their role and access privileges. The master then issues commands to the segment instances behind the scenes as the currently logged in role.

Roles are defined at the system level, meaning they are valid for all databases in the system.

In order to bootstrap the Greenplum Database system, a freshly initialized system always contains one predefined *superuser* role \(also referred to as the system user\). This role will have the same name as the operating system user that initialized the Greenplum Database system. Customarily, this role is named `gpadmin`. In order to create more roles you first have to connect as this initial role.

**Parent topic:**[Managing Greenplum Database Access](partIII.html)

## <a id="topic2"></a>Security Best Practices for Roles and Privileges 

-   **Secure the gpadmin system user.** Greenplum requires a UNIX user id to install and initialize the Greenplum Database system. This system user is referred to as `gpadmin` in the Greenplum documentation. This `gpadmin` user is the default database superuser in Greenplum Database, as well as the file system owner of the Greenplum installation and its underlying data files. This default administrator account is fundamental to the design of Greenplum Database. The system cannot run without it, and there is no way to limit the access of this gpadmin user id. Use roles to manage who has access to the database for specific purposes. You should only use the `gpadmin` account for system maintenance tasks such as expansion and upgrade. Anyone who logs on to a Greenplum host as this user id can read, alter or delete any data; including system catalog data and database access rights. Therefore, it is very important to secure the gpadmin user id and only provide access to essential system administrators. Administrators should only log in to Greenplum as `gpadmin` when performing certain system maintenance tasks \(such as upgrade or expansion\). Database users should never log on as `gpadmin`, and ETL or production workloads should never run as `gpadmin`.
-   **Assign a distinct role to each user that logs in.** For logging and auditing purposes, each user that is allowed to log in to Greenplum Database should be given their own database role. For applications or web services, consider creating a distinct role for each application or service. See [Creating New Roles \(Users\)](#topic3).
-   **Use groups to manage access privileges.** See [Role Membership](#topic5).
-   **Limit users who have the SUPERUSER role attribute.** Roles that are superusers bypass all access privilege checks in Greenplum Database, as well as resource queuing. Only system administrators should be given superuser rights. See [Altering Role Attributes](#topic4).

## <a id="topic3"></a>Creating New Roles \(Users\) 

A user-level role is considered to be a database role that can log in to the database and initiate a database session. Therefore, when you create a new user-level role using the `CREATE ROLE` command, you must specify the `LOGIN` privilege. For example:

```
=# CREATE ROLE jsmith WITH LOGIN;

```

A database role may have a number of attributes that define what sort of tasks that role can perform in the database. You can set these attributes when you create the role, or later using the `ALTER ROLE` command. See [Table 1](#iq139556) for a description of the role attributes you can set.

### <a id="topic4"></a>Altering Role Attributes 

A database role may have a number of attributes that define what sort of tasks that role can perform in the database.

|Attributes|Description|
|----------|-----------|
|`SUPERUSER` or `NOSUPERUSER`|Determines if the role is a superuser. You must yourself be a superuser to create a new superuser. `NOSUPERUSER` is the default.|
|`CREATEDB` or `NOCREATEDB`|Determines if the role is allowed to create databases. `NOCREATEDB` is the default.|
|`CREATEROLE` or `NOCREATEROLE`|Determines if the role is allowed to create and manage other roles. `NOCREATEROLE` is the default.|
|`INHERIT` or `NOINHERIT`|Determines whether a role inherits the privileges of roles it is a member of. A role with the `INHERIT` attribute can automatically use whatever database privileges have been granted to all roles it is directly or indirectly a member of. `INHERIT` is the default.|
|`LOGIN` or `NOLOGIN`|Determines whether a role is allowed to log in. A role having the `LOGIN` attribute can be thought of as a user. Roles without this attribute are useful for managing database privileges \(groups\). `NOLOGIN` is the default.|
|`CONNECTION LIMIT *connlimit*`|If role can log in, this specifies how many concurrent connections the role can make. -1 \(the default\) means no limit.|
|`CREATEEXTTABLE` or `NOCREATEEXTTABLE`|Determines whether a role is allowed to create external tables. `NOCREATEEXTTABLE` is the default. For a role with the `CREATEEXTTABLE` attribute, the default external table `type` is `readable` and the default `protocol` is `gpfdist`. Note that external tables that use the `file` or `execute` protocols can only be created by superusers.|
|`PASSWORD '*password*'`|Sets the role's password. If you do not plan to use password authentication you can omit this option. If no password is specified, the password will be set to null and password authentication will always fail for that user. A null password can optionally be written explicitly as `PASSWORD NULL`.|
|`ENCRYPTED` or `UNENCRYPTED`|Controls whether a new password is stored as a hash string in the `pg_authid` system catalog. If neither `ENCRYPTED` nor `UNENCRYPTED` is specified, the default behavior is determined by the `password_encryption` configuration parameter, which is `on` by default. If the supplied `*password*` string is already in hashed format, it is stored as-is, regardless of whether `ENCRYPTED` or `UNENCRYPTED` is specified.

See [Protecting Passwords in Greenplum Database](#topic9) for additional information about protecting login passwords.

|
|`VALID UNTIL '*timestamp*'`|Sets a date and time after which the role's password is no longer valid. If omitted the password will be valid for all time.|
|`RESOURCE QUEUE *queue\_name*`|Assigns the role to the named resource queue for workload management. Any statement that role issues is then subject to the resource queue's limits. Note that the `RESOURCE QUEUE` attribute is not inherited; it must be set on each user-level \(`LOGIN`\) role.|
|`DENY {deny_interval | deny_point}`|Restricts access during an interval, specified by day or day and time. For more information see [Time-based Authentication](#topic13).|

You can set these attributes when you create the role, or later using the `ALTER ROLE` command. For example:

```
=# ALTER ROLE jsmith WITH PASSWORD 'passwd123';
=# ALTER ROLE admin VALID UNTIL 'infinity';
=# ALTER ROLE jsmith LOGIN;
=# ALTER ROLE jsmith RESOURCE QUEUE adhoc;
=# ALTER ROLE jsmith DENY DAY 'Sunday';
```

A role can also have role-specific defaults for many of the server configuration settings. For example, to set the default schema search path for a role:

```
=# ALTER ROLE admin SET search_path TO myschema, public;
```

## <a id="topic5"></a>Role Membership 

It is frequently convenient to group users together to ease management of object privileges: that way, privileges can be granted to, or revoked from, a group as a whole. In Greenplum Database this is done by creating a role that represents the group, and then granting membership in the group role to individual user roles.

Use the `CREATE ROLE` SQL command to create a new group role. For example:

```
=# CREATE ROLE admin CREATEROLE CREATEDB;

```

Once the group role exists, you can add and remove members \(user roles\) using the `GRANT` and `REVOKE` commands. For example:

```
=# GRANT admin TO john, sally;
=# REVOKE admin FROM bob;

```

For managing object privileges, you would then grant the appropriate permissions to the group-level role only \(see [Table 2](#iq139925)\). The member user roles then inherit the object privileges of the group role. For example:

```
=# GRANT ALL ON TABLE mytable TO admin;
=# GRANT ALL ON SCHEMA myschema TO admin;
=# GRANT ALL ON DATABASE mydb TO admin;

```

The role attributes `LOGIN`, `SUPERUSER`, `CREATEDB`, `CREATEROLE`, `CREATEEXTTABLE`, and `RESOURCE QUEUE` are never inherited as ordinary privileges on database objects are. User members must actually `SET ROLE` to a specific role having one of these attributes in order to make use of the attribute. In the above example, we gave `CREATEDB` and `CREATEROLE` to the `admin` role. If `sally` is a member of `admin`, she could issue the following command to assume the role attributes of the parent role:

```
=> SET ROLE admin;

```

## <a id="topic6"></a>Managing Object Privileges 

When an object \(table, view, sequence, database, function, language, schema, or tablespace\) is created, it is assigned an owner. The owner is normally the role that ran the creation statement. For most kinds of objects, the initial state is that only the owner \(or a superuser\) can do anything with the object. To allow other roles to use it, privileges must be granted. Greenplum Database supports the following privileges for each object type:

<table class="table" id="topic6__iq139925"><caption><span class="table--title-label">Table 2. </span><span class="title">Object Privileges</span></caption><colgroup><col style="width:66.66666666666666%"><col style="width:33.33333333333333%"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic6__iq139925__entry__1">Object Type</th>
              <th class="entry" id="topic6__iq139925__entry__2">Privileges</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Tables, External Tables, Views</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                <code class="ph codeph">SELECT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">INSERT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">UPDATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">DELETE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">REFERENCES</code>
                </p>
                <p class="p">
                  <code class="ph codeph">TRIGGER</code>
                </p>
                <p class="p">
                  <code class="ph codeph">TRUNCATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Columns</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">SELECT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">INSERT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">UPDATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">REFERENCES</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
                <td class="entry" headers="topic6__iq139925__entry__1">Sequences</td>
                <td class="entry" headers="topic6__iq139925__entry__2">
                  <p class="p">
                    <code class="ph codeph">USAGE</code>
                  </p>
                  <p class="p">
                    <code class="ph codeph">SELECT</code>
                  </p>
                  <p class="p">
                    <code class="ph codeph">UPDATE</code>
                  </p>
                  <p class="p">
                    <code class="ph codeph">ALL</code>
                  </p>
                </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Databases</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">CREATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">CONNECT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">TEMPORARY</code>
                </p>
                <p class="p">
                  <code class="ph codeph">TEMP</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Domains</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Foreign Data Wrappers</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Foreign Servers</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Functions</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                <code class="ph codeph">EXECUTE</code>
                </p>
                <p class="p">
                <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Procedural Languages</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Schemas</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                <code class="ph codeph">CREATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Tablespaces</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">CREATE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Types</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">USAGE</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic6__iq139925__entry__1">Protocols</td>
              <td class="entry" headers="topic6__iq139925__entry__2">
                <p class="p">
                  <code class="ph codeph">SELECT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">INSERT</code>
                </p>
                <p class="p">
                  <code class="ph codeph">ALL</code>
                </p>
              </td>
            </tr>
          </tbody></table>

**Note:** You must grant privileges for each object individually. For example, granting `ALL` on a database does not grant full access to the objects within that database. It only grants all of the database-level privileges \(`CONNECT`, `CREATE`, `TEMPORARY`\) to the database itself.

Use the `GRANT` SQL command to give a specified role privileges on an object. For example, to grant the role named `jsmith` insert privileges on the table named `mytable`:

```
=# GRANT INSERT ON mytable TO jsmith;
```

Similarly, to grant `jsmith` select privileges only to the column named `col1` in the table named `table2`:

```
=# GRANT SELECT (col1) on TABLE table2 TO jsmith;
```

To revoke privileges, use the `REVOKE` command. For example:

```
=# REVOKE ALL PRIVILEGES ON mytable FROM jsmith;

```

You can also use the `DROP OWNED` and `REASSIGN OWNED` commands for managing objects owned by deprecated roles \(Note: only an object's owner or a superuser can drop an object or reassign ownership\). For example:

```
=# REASSIGN OWNED BY sally TO bob;
=# DROP OWNED BY visitor;

```

### <a id="topic7"></a>Simulating Row Level Access Control 

Greenplum Database does not support row-level access or row-level, labeled security. You can simulate row-level access by using views to restrict the rows that are selected. You can simulate row-level labels by adding an extra column to the table to store sensitivity information, and then using views to control row-level access based on this column. You can then grant roles access to the views rather than to the base table.

## <a id="topic8"></a>Encrypting Data 

Greenplum Database is installed with an optional module of encryption/decryption functions called `pgcrypto`. The `pgcrypto` functions allow database administrators to store certain columns of data in encrypted form. This adds an extra layer of protection for sensitive data, as data stored in Greenplum Database in encrypted form cannot be read by anyone who does not have the encryption key, nor can it be read directly from the disks.

**Note:** The `pgcrypto` functions run inside the database server, which means that all the data and passwords move between `pgcrypto` and the client application in clear-text. For optimal security, consider also using SSL connections between the client and the Greenplum master server.

To use `pgcrypto` functions, register the `pgcrypto` extension in each database in which you want to use the functions. For example:

```
$ psql -d testdb -c "CREATE EXTENSION pgcrypto"
```

See [pgcrypto](https://www.postgresql.org/docs/9.4/pgcrypto.html) in the PostgreSQL documentation for more information about individual functions.

## <a id="topic9"></a>Protecting Passwords in Greenplum Database 

In its default configuration, Greenplum Database saves MD5 hashes of login users' passwords in the `pg_authid` system catalog rather than saving clear text passwords. Anyone who is able to view the `pg_authid` table can see hash strings, but no passwords. This also ensures that passwords are obscured when the database is dumped to backup files.

The hash function runs when the password is set by using any of the following commands:

-   `CREATE USER name WITH ENCRYPTED PASSWORD 'password'`
-   `CREATE ROLE name WITH LOGIN ENCRYPTED PASSWORD 'password'`
-   `ALTER USER name WITH ENCRYPTED PASSWORD 'password'`
-   `ALTER ROLE name WITH ENCRYPTED PASSWORD 'password'`

The `ENCRYPTED` keyword may be omitted when the `password_encryption` system configuration parameter is `on`, which is the default value. The `password_encryption` configuration parameter determines whether clear text or hashed passwords are saved when the `ENCRYPTED` or `UNENCRYPTED` keyword is not present in the command.

**Note:** The SQL command syntax and `password_encryption` configuration variable include the term *encrypt*, but the passwords are not technically encrypted. They are *hashed* and therefore cannot be decrypted.

The hash is calculated on the concatenated clear text password and role name. The MD5 hash produces a 32-byte hexadecimal string prefixed with the characters `md5`. The hashed password is saved in the `rolpassword` column of the `pg_authid` system table.

Although it is not recommended, passwords may be saved in clear text in the database by including the `UNENCRYPTED` keyword in the command or by setting the `password_encryption` configuration variable to `off`. Note that changing the configuration value has no effect on existing passwords, only newly created or updated passwords.

To set `password_encryption` globally, run these commands in a shell as the `gpadmin` user:

```
$ gpconfig -c password_encryption -v 'off'
$ gpstop -u
```

To set `password_encryption` in a session, use the SQL `SET` command:

```
=# SET password_encryption = 'on';
```

## <a id="topic13"></a>Time-based Authentication 

Greenplum Database enables the administrator to restrict access to certain times by role. Use the `CREATE ROLE` or `ALTER ROLE` commands to specify time-based constraints.

For details, refer to the *Greenplum Database Security Configuration Guide*.

