# createuser 

Creates a new database role.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
createuser [<connection-option> ...] [<role_attribute> ...] [-e] <role_name>

createuser -? | --help 

createuser -V | --version
```

## <a id="section3"></a>Description 

`createuser` creates a new Greenplum Database role. You must be a superuser or have the `CREATEROLE` privilege to create new roles. You must connect to the database as a superuser to create new superusers.

Superusers can bypass all access permission checks within the database, so superuser privileges should not be granted lightly.

`createuser` is a wrapper around the SQL command `CREATE ROLE`.

## <a id="section4"></a>Options 

role\_name
:   The name of the role to be created. This name must be different from all existing roles in this Greenplum Database installation.

-c number \| --connection-limit=number
:   Set a maximum number of connections for the new role. The default is to set no limit.

-d \| --createdb
:   The new role will be allowed to create databases.

-D \| --no-createdb
:   The new role will not be allowed to create databases. This is the default.

-e \| --echo
:   Echo the commands that `createuser` generates and sends to the server.

-E \| --encrypted
:   Encrypts the role's password stored in the database. If not specified, the default password behavior is used.

-i \| --inherit
:   The new role will automatically inherit privileges of roles it is a member of. This is the default.

-I \| --no-inherit
:   The new role will not automatically inherit privileges of roles it is a member of.

--interactive
:   Prompt for the user name if none is specified on the command line, and also prompt for whichever of the options `-d`/`-D`, `-r`/`-R`, `-s`/`-S` is not specified on the command line.

-l \| --login
:   The new role will be allowed to log in to Greenplum Database. This is the default.

-L \| --no-login
:   The new role will not be allowed to log in \(a group-level role\).

-N \| --unencrypted
:   Does not encrypt the role's password stored in the database. If not specified, the default password behavior is used.

-P \| --pwprompt
:   If given, `createuser` will issue a prompt for the password of the new role. This is not necessary if you do not plan on using password authentication.

-r \| --createrole
:   The new role will be allowed to create new roles \(`CREATEROLE` privilege\).

-R \| --no-createrole
:   The new role will not be allowed to create new roles. This is the default.

-s \| --superuser
:   The new role will be a superuser.

-S \| --no-superuser
:   The new role will not be a superuser. This is the default.

-V \| --version
:   Print the `createuser` version and exit.

--replication
:   The new user will have the `REPLICATION` privilege, which is described more fully in the documentation for [CREATE ROLE](../../ref_guide/sql_commands/CREATE_ROLE.html).

--no-replication
:   The new user will not have the `REPLICATION` privilege, which is described more fully in the documentation for [CREATE ROLE](../../ref_guide/sql_commands/CREATE_ROLE.html).

-? \| --help
:   Show help about `createuser` command line arguments, and exit.

**Connection Options**

-h host \| --host=host
:   The host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port=port
:   The TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username=username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

## <a id="section6"></a>Examples 

To create a role `joe` on the default database server:

```
$ createuser joe
```

To create a role `joe` on the default database server with prompting for some additional attributes:

```
$ createuser --interactive joe
Shall the new role be a superuser? (y/n) **n**
Shall the new role be allowed to create databases? (y/n) **n**
Shall the new role be allowed to create more new roles? (y/n) **n**
CREATE ROLE
```

To create the same role `joe` using connection options, with attributes explicitly specified, and taking a look at the underlying command:

```
createuser -h coordinatorhost -p 54321 -S -D -R -e joe
CREATE ROLE joe NOSUPERUSER NOCREATEDB NOCREATEROLE INHERIT 
LOGIN;
CREATE ROLE
```

To create the role `joe` as a superuser, and assign password `admin123` immediately:

```
createuser -P -s -e joe
Enter password for new role: admin123
Enter it again: admin123
CREATE ROLE joe PASSWORD 'admin123' SUPERUSER CREATEDB 
CREATEROLE INHERIT LOGIN;
CREATE ROLE
```

In the above example, the new password is not actually echoed when typed, but we show what was typed for clarity. However the password will appear in the echoed command, as illustrated if the `-e` option is used.

## <a id="section7"></a>See Also 

[CREATE ROLE](../../ref_guide/sql_commands/CREATE_ROLE.html), [dropuser](dropuser.html)

