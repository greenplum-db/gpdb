---
title: Procedural Languages 
---

Greenplum supports a pluggable procedural language architecture by virtue of its PostgreSQL heritage. This allows user-defined functions to be written in languages other than SQL and C. It may be more convenient to develop analytics functions in a familiar procedural language compared to using only SQL constructs. For example, suppose you have existing Python code that you want to use on data in Greenplum, you can wrap this code in a PL/Python function and call it from SQL.

The available Greenplum procedural languages are typically packaged as extensions. You register a language in a database using the [CREATE EXTENSION](../ref_guide/sql_commands/CREATE_EXTENSION.html) command. You remove a language from a database with [DROP EXTENSION](../ref_guide/sql_commands/DROP_EXTENSION.html).

The Greenplum Database distribution supports the following procedural languages; refer to the linked language documentation for installation and usage instructions:

-   [PL/Container](pl_container.html)
-   [PL/Java](pl_java.html)
-   [PL/Perl](pl_perl.html)
-   [PL/pgSQL](pl_sql.html)
-   [PL/Python](pl_python.html)
-   [PL/R](pl_r.html)

