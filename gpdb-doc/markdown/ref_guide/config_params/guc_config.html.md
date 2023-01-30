# Server Configuration Parameters 

There are many Greenplum server configuration parameters that affect the behavior of the Greenplum Database system. Many of these configuration parameters have the same names, settings, and behaviors as in a regular PostgreSQL database system.

-   [Parameter Types and Values](#topic_vsn_22l_z4) describes the parameter data types and values.
-   [Setting Parameters](#topic_cyz_p2l_z4) describes limitations on who can change them and where or when they can be set.
-   [Parameter Categories](guc_category-list.html) organizes parameters by functionality.
-   [Configuration Parameters](guc-list.html) lists the parameter descriptions in alphabetic order.

## <a id="topic_vsn_22l_z4"></a>Parameter Types and Values 

All parameter names are case-insensitive. Every parameter takes a value of one of the following types: `Boolean`, `integer`, `floating point`, `enum`, or `string`.

Boolean values may be specified as `ON`, `OFF`, `TRUE`, `FALSE`, `YES`, `NO`, `1`, `0` \(all case-insensitive\).

Enum-type parameters are specified in the same manner as string parameters, but are restricted to a limited set of values. Enum parameter values are case-insensitive.

Some settings specify a memory size or time value. Each of these has an implicit unit, which is either kilobytes, blocks \(typically eight kilobytes\), milliseconds, seconds, or minutes. Valid memory size units are `kB` \(kilobytes\), `MB` \(megabytes\), and `GB` \(gigabytes\). Valid time units are `ms` \(milliseconds\), `s` \(seconds\), `min` \(minutes\), `h` \(hours\), and `d` \(days\). Note that the multiplier for memory units is 1024, not 1000. A valid time expression contains a number and a unit. When specifying a memory or time unit using the `SET` command, enclose the value in quotes. For example:

```
SET statement_mem TO '200MB';
```

> **Note** There is no space between the value and the unit names.

## <a id="topic_cyz_p2l_z4"></a>Setting Parameters 

Many of the configuration parameters have limitations on who can change them and where or when they can be set. For example, to change certain parameters, you must be a Greenplum Database superuser. Other parameters require a restart of the system for the changes to take effect. A parameter that is classified as *session* can be set at the system level \(in the `postgresql.conf` file\), at the database-level \(using `ALTER DATABASE`\), at the role-level \(using `ALTER ROLE`\), at the database- and role-level \(`ALTER ROLE...IN DATABASE...SET`, or at the session-level \(using `SET`\). System parameters can only be set in the `postgresql.conf` file.

In Greenplum Database, the coordinator and each segment instance has its own `postgresql.conf` file \(located in their respective data directories\). Some parameters are considered *local* parameters, meaning that each segment instance looks to its own `postgresql.conf` file to get the value of that parameter. You must set local parameters on every instance in the system \(coordinator and segments\). Others parameters are considered *coordinator* parameters. Coordinator parameters need only be set at the coordinator instance.

This table describes the values in the Settable Classifications column of the table in the description of a server configuration parameter.

|Set Classification|Description|
|------------------|-----------|
|coordinator or local|A *coordinator* parameter only needs to be set in the `postgresql.conf` file of the Greenplum coordinator instance. The value for this parameter is then either passed to \(or ignored by\) the segments at run time.<br/><br/>A local parameter must be set in the `postgresql.conf` file of the coordinator AND each segment instance. Each segment instance looks to its own configuration to get the value for the parameter. Local parameters always requires a system restart for changes to take effect.|
|session or system|*Session* parameters can be changed on the fly within a database session, and can have a hierarchy of settings: at the system level \(`postgresql.conf`\), at the database level \(`ALTER DATABASE...SET`\), at the role level \(`ALTER ROLE...SET`\), at the database and role level \(`ALTER ROLE...IN DATABASE...SET`\), or at the session level \(`SET`\). If the parameter is set at multiple levels, then the most granular setting takes precedence \(for example, session overrides database and role, database and role overrides role, role overrides database, and database overrides system\).<br/><br/>A *system* parameter can only be changed via the `postgresql.conf`file\(s\).|
|restart or reload|When changing parameter values in the postgresql.conf file\(s\), some require a *restart* of Greenplum Database for the change to take effect. Other parameter values can be refreshed by just reloading the server configuration file \(using `gpstop -u`\), and do not require stopping the system.|
|superuser|These session parameters can only be set by a database superuser. Regular database users cannot set this parameter.|
|read only|These parameters are not settable by database users or superusers. The current value of the parameter can be shown but not altered.|

