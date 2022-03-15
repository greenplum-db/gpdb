---
title: Examples of Reading Fixed-Width Data 
---

The following examples show how to read fixed-width data.

## <a id="ex1"></a>Example 1 – Loading a table with all fields defined 

```
CREATE READABLE EXTERNAL TABLE students (
name varchar(20), address varchar(30), age int)
LOCATION ('file://<host>/file/path/')
FORMAT 'CUSTOM' (formatter=fixedwidth_in, 
         name=20, address=30, age=4);

```

## <a id="ex2"></a>Example 2 – Loading a table with PRESERVED\_BLANKS on 

```
CREATE READABLE EXTERNAL TABLE students (
name varchar(20), address varchar(30), age int)
LOCATION ('gpfdist://<host>:<portNum>/file/path/')
FORMAT 'CUSTOM' (formatter=fixedwidth_in, 
         name=20, address=30, age=4,
        preserve_blanks='on',null='NULL');

```

## <a id="ex3"></a>Example 3 – Loading data with no line delimiter 

```
CREATE READABLE EXTERNAL TABLE students (
name varchar(20), address varchar(30), age int)
LOCATION ('file://<host>/file/path/')
FORMAT 'CUSTOM' (formatter=fixedwidth_in, 
         name='20', address='30', age='4', line_delim='?@')

```

## <a id="ex4"></a>Example 4 – Create a writable external table with a \\r\\n line delimiter 

```
CREATE WRITABLE EXTERNAL TABLE students_out (
name varchar(20), address varchar(30), age int)
LOCATION ('gpfdist://<host>:<portNum>/file/path/students_out.txt')     
FORMAT 'CUSTOM' (formatter=fixedwidth_out, 
        name=20, address=30, age=4, line_delim=E'\r\n');

```

**Parent topic:**[Using a Custom Format](../../load/topics/g-using-a-custom-format.html)

