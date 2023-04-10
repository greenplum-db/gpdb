---
title: PL/Java Language 
---

This section contains an overview of the Greenplum Database PL/Java language.

-   [About PL/Java](#topic2)
-   [About Greenplum Database PL/Java](#topic10)
-   [Installing Java](#topic_qx1_xcp_w3b)
-   [Installing PL/Java](#topic4)
-   [Enabling PL/Java and Installing JAR Files](#topic6)
-   [Uninstalling PL/Java](#topic7)
-   [Writing PL/Java functions](#topic13)
-   [Using JDBC](#topic24)
-   [Exception Handling](#topic25)
-   [Savepoints](#topic26)
-   [Logging](#topic27)
-   [Security](#topic28)
-   [Some PL/Java Issues and Solutions](#topic33)
-   [Example](#topic40)
-   [References](#topic41)

## <a id="topic2"></a>About PL/Java 

With the Greenplum Database PL/Java extension, you can write Java methods using your favorite Java IDE and install the JAR files that contain those methods into Greenplum Database.

Greenplum Database PL/Java package is based on the open source PL/Java 1.5.0. Greenplum Database PL/Java provides the following features.

-   Ability to run PL/Java functions with Java 8 or Java 11.
-   Ability to specify Java runtime.
-   Standardized utilities \(modeled after the SQL 2003 proposal\) to install and maintain Java code in the database.
-   Standardized mappings of parameters and result. Complex types as well as sets are supported.
-   An embedded, high performance, JDBC driver utilizing the internal Greenplum Database SPI routines.
-   Metadata support for the JDBC driver. Both `DatabaseMetaData` and `ResultSetMetaData` are included.
-   The ability to return a ResultSet from a query as an alternative to building a ResultSet row by row.
-   Full support for savepoints and exception handling.
-   The ability to use IN, INOUT, and OUT parameters.
-   Two separate Greenplum Database languages:
    -   pljava, TRUSTED PL/Java language
    -   pljavau, UNTRUSTED PL/Java language
-   Transaction and Savepoint listeners enabling code execution when a transaction or savepoint is committed or rolled back.
-   Integration with GNU GCJ on selected platforms.

A function in SQL will appoint a static method in a Java class. In order for the function to run, the appointed class must available on the class path specified by the Greenplum Database server configuration parameter `pljava_classpath`. The PL/Java extension adds a set of functions that helps installing and maintaining the java classes. Classes are stored in normal Java archives, JAR files. A JAR file can optionally contain a deployment descriptor that in turn contains SQL commands to be run when the JAR is deployed or undeployed. The functions are modeled after the standards proposed for SQL 2003.

PL/Java implements a standardized way of passing parameters and return values. Complex types and sets are passed using the standard JDBC ResultSet class.

A JDBC driver is included in PL/Java. This driver calls Greenplum Database internal SPI routines. The driver is essential since it is common for functions to make calls back to the database to fetch data. When PL/Java functions fetch data, they must use the same transactional boundaries that are used by the main function that entered PL/Java execution context.

PL/Java is optimized for performance. The Java virtual machine runs within the same process as the backend to minimize call overhead. PL/Java is designed with the objective to enable the power of Java to the database itself so that database intensive business logic can run as close to the actual data as possible.

The standard Java Native Interface \(JNI\) is used when bridging calls between the backend and the Java VM.

## <a id="topic10"></a>About Greenplum Database PL/Java 

There are a few key differences between the implementation of PL/Java in standard PostgreSQL and Greenplum Database.

### <a id="topic11"></a>Functions 

The following functions are not supported in Greenplum Database. The classpath is handled differently in a distributed Greenplum Database environment than in the PostgreSQL environment.

-   `sqlj.install_jar`
-   `sqlj.replace_jar`
-   `sqlj.remove_jar`
-   `sqlj.get_classpath`
-   `sqlj.set_classpath`

Greenplum Database uses the `pljava_classpath` server configuration parameter in place of the `sqlj.set_classpath` function.

### <a id="topic12"></a>Server Configuration Parameters 

The following server configuration parameters are used by PL/Java in Greenplum Database. These parameters replace the `pljava.*` parameters that are used in the standard PostgreSQL PL/Java implementation:

-   `pljava_classpath`

    A colon \(`:`\) separated list of the jar files containing the Java classes used in any PL/Java functions. The jar files must be installed in the same locations on all Greenplum Database hosts. With the trusted PL/Java language handler, jar file paths must be relative to the `$GPHOME/lib/postgresql/java/` directory. With the untrusted language handler \(javaU language tag\), paths may be relative to `$GPHOME/lib/postgresql/java/` or absolute.

    The server configuration parameter `pljava_classpath_insecure` controls whether the server configuration parameter `pljava_classpath` can be set by a user without Greenplum Database superuser privileges. When `pljava_classpath_insecure` is enabled, Greenplum Database developers who are working on PL/Java functions do not have to be database superusers to change `pljava_classpath`.

    > **Caution** Enabling `pljava_classpath_insecure` exposes a security risk by giving non-administrator database users the ability to run unauthorized Java methods.

-   `pljava_statement_cache_size`

    Sets the size in KB of the Most Recently Used \(MRU\) cache for prepared statements.

-   `pljava_release_lingering_savepoints`

    If `TRUE`, lingering savepoints will be released on function exit. If `FALSE`, they will be rolled back.

-   `pljava_vmoptions`

    Defines the start up options for the Greenplum Database Java VM.


See the *Greenplum Database Reference Guide* for information about the Greenplum Database server configuration parameters.

## <a id="topic_qx1_xcp_w3b"></a>Installing Java 

PL/Java requires a Java runtime environment on each Greenplum Database host. Ensure that the same Java environment is at the same location on all hosts: coordinators and segments. The command `java -version` displays the Java version.

The commands that you use to install Java depend on the host system operating system and Java version. To install OpenJDK 8 or 11 \(Java 8 JDK or Java 11 JDK\) on RHEL/Oracle/Rocky Linux:

```
$ sudo yum install java-<version>-openjdk-devel
```

For OpenJDK 8 the version is `1.8.0`, for OpenJDK 11 the version is `11`.

After installing OpenJDK on a RHEL system, run this `update-alternatives` command to change the default Java. Enter the number that represents the OpenJDK version to use as the default.

```
$ sudo update-alternatives --config java 
```

> **Note** When configuring host systems, you can use the [gpssh](../utility_guide/ref/gpssh.html) utility to run bash shell commands on multiple remote hosts.

## <a id="topic4"></a>Installing PL/Java 

For Greenplum Database, the PL/Java extension is available as a package. Download the package from the Greenplum Database page on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb) and then install the software with the Greenplum Package Manager \(`gppkg`\).

The [gppkg](../utility_guide/ref/gppkg.html) utility installs Greenplum Database extensions, along with any dependencies, on all hosts across a cluster. It also automatically installs extensions on new hosts in the case of system expansion and segment recovery.

To install and use PL/Java:

1.  Specify the Java version used by PL/Java. Set the environment variables `JAVA_HOME` and `LD_LIBRARY_PATH` in the `greenplum_path.sh`.
2.  Install the Greenplum Database PL/Java extension.
3.  Enable the language for each database where you intend to use PL/Java.
4.  Install user-created JAR files containing Java methods into the same directory on all Greenplum Database hosts.
5.  Add the name of the JAR file to the Greenplum Database server configuration parameter `pljava_classpath`. The parameter lists the installed JAR files. For information about the parameter, see the *Greenplum Database Reference Guide*.

### <a id="topic5"></a>Installing the Greenplum PL/Java Extension 

Before you install the PL/Java extension, make sure that your Greenplum database is running, you have sourced `greenplum_path.sh`, and that the `$MASTER_DATA_DIRECTORY` and `$GPHOME` variables are set.

1.  Download the PL/Java extension package from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb) then copy it to the coordinator host.
2.  Follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the **Greenplum Procedural Languages PL/Java** software.
3.  Install the software extension package by running the `gppkg` command. This example installs the PL/Java extension package on a Linux system:
    ```
    $ gppkg -i pljava-1.4.3-gp5-rhel<osversion>_x86_64.gppkg
    ```

4.  Ensure that the environment variables `JAVA_HOME` and `LD_LIBRARY_PATH` are set properly in `$GPHOME/greenplum_path.sh` on all Greenplum Database hosts.
    -   Set the `JAVA_HOME` variable to the directory where your Java Runtime is installed. For example, for Oracle JRE this directory would be `/usr/java/latest`. For OpenJDK, the directory is `/usr/lib/jvm`. This example changes the environment variable to use `/usr/lib/jvm`.

        ```
        export JAVA_HOME=/usr/lib/jvm
        ```
    -   Set the `LD_LIBRARY_PATH` to include the directory with the Java server runtime libraries. PL/Java depends on `libjvm.so` and the shared object should be in your `LD_LIBRARY_PATH`. By default, `libjvm.so` is available in `$JAVA_HOME/lib/server` with JDK 11, or in `$JAVA_HOME/jre/lib/amd64/server` with JDK 8. This example adds the JDK 11 directory to the environment variable.
        ```
        export LD_LIBRARY_PATH=$GPHOME/lib:$GPHOME/ext/python/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH
        ```

    This example [gpsync](../utility_guide/ref/gpsync.html) command copies the file to all hosts specified in the file `gphosts_file`.

    ```
    $ gpsync -f gphosts_file $GPHOME/greenplum_path.sh 
    =:$GPHOME/greenplum_path.sh
    ```

5.  Reload `greenplum_path.sh`.
    ```
    $ source $GPHOME/greenplum_path.sh
    ```

6.  Restart Greenplum Database.
    ```
    $ gpstop -r
    ```


## <a id="topic6"></a>Enabling PL/Java and Installing JAR Files 

Perform the following steps as the Greenplum Database administrator `gpadmin`.

1.  Enable PL/Java in a database by running the `CREATE EXTENSION` command to register the language. For example, this command enables PL/Java in the `testdb` database:

    ```
    $ psql -d testdb -c 'CREATE EXTENSION pljava;'
    ```

    > **Note** The PL/Java `install.sql` script, used in previous releases to register the language, is deprecated.

2.  Copy your Java archives \(JAR files\) to the same directory on all Greenplum Database hosts. This example uses the Greenplum Database `gpsync` utility to copy the file `myclasses.jar` to the directory `$GPHOME/lib/postgresql/java/`:

    ```
    $ gpsync -f gphosts_file myclasses.jar 
    =:/usr/local/greenplum-db/lib/postgresql/java/
    ```

    The file `gphosts_file` contains a list of the Greenplum Database hosts.

3.  Set the `pljava_classpath` server configuration parameter in the coordinator `postgresql.conf` file. For this example, the parameter value is a colon \(:\) separated list of the JAR files. For example:

    ```
    $ gpconfig -c pljava_classpath -v 'examples.jar:myclasses.jar'
    ```

    The file `examples.jar` is installed when you install the PL/Java extension package with the `gppkg` utility.

    > **Note** If you install JAR files in a directory other than `$GPHOME/lib/postgresql/java/`, you must specify the absolute path to the JAR file. Each JAR file must be in the same location on all Greenplum Database hosts. For more information about specifying the location of JAR files, see the information about the `pljava_classpath` server configuration parameter in the *Greenplum Database Reference Guide*.

4.  Reload the `postgresql.conf` file.

    ```
    $ gpstop -u
    ```

5.  \(optional\) Greenplum provides an `examples.sql` file containing sample PL/Java functions that you can use for testing. Run the commands in this file to create the test functions \(which use the Java classes in `examples.jar`\).

    ```
    $ psql -f $GPHOME/share/postgresql/pljava/examples.sql
    ```


## <a id="topic7"></a>Uninstalling PL/Java 

-   [Remove PL/Java Support for a Database](#topic8)
-   [Uninstall the Java JAR files and Software Package](#topic_tgt_nfg_mjb)

### <a id="topic8"></a>Remove PL/Java Support for a Database 

Use the `DROP EXTENSION` command to remove support for PL/Java from a database. For example, this command deactivates the PL/Java language in the `testdb` database:

```
$ psql -d testdb -c 'DROP EXTENSION pljava;'
```

The default command fails if any existing objects \(such as functions\) depend on the language. Specify the `CASCADE` option to also drop all dependent objects, including functions that you created with PL/Java.

> **Note** The PL/Java `uninstall.sql` script, used in previous releases to remove the language registration, is deprecated.

### <a id="topic_tgt_nfg_mjb"></a>Uninstall the Java JAR files and Software Package 

If no databases have PL/Java as a registered language, remove the Java JAR files and uninstall the Greenplum PL/Java extension with the `gppkg` utility.

1.  Remove the `pljava_classpath` server configuration parameter from the `postgresql.conf` file on all Greenplum Database hosts. For example:

    ```
    $ gpconfig -r pljava_classpath
    ```

2.  Remove the JAR files from the directories where they were installed on all Greenplum Database hosts. For information about JAR file installation directories, see [Enabling PL/Java and Installing JAR Files](#topic6).
3.  Use the Greenplum `gppkg` utility with the `-r` option to uninstall the PL/Java extension. This example uninstalls the PL/Java extension on a Linux system:

    ```
    $ gppkg -r pljava-1.4.3
    ```

    You can run the `gppkg` utility with the options `-q --all` to list the installed extensions and their versions.

4.  Remove any updates you made to `greenplum_path.sh` for PL/Java.
5.  Reload `greenplum_path.sh` and restart the database

    ```
    $ source $GPHOME/greenplum_path.sh
    $ gpstop -r 
    ```


## <a id="topic13"></a>Writing PL/Java functions 

Information about writing functions with PL/Java.

-   [SQL Declaration](#topic14)
-   [Type Mapping](#topic15)
-   [NULL Handling](#topic16)
-   [Complex Types](#topic17)
-   [Returning Complex Types](#topic18)
-   [Returning Complex Types](#topic18)
-   [Functions That Return Sets](#topic19)
-   [Returning a SETOF <scalar type\>](#topic20)
-   [Returning a SETOF <complex type\>](#topic21)

### <a id="topic14"></a>SQL Declaration 

A Java function is declared with the name of a class and a static method on that class. The class will be resolved using the classpath that has been defined for the schema where the function is declared. If no classpath has been defined for that schema, the public schema is used. If no classpath is found there either, the class is resolved using the system classloader.

The following function can be declared to access the static method `getProperty` on `java.lang.System` class:

```
CREATE FUNCTION getsysprop(VARCHAR)
  RETURNS VARCHAR
  AS 'java.lang.System.getProperty'
  LANGUAGE java;
```

Run the following command to return the Java `user.home` property:

```
SELECT getsysprop('user.home');
```

### <a id="topic15"></a>Type Mapping 

Scalar types are mapped in a straight forward way. This table lists the current mappings.

|PostgreSQL|Java|
|----------|----|
|bool|boolean|
|char|byte|
|int2|short|
|int4|int|
|int8|long|
|varchar|java.lang.String|
|text|java.lang.String|
|bytea|byte\[ \]|
|date|java.sql.Date|
|time|java.sql.Time \(stored value treated as local time\)|
|timetz|java.sql.Time|
|timestamp|java.sql.Timestamp \(stored value treated as local time\)|
|timestamptz|java.sql.Timestamp|
|complex|java.sql.ResultSet|
|setof complex|java.sql.ResultSet|

All other types are mapped to java.lang.String and will utilize the standard `textin`/`textout` routines registered for respective type.

### <a id="topic16"></a>NULL Handling 

The scalar types that map to Java primitives can not be passed as `NULL` values. To pass `NULL` values, those types can have an alternative mapping. You enable this mapping by explicitly denoting it in the method reference.

```
CREATE FUNCTION trueIfEvenOrNull(integer)
  RETURNS bool
  AS 'foo.fee.Fum.trueIfEvenOrNull(java.lang.Integer)'
  LANGUAGE java;
```

The Java code would be similar to this:

```
package foo.fee;
public class Fum
{
  static boolean trueIfEvenOrNull(Integer value)
  {
    return (value == null)
      ? true
      : (value.intValue() % 2) == 0;
  }
}
```

The following two statements both yield true:

```
SELECT trueIfEvenOrNull(NULL);
SELECT trueIfEvenOrNull(4);
```

In order to return `NULL` values from a Java method, you use the object type that corresponds to the primitive \(for example, you return `java.lang.Integer` instead of `int`\). The PL/Java resolve mechanism finds the method regardless. Since Java cannot have different return types for methods with the same name, this does not introduce any ambiguity.

### <a id="topic17"></a>Complex Types 

A complex type will always be passed as a read-only `java.sql.ResultSet` with exactly one row. The ResultSet is positioned on its row so a call to `next()` should not be made. The values of the complex type are retrieved using the standard getter methods of the ResultSet.

Example:

```
CREATE TYPE complexTest
  AS(base integer, incbase integer, ctime timestamptz);
CREATE FUNCTION useComplexTest(complexTest)
  RETURNS VARCHAR
  AS 'foo.fee.Fum.useComplexTest'
  IMMUTABLE LANGUAGE java;
```

In the Java class `Fum`, we add the following static method:

```
public static String useComplexTest(ResultSet complexTest)
throws SQLException
{
  int base = complexTest.getInt(1);
  int incbase = complexTest.getInt(2);
  Timestamp ctime = complexTest.getTimestamp(3);
  return "Base = \"" + base +
    "\", incbase = \"" + incbase +
    "\", ctime = \"" + ctime + "\"";
}
```

### <a id="topic18"></a>Returning Complex Types 

Java does not stipulate any way to create a ResultSet. Hence, returning a ResultSet is not an option. The SQL-2003 draft suggests that a complex return value should be handled as an IN/OUT parameter. PL/Java implements a ResultSet that way. If you declare a function that returns a complex type, you will need to use a Java method with boolean return type with a last parameter of type `java.sql.ResultSet`. The parameter will be initialized to an empty updateable ResultSet that contains exactly one row.

Assume that the `complexTest` type in previous section has been created.

```
CREATE FUNCTION createComplexTest(int, int)
  RETURNS complexTest
  AS 'foo.fee.Fum.createComplexTest'
  IMMUTABLE LANGUAGE java;
```

The PL/Java method resolve will now find the following method in the `Fum` class:

```
public static boolean complexReturn(int base, int increment, 
  ResultSet receiver)
throws SQLException
{
  receiver.updateInt(1, base);
  receiver.updateInt(2, base + increment);
  receiver.updateTimestamp(3, new 
    Timestamp(System.currentTimeMillis()));
  return true;
}
```

The return value denotes if the receiver should be considered as a valid tuple \(true\) or NULL \(false\).

### <a id="topic19"></a>Functions That Return Sets 

When returning result sets, you should not build a result set before returning it, because building a large result set would consume a large amount of resources. It is better to produce one row at a time. Incidentally, that is what the Greenplum Database backend expects a function with SETOF return to do. You can return a SETOF a scalar type such as an `int`, `float` or `varchar`, or you can return a SETOF a complex type.

### <a id="topic20"></a>Returning a SETOF <scalar type\> 

In order to return a set of a scalar type, you need create a Java method that returns something that implements the `java.util.Iterator` interface. Here is an example of a method that returns a SETOF `varchar`:

```
CREATE FUNCTION javatest.getSystemProperties()
  RETURNS SETOF varchar
  AS 'foo.fee.Bar.getNames'
  IMMUTABLE LANGUAGE java;
```

This simple Java method returns an iterator:

```
package foo.fee;
import java.util.Iterator;

public class Bar
{
    public static Iterator getNames()
    {
        ArrayList names = new ArrayList();
        names.add("Lisa");
        names.add("Bob");
        names.add("Bill");
        names.add("Sally");
        return names.iterator();
    }
}
```

### <a id="topic21"></a>Returning a SETOF <complex type\> 

A method returning a SETOF <complex type\> must use either the interface `org.postgresql.pljava.ResultSetProvider` or `org.postgresql.pljava.ResultSetHandle`. The reason for having two interfaces is that they cater for optimal handling of two distinct use cases. The former is for cases when you want to dynamically create each row that is to be returned from the SETOF function. The latter makes sense in cases where you want to return the result of a query after it runs.

#### <a id="topic22"></a>Using the ResultSetProvider Interface 

This interface has two methods. The boolean `assignRowValues(java.sql.ResultSet tupleBuilder, int rowNumber)` and the `void close()` method. The Greenplum Database query evaluator will call the `assignRowValues` repeatedly until it returns false or until the evaluator decides that it does not need any more rows. Then it calls close.

You can use this interface the following way:

```
CREATE FUNCTION javatest.listComplexTests(int, int)
  RETURNS SETOF complexTest
  AS 'foo.fee.Fum.listComplexTest'
  IMMUTABLE LANGUAGE java;
```

The function maps to a static java method that returns an instance that implements the `ResultSetProvider` interface.

```
public class Fum implements ResultSetProvider
{
  private final int m_base;
  private final int m_increment;
  public Fum(int base, int increment)
  {
    m_base = base;
    m_increment = increment;
  }
  public boolean assignRowValues(ResultSet receiver, int 
currentRow)
  throws SQLException
  {
    // Stop when we reach 12 rows.
    //
    if(currentRow >= 12)
      return false;
    receiver.updateInt(1, m_base);
    receiver.updateInt(2, m_base + m_increment * currentRow);
    receiver.updateTimestamp(3, new 
Timestamp(System.currentTimeMillis()));
    return true;
  }
  public void close()
  {
   // Nothing needed in this example
  }
  public static ResultSetProvider listComplexTests(int base, 
int increment)
  throws SQLException
  {
    return new Fum(base, increment);
  }
}
```

The `listComplextTests` method is called once. It may return `NULL` if no results are available or an instance of the `ResultSetProvider`. Here the Java class `Fum` implements this interface so it returns an instance of itself. The method `assignRowValues` will then be called repeatedly until it returns false. At that time, close will be called.

#### <a id="topic23"></a>Using the ResultSetHandle Interface 

This interface is similar to the `ResultSetProvider` interface in that it has a `close()` method that will be called at the end. But instead of having the evaluator call a method that builds one row at a time, this method has a method that returns a ResultSet. The query evaluator will iterate over this set and deliver the RestulSet contents, one tuple at a time, to the caller until a call to `next()` returns false or the evaluator decides that no more rows are needed.

Here is an example that runs a query using a statement that it obtained using the default connection. The SQL suitable for the deployment descriptor looks like this:

```
CREATE FUNCTION javatest.listSupers()
  RETURNS SETOF pg_user
  AS 'org.postgresql.pljava.example.Users.listSupers'
  LANGUAGE java;
CREATE FUNCTION javatest.listNonSupers()
  RETURNS SETOF pg_user
  AS 'org.postgresql.pljava.example.Users.listNonSupers'
  LANGUAGE java;
```

And in the Java package `org.postgresql.pljava.example` a class `Users` is added:

```
public class Users implements ResultSetHandle
{
  private final String m_filter;
  private Statement m_statement;
  public Users(String filter)
  {
    m_filter = filter;
  }
  public ResultSet getResultSet()
  throws SQLException
  {
    m_statement = 
      DriverManager.getConnection("jdbc:default:connection").cr
eateStatement();
    return m_statement.executeQuery("SELECT * FROM pg_user 
       WHERE " + m_filter);
  }

  public void close()
  throws SQLException
  {
    m_statement.close();
  }

  public static ResultSetHandle listSupers()
  {
    return new Users("usesuper = true");
  }

  public static ResultSetHandle listNonSupers()
  {
    return new Users("usesuper = false");
  }
}
```

## <a id="topic24"></a>Using JDBC 

PL/Java contains a JDBC driver that maps to the PostgreSQL SPI functions. A connection that maps to the current transaction can be obtained using the following statement:

```
Connection conn = 
  DriverManager.getConnection("jdbc:default:connection"); 
```

After obtaining a connection, you can prepare and run statements similar to other JDBC connections. These are limitations for the PL/Java JDBC driver:

-   The transaction cannot be managed in any way. Thus, you cannot use methods on the connection such as:
    -   `commit()`
    -   `rollback()`
    -   `setAutoCommit()`
    -   `setTransactionIsolation()`
-   Savepoints are available with some restrictions. A savepoint cannot outlive the function in which it was set and it must be rolled back or released by that same function.
-   A ResultSet returned from `executeQuery()` are always `FETCH_FORWARD` and `CONCUR_READ_ONLY`.
-   Metadata is only available in PL/Java 1.1 or higher.
-   `CallableStatement` \(for stored procedures\) is not implemented.
-   The types `Clob` or `Blob` are not completely implemented, they need more work. The types `byte[]` and `String` can be used for `bytea` and `text` respectively.

## <a id="topic25"></a>Exception Handling 

You can catch and handle an exception in the Greenplum Database backend just like any other exception. The backend ErrorData structure is exposed as a property in a class called `org.postgresql.pljava.ServerException` \(derived from `java.sql.SQLException`\) and the Java try/catch mechanism is synchronized with the backend mechanism.

> **Important** You will not be able to continue running backend functions until your function has returned and the error has been propagated when the backend has generated an exception unless you have used a savepoint. When a savepoint is rolled back, the exceptional condition is reset and you can continue your execution.

## <a id="topic26"></a>Savepoints 

Greenplum Database savepoints are exposed using the java.sql.Connection interface. Two restrictions apply.

-   A savepoint must be rolled back or released in the function where it was set.
-   A savepoint must not outlive the function where it was set.

## <a id="topic27"></a>Logging 

PL/Java uses the standard Java Logger. Hence, you can write things like:

```
Logger.getAnonymousLogger().info( "Time is " + new 
Date(System.currentTimeMillis()));
```

At present, the logger uses a handler that maps the current state of the Greenplum Database configuration setting `log_min_messages` to a valid Logger level and that outputs all messages using the Greenplum Database backend function `elog()`.

> **Note** The `log_min_messages` setting is read from the database the first time a PL/Java function in a session is run. On the Java side, the setting does not change after the first PL/Java function execution in a specific session until the Greenplum Database session that is working with PL/Java is restarted.

The following mapping apply between the Logger levels and the Greenplum Database backend levels.

|java.util.logging.Level|Greenplum Database Level|
|-----------------------|------------------------|
|SEVERE ERROR|ERROR|
|WARNING|WARNING|
|CONFIG|LOG|
|INFO|INFO|
|FINE|DEBUG1|
|FINER|DEBUG2|
|FINEST|DEBUG3|

## <a id="topic28"></a>Security 

-   [Installation](#topic29)
-   [Trusted Language](#topic30)

### <a id="topic29"></a>Installation 

Only a database superuser can install PL/Java. The PL/Java utility functions are installed using SECURITY DEFINER so that they run with the access permissions that were granted to the creator of the functions.

### <a id="topic30"></a>Trusted Language 

PL/Java is a *trusted* language. The trusted PL/Java language has no access to the file system as stipulated by PostgreSQL definition of a trusted language. Any database user can create and access functions in a trusted language.

PL/Java also installs a language handler for the language `javau`. This version is *not trusted* and only a superuser can create new functions that use it. Any user can call the functions.

To install both the trusted and untrusted languages, register the extension by running the `'CREATE EXTENSION pljava'` command when [Enabling PL/Java and Installing JAR Files](#topic6).

To install only the trusted language, register the extension by running the `'CREATE EXTENSION pljavat'` command when [Enabling PL/Java and Installing JAR Files](#topic6).

## <a id="topic33"></a>Some PL/Java Issues and Solutions 

When writing the PL/Java, mapping the JVM into the same process-space as the Greenplum Database backend code, some concerns have been raised regarding multiple threads, exception handling, and memory management. Here are brief descriptions explaining how these issues where resolved.

-   [Multi-threading](#topic34)
-   [Exception Handling](#topic36)
-   [Java Garbage Collector Versus palloc\(\) and Stack Allocation](#topic38)

### <a id="topic34"></a>Multi-threading 

Java is inherently multi-threaded. The Greenplum Database backend is not. There is nothing stopping a developer from utilizing multiple Threads class in the Java code. Finalizers that call out to the backend might have been spawned from a background Garbage Collection thread. Several third party Java-packages that are likely to be used make use of multiple threads. How can this model coexist with the Greenplum Database backend in the same process?

#### <a id="topic35"></a>Solution 

The solution is simple. PL/Java defines a special object called the `Backend.THREADLOCK`. When PL/Java is initialized, the backend immediately grabs this objects monitor \(i.e. it will synchronize on this object\). When the backend calls a Java function, the monitor is released and then immediately regained when the call returns. All calls from Java out to backend code are synchronized on the same lock. This ensures that only one thread at a time can call the backend from Java, and only at a time when the backend is awaiting the return of a Java function call.

### <a id="topic36"></a>Exception Handling 

Java makes frequent use of try/catch/finally blocks. Greenplum Database sometimes use an exception mechanism that calls `longjmp` to transfer control to a known state. Such a jump would normally effectively bypass the JVM.

#### <a id="topic37"></a>Solution 

The backend now allows errors to be caught using the macros `PG_TRY/PG_CATCH`/`PG_END_TRY` and in the catch block, the error can be examined using the ErrorData structure. PL/Java implements a `java.sql.SQLException` subclass called `org.postgresql.pljava.ServerException`. The ErrorData can be retrieved and examined from that exception. A catch handler is allowed to issue a rollback to a savepoint. After a successful rollback, execution can continue.

### <a id="topic38"></a>Java Garbage Collector Versus palloc\(\) and Stack Allocation 

Primitive types are always be passed by value. This includes the `String` type \(this is a must since Java uses double byte characters\). Complex types are often wrapped in Java objects and passed by reference. For example, a Java object can contain a pointer to a palloc'ed or stack allocated memory and use native JNI calls to extract and manipulate data. Such data will become stale once a call has ended. Further attempts to access such data will at best give very unpredictable results but more likely cause a memory fault and a crash.

#### <a id="topic39"></a>Solution 

The PL/Java contains code that ensures that stale pointers are cleared when the MemoryContext or stack where they were allocated goes out of scope. The Java wrapper objects might live on but any attempt to use them will result in a stale native handle exception.

## <a id="topic40"></a>Example 

The following simple Java example creates a JAR file that contains a single method and runs the method.

> **Note** The example requires Java SDK to compile the Java file.

The following method returns a substring.

``` pre
{
public static String substring(String text, int beginIndex,
  int endIndex)
    {
    return text.substring(beginIndex, endIndex);
    }
}
```

Enter the java code in a text file `example.class`.

Contents of the file `manifest.txt`:

```
Manifest-Version: 1.0
Main-Class: Example
Specification-Title: "Example"
Specification-Version: "1.0"
Created-By: 1.6.0_35-b10-428-11M3811
Build-Date: 01/20/2013 10:09 AM
```

Compile the java code:

```
javac *.java
```

Create a JAR archive named analytics.jar that contains the class file and the manifest file MANIFEST file in the JAR.

```
jar cfm analytics.jar manifest.txt *.class
```

Upload the jar file to the Greenplum coordinator host.

Run the `gpsync` utility to copy the jar file to the Greenplum Java directory. Use the `-f` option to specify the file that contains a list of the coordinator and segment hosts.

```
gpsync -f gphosts_file analytics.jar 
=:/usr/local/greenplum-db/lib/postgresql/java/
```

Use the `gpconfig` utility to set the Greenplum `pljava_classpath` server configuration parameter. The parameter lists the installed jar files.

```
gpconfig -c pljava_classpath -v 'analytics.jar'
```

Run the `gpstop` utility with the `-u` option to reload the configuration files.

```
gpstop -u
```

From the `psql` command line, run the following command to show the installed jar files.

```
show pljava_classpath
```

The following SQL commands create a table and define a Java function to test the method in the jar file:

```
create table temp (a varchar) distributed randomly; 
insert into temp values ('my string'); 
--Example function 
create or replace function java_substring(varchar, int, int) 
returns varchar as 'Example.substring' language java; 
--Example execution 
select java_substring(a, 1, 5) from temp;
```

You can place the contents in a file, `mysample.sql` and run the command from a `psql` command line:

```
> \i mysample.sql 
```

The output is similar to this:

```
java_substring
----------------
 y st
(1 row)
```

## <a id="topic41"></a>References 

The PL/Java Github wiki page - [https://github.com/tada/pljava/wiki](https://github.com/tada/pljava/wiki).

PL/Java 1.5.0 release - [https://github.com/tada/pljava/tree/REL1\_5\_STABLE](https://github.com/tada/pljava/tree/REL1_5_STABLE).

