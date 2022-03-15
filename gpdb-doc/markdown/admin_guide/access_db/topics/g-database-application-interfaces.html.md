---
title: Database Application Interfaces 
---

You may want to develop your own client applications that interface to Greenplum Database. PostgreSQL provides a number of database drivers for the most commonly used database application programming interfaces \(APIs\), which can also be used with Greenplum Database. These drivers are available as a separate download. Each driver \(except libpq, which comes with PostgreSQL\) is an independent PostgreSQL development project and must be downloaded, installed and configured to connect to Greenplum Database. The following drivers are available:

|API|PostgreSQL Driver|Download Link|
|---|-----------------|-------------|
|ODBC|Greenplum DataDirect ODBC Driver|[https://network.pivotal.io/products/pivotal-gpdb](https://network.pivotal.io/products/pivotal-gpdb).|
|ODBC|psqlODBC|[https://odbc.postgresql.org/](https://odbc.postgresql.org/)|
|JDBC|Greenplum DataDirect JDBC Driver|[https://network.pivotal.io/products/pivotal-gpdb](https://network.pivotal.io/products/pivotal-gpdb)|
|JDBC|pgjdbc|[https://jdbc.postgresql.org/](https://jdbc.postgresql.org/)|
|Perl DBI|pgperl|[https://metacpan.org/release/DBD-Pg](https://metacpan.org/release/DBD-Pg)|
|Python DBI|pygresql|[http://www.pygresql.org/](http://www.pygresql.org/)|
|libpq C Library|libpq|[https://www.postgresql.org/docs/9.4/libpq.html](https://www.postgresql.org/docs/9.4/libpq.html)|

General instructions for accessing a Greenplum Database with an API are:

1.  Download your programming language platform and respective API from the appropriate source. For example, you can get the Java Development Kit \(JDK\) and JDBC API from Oracle.
2.  Write your client application according to the API specifications. When programming your application, be aware of the SQL support in Greenplum Database so you do not include any unsupported SQL syntax.

Download the appropriate driver and configure connectivity to your Greenplum Database master instance.

**Parent topic:**[Accessing the Database](../../access_db/topics/g-accessing-the-database.html)

