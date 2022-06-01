# Configuring Database Authorization 

Describes how to restrict authorization access to database data at the user level by using roles and permissions.

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

## <a id="topic_k35_qtx_kr"></a>Access Permissions and Roles 

Greenplum Database manages database access permissions using *roles*. The concept of roles subsumes the concepts of users and groups. A role can be a database user, a group, or both. Roles can own database objects \(for example, tables\) and can assign privileges on those objects to other roles to control access to the objects. Roles can be members of other roles, thus a member role can inherit the object privileges of its parent role.

Every Greenplum Database system contains a set of database roles \(users and groups\). Those roles are separate from the users and groups managed by the operating system on which the server runs. However, for convenience you may want to maintain a relationship between operating system user names and Greenplum Database role names, since many of the client applications use the current operating system user name as the default.

In Greenplum Database, users log in and connect through the master instance, which verifies their role and access privileges. The master then issues out commands to the segment instances behind the scenes using the currently logged in role.

Roles are defined at the system level, so they are valid for all databases in the system.

To bootstrap the Greenplum Database system, a freshly initialized system always contains one predefined superuser role \(also referred to as the system user\). This role will have the same name as the operating system user that initialized the Greenplum Database system. Customarily, this role is named `gpadmin`. To create more roles you first must connect as this initial role.

## <a id="managing_obj_priv"></a>Managing Object Privileges 

When an object \(table, view, sequence, database, function, language, schema, or tablespace\) is created, it is assigned an owner. The owner is normally the role that ran the creation statement. For most kinds of objects, the initial state is that only the owner \(or a superuser\) can do anything with the object. To allow other roles to use it, privileges must be granted. Greenplum Database supports the following privileges for each object type:

|Object Type|Privileges|
|:----------|:---------|
|Tables, Views, Sequences|`SELECT`<br/>`INSERT`<br/>`UPDATE`<br/>`DELETE`<br/>`RULE`<br/>`ALL`|
|External Tables|`SELECT`<br/>`RULE`<br/>`ALL`|
|Databases|`CONNECT`<br/>`CREATE`<br/>`TEMPORARY` or `TEMP`<br/>`ALL`|
|Functions|`EXECUTE`|
|Procedural Languages|`USAGE`|
|Schemas|`CREATE`<br/>`USAGE`<br/>`ALL`|

Privileges must be granted for each object individually. For example, granting `ALL` on a database does not grant full access to the objects within that database. It only grants all of the database-level privileges \(`CONNECT`, `CREATE`, `TEMPORARY`\) to the database itself.

Use the `GRANT` SQL command to give a specified role privileges on an object. For example:

```
=# GRANT INSERT ON mytable TO jsmith; 
```

To revoke privileges, use the `REVOKE` command. For example:

```
=# REVOKE ALL PRIVILEGES ON mytable FROM jsmith; 
```

