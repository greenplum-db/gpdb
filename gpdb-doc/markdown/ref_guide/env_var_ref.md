# Greenplum Environment Variables 

This reference lists and describes the environment variables to set for Greenplum Database. Set these in your user's startup shell profile \(such as `~/.bashrc` or `~/.bash_profile`\), or in `/etc/profile` if you want to set them for all users.

-   [Required Environment Variables](#topic2)
-   [Optional Environment Variables](#topic7)

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## Required Environment Variables 

**Note:** `GPHOME`, `PATH` and `LD_LIBRARY_PATH` can be set by sourcing the `greenplum_path.sh` file from your Greenplum Database installation directory

### GPHOME 

This is the installed location of your Greenplum Database software. For example:

```
GPHOME=/usr/local/greenplum-db-<<version>>
export GPHOME
```

### PATH 

Your `PATH` environment variable should point to the location of the Greenplum Database `bin` directory. For example:

```
PATH=$GPHOME/bin:$PATH
export PATH
```

### LD\_LIBRARY\_PATH 

The `LD_LIBRARY_PATH` environment variable should point to the location of the Greenplum Database/PostgreSQL library files. For example:

```
LD_LIBRARY_PATH=$GPHOME/lib
export LD_LIBRARY_PATH
```

### MASTER\_DATA\_DIRECTORY 

This should point to the directory created by the gpinitsystem utility in the master data directory location. For example:

```
MASTER_DATA_DIRECTORY=/data/master/gpseg-1
export MASTER_DATA_DIRECTORY
```

## Optional Environment Variables 

The following are standard PostgreSQL environment variables, which are also recognized in Greenplum Database. You may want to add the connection-related environment variables to your profile for convenience, so you do not have to type so many options on the command line for client connections. Note that these environment variables should be set on the Greenplum Database master host only.

### PGAPPNAME 

The name of the application that is usually set by an application when it connects to the server. This name is displayed in the activity view and in log entries. The `PGAPPNAME` environmental variable behaves the same as the `application_name` connection parameter. The default value for `application_name` is `psql`. The name cannot be longer than 63 characters.

### PGDATABASE 

The name of the default database to use when connecting.

### PGHOST 

The Greenplum Database master host name.

### PGHOSTADDR 

The numeric IP address of the master host. This can be set instead of or in addition to `PGHOST` to avoid DNS lookup overhead.

### PGPASSWORD 

The password used if the server demands password authentication. Use of this environment variable is not recommended for security reasons \(some operating systems allow non-root users to see process environment variables via `ps`\). Instead consider using the `~/.pgpass` file.

### PGPASSFILE 

The name of the password file to use for lookups. If not set, it defaults to `~/.pgpass`. See the topic about [The Password File](https://www.postgresql.org/docs/8.3/static/libpq-pgpass.html) in the PostgreSQL documentation for more information.

### PGOPTIONS 

Sets additional configuration parameters for the Greenplum Database master server.

### PGPORT 

The port number of the Greenplum Database server on the master host. The default port is 5432.

### PGUSER 

The Greenplum Database user name used to connect.

### PGDATESTYLE 

Sets the default style of date/time representation for a session. \(Equivalent to `SET datestyle TO...`\)

### PGTZ 

Sets the default time zone for a session. \(Equivalent to `SET timezone TO...`\)

### PGCLIENTENCODING 

Sets the default client character set encoding for a session. \(Equivalent to `SET client_encoding TO...`\)