You can also use the `DROP OWNED` and `REASSIGN OWNED` commands for managing objects owned by deprecated roles. \(Note: only an object's owner or a superuser can drop an object or reassign ownership.\) For example:

```
 =# REASSIGN OWNED BY sally TO bob;
 =# DROP OWNED BY visitor; 
```

## <a id="obj-access"></a>About Object Access Privileges

Greenplum Database access control corresponds roughly to the Orange Book 'C2' level of security, not the 'B1' level. Greenplum Database currently supports access privileges at the object level. Greenplum Database does not support row-level access or row-level, labeled security.

You can simulate row-level access by using views to restrict the rows that are selected. You can simulate row-level labels by adding an extra column to the table to store sensitivity information, and then using views to control row-level access based on this column. You can then grant roles access to the views rather than the base table. While these workarounds do not provide the same as "B1" level security, they may still be a viable alternative for many organizations.


## <a id="password-encryption"></a>About Password Encryption in Greenplum Database

The available password encryption methods in Greenplum Database are SCRAM-SHA-256, SHA-256, and MD5 \(the default\).

You can set your chosen encryption method system-wide or on a per-session basis.

### <a id="using-scram-256"></a>Using SCRAM-SHA-256 Password Encryption

To use SCRAM-SHA-256 password encryption, you must set a server configuration parameter either at the system or the session level. This section outlines how to use a server parameter to implement SCRAM-SHA-256 encrypted password storage.

Note that in order to use SCRAM-SHA-256 encryption for password storage, the `pg_hba.conf` client authentication method must be set to `scram-sha-256` rather than the default, `md5`.

### <a id="scram-system-wide"></a>Setting the SCRAM-SHA-256 Password Hash Algorithm System-wide

To set the `password_hash_algorithm` server parameter on a complete Greenplum system \(master and its segments\):

1.  Log in to your Greenplum Database instance as a superuser.
2.  Execute `gpconfig` with the `password_hash_algorithm` set to SCRAM-SHA-256:

    ```
    $ gpconfig -c password_hash_algorithm -v 'SCRAM-SHA-256' 
    ```

3.  Verify the setting:

    ```
    $ gpconfig -s
    ```

    You will see:

    ```
    Master value: SCRAM-SHA-256
    Segment value: SCRAM-SHA-256 
    ```


### <a id="scram-individual_session"></a>Setting the SCRAM-SHA-256 Password Hash Algorithm for an Individual Session

To set the `password_hash_algorithm` server parameter for an individual session:

1.  Log in to your Greenplum Database instance as a superuser.
2.  Set the `password_hash_algorithm` to SCRAM-SHA-256:

    ```
    # set password_hash_algorithm = 'SCRAM-SHA-256'
      
    ```

3.  Verify the setting:

    ```
    # show password_hash_algorithm;
    ```

    You will see:

    ```
    SCRAM-SHA-256 
    ```

### <a id="using-ssh-256"></a>Using SHA-256 Password Encryption

To use SHA-256 password encryption, you must set a server configuration parameter either at the system or the session level. This section outlines how to use a server parameter to implement SHA-256 encrypted password storage.

Note that in order to use SHA-256 encryption for password storage, the `pg_hba.conf` client authentication method must be set to `password` rather than the default, `md5`. \(See [Configuring the SSL Client Connection](Authenticate.md#config_ssl_client_conn) for more details.\) **With this authentication setting, the password is transmitted in clear text over the network; it is highly recommend that you set up SSL to encrypt the client server communication channel.**


#### <a id="system-wide"></a>Setting the SHA-256 Password Hash Algorithm System-wide

To set the `password_hash_algorithm` server parameter on a complete Greenplum system \(master and its segments\):

1.  Log in to your Greenplum Database instance as a superuser.
2.  Execute `gpconfig` with the `password_hash_algorithm` set to SHA-256:

    ```
    $ gpconfig -c password_hash_algorithm -v 'SHA-256' 
    ```

3.  Verify the setting:

    ```
    $ gpconfig -s
    ```

    You will see:

    ```
    Master value: SHA-256
    Segment value: SHA-256 
    ```


#### <a id="individual_session"></a>Setting the SHA-256 Password Hash Algorithm for an Individual Session

To set the `password_hash_algorithm` server parameter for an individual session:

1.  Log in to your Greenplum Database instance as a superuser.
2.  Set the `password_hash_algorithm` to SHA-256:

    ```
    # set password_hash_algorithm = 'SHA-256'
      
    ```

3.  Verify the setting:

    ```
    # show password_hash_algorithm;
    ```

    You will see:

    ```
    SHA-256 
    ```


#### <a id="sha256-example"></a>Example

An example of how to use and verify the `SHA-256` `password_hash_algorithm` follows:

1.  Log in as a super user and verify the password hash algorithm setting:

    ```
    SHOW password_hash_algorithm 
     password_hash_algorithm 
     ------------------------------- 
     SHA-256
    ```

2.  Create a new role with password that has login privileges.

    ```
    CREATE ROLE testdb WITH PASSWORD 'testdb12345#' LOGIN; 
    ```

3.  Change the client authentication method to allow for storage of SHA-256 encrypted passwords:

    Open the `pg_hba.conf` file on the master and add the following line:

    ```
    host all testdb 0.0.0.0/0 password
    ```

4.  Restart the cluster.
5.  Log in to the database as the user just created, `testdb`.

    ```
    psql -U testdb
    ```

6.  Enter the correct password at the prompt.
7.  Verify that the password is stored as a SHA-256 hash.

    Password hashes are stored in `pg_authid.rolpasswod`.

8.  Log in as the super user.
9.  Execute the following query:

    ```
    # SELECT rolpassword FROM pg_authid WHERE rolname = 'testdb';
    Rolpassword
    -----------
    sha256<64 hexadecimal characters>
    ```

## <a id="time-based-restriction"></a>Restricting Access by Time 

Greenplum Database enables the administrator to restrict access to certain times by role. Use the `CREATE ROLE` or `ALTER ROLE` commands to specify time-based constraints.

Access can be restricted by day or by day and time. The constraints are removable without deleting and recreating the role.

Time-based constraints only apply to the role to which they are assigned. If a role is a member of another role that contains a time constraint, the time constraint is not inherited.

Time-based constraints are enforced only during login. The `SET ROLE` and `SET SESSION AUTHORIZATION` commands are not affected by any time-based constraints.

Superuser or `CREATEROLE` privileges are required to set time-based constraints for a role. No one can add time-based constraints to a superuser.

There are two ways to add time-based constraints. Use the keyword `DENY` in the `CREATE ROLE` or `ALTER ROLE` command followed by one of the following.

-   A day, and optionally a time, when access is restricted. For example, no access on Wednesdays.
-   An interval—that is, a beginning and ending day and optional time—when access is restricted. For example, no access from Wednesday 10 p.m. through Thursday at 8 a.m.

You can specify more than one restriction; for example, no access Wednesdays at any time and no access on Fridays between 3:00 p.m. and 5:00 p.m. 

There are two ways to specify a day. Use the word `DAY` followed by either the English term for the weekday, in single quotation marks, or a number between 0 and 6, as shown in the table below.

|English Term|Number|
|:-----------|:-----|
|DAY 'Sunday'|DAY 0|
|DAY 'Monday'|DAY 1|
|DAY 'Tuesday'|DAY 2|
|DAY 'Wednesday'|DAY 3|
|DAY 'Thursday'|DAY 4|
|DAY 'Friday'|DAY 5|
|DAY 'Saturday'|DAY 6|

A time of day is specified in either 12- or 24-hour format. The word `TIME` is followed by the specification in single quotation marks. Only hours and minutes are specified and are separated by a colon \( : \). If using a 12-hour format, add `AM` or `PM` at the end. The following examples show various time specifications.

```
TIME '14:00'     # 24-hour time implied
TIME '02:00 PM'  # 12-hour time specified by PM 
TIME '02:00'     # 24-hour time implied. This is equivalent to TIME '02:00 AM'. 
```

**Important:** Time-based authentication is enforced with the server time. Timezones are disregarded.

To specify an interval of time during which access is denied, use two day/time specifications with the words `BETWEEN` and `AND`, as shown. `DAY` is always required.

```
BETWEEN DAY 'Monday' AND DAY 'Tuesday' 

BETWEEN DAY 'Monday' TIME '00:00' AND
        DAY 'Monday' TIME '01:00'

BETWEEN DAY 'Monday' TIME '12:00 AM' AND
        DAY 'Tuesday' TIME '02:00 AM'

BETWEEN DAY 'Monday' TIME '00:00' AND
        DAY 'Tuesday' TIME '02:00'
        DAY 2 TIME '02:00'
```

The last three statements are equivalent.

**Note:** Intervals of days cannot wrap past Saturday.

The following syntax is not correct:

```
DENY BETWEEN DAY 'Saturday' AND DAY 'Sunday'
```

The correct specification uses two DENY clauses, as follows:

```
DENY DAY 'Saturday'
DENY DAY 'Sunday'
```

The following examples demonstrate creating a role with time-based constraints and modifying a role to add time-based constraints. Only the statements needed for time-based constraints are shown. For more details on creating and altering roles see the descriptions of `CREATE ROLE` and `ALTER ROLE` in the *Greenplum Database Reference Guide*.

### <a id="ex1"></a>Example 1 – Create a New Role with Time-based Constraints 

No access is allowed on weekends.

```
 CREATE ROLE generaluser
 DENY DAY 'Saturday'
 DENY DAY 'Sunday'
 ... 
```

### <a id="ex2"></a>Example 2 – Alter a Role to Add Time-based Constraints 

No access is allowed every night between 2:00 a.m. and 4:00 a.m.

```
ALTER ROLE generaluser
 DENY BETWEEN DAY 'Monday' TIME '02:00' AND DAY 'Monday' TIME '04:00'
 DENY BETWEEN DAY 'Tuesday' TIME '02:00' AND DAY 'Tuesday' TIME '04:00'
 DENY BETWEEN DAY 'Wednesday' TIME '02:00' AND DAY 'Wednesday' TIME '04:00'
 DENY BETWEEN DAY 'Thursday' TIME '02:00' AND DAY 'Thursday' TIME '04:00'
 DENY BETWEEN DAY 'Friday' TIME '02:00' AND DAY 'Friday' TIME '04:00'
 DENY BETWEEN DAY 'Saturday' TIME '02:00' AND DAY 'Saturday' TIME '04:00'
 DENY BETWEEN DAY 'Sunday' TIME '02:00' AND DAY 'Sunday' TIME '04:00'
  ... 
```

### <a id="ex3"></a>Excample 3 – Alter a Role to Add Time-based Constraints 

No access is allowed Wednesdays or Fridays between 3:00 p.m. and 5:00 p.m.

```
ALTER ROLE generaluser
 DENY DAY 'Wednesday'
 DENY BETWEEN DAY 'Friday' TIME '15:00' AND DAY 'Friday' TIME '17:00'
 
```

## <a id="drop_timebased_restriction"></a>Dropping a Time-based Restriction 

To remove a time-based restriction, use the ALTER ROLE command. Enter the keywords DROP DENY FOR followed by a day/time specification to drop.

```
DROP DENY FOR DAY 'Sunday' 
```

Any constraint containing all or part of the conditions in a DROP clause is removed. For example, if an existing constraint denies access on Mondays and Tuesdays, and the DROP clause removes constraints for Mondays, the existing constraint is completely dropped. The DROP clause completely removes all constraints that overlap with the constraint in the drop clause. The overlapping constraints are completely removed even if they contain more restrictions that the restrictions mentioned in the DROP clause.

Example 1 - Remove a Time-based Restriction from a Role

```
 ALTER ROLE generaluser
 DROP DENY FOR DAY 'Monday'
    ... 
```

This statement would remove all constraints that overlap with a Monday constraint for the role `generaluser` in Example 2, even if there are additional constraints.

