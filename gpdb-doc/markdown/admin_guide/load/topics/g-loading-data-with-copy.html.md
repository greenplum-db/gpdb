---
title: Loading Data with COPY 
---

`COPY FROM` copies data from a file or standard input into a table and appends the data to the table contents. `COPY` is non-parallel: data is loaded in a single process using the Greenplum master instance. Using `COPY` is only recommended for very small data files.

The `COPY` source file must be accessible to the `postgres` process on the master host. Specify the `COPY` source file name relative to the data directory on the master host, or specify an absolute path.

Greenplum copies data from `STDIN` or `STDOUT` using the connection between the client and the master server.

**Parent topic:**[Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

## <a id="topic1"></a>Loading From a File 

The `COPY` command asks the `postgres` backend to open the specified file, read it and append it to the table. In order to be able to read the file, the backend needs to have read permissions on the file, and the file name must be specified using an absolute path on the master host, or a relative path to the master data directory.

```
COPY <table_name> FROM </path/to/filename>;
```

## <a id="topic2"></a>Loading From STDIN 

To avoid the problem of copying the data file to the master host before loading the data, `COPY FROM STDIN` uses the Standard Input channel and feeds data directly into the `postgres` backend. After the `COPY FROM STDIN` command started, the backend will accept lines of data until a single line only contains a backslash-period \(\\.\).

```
COPY <table_name> FROM <STDIN>;
```

## <a id="topic3"></a>Loading Data Using \\copy in psql 

Do not confuse the psql `\copy` command with the `COPY` SQL command. The `\copy` invokes a regular `COPY FROM STDIN` and sends the data from the psql client to the backend. Therefore any file must reside on the host where the psql client runs, and must be accessible to the user which runs the client.

To avoid the problem of copying the data file to the master host before loading the data, `COPY FROM STDIN` uses the Standard Input channel and feeds data directly into the `postgres` backend. After the `COPY FROM STDIN` command started, the backend will accept lines of data until a single line only contains a backslash-period \(\\.\). psql is wrapping all of this into the handy `\copy` command.

```
\copy <table_name> FROM <filename>;
```

## <a id="topic4"></a>Input Format 

`COPY FROM` accepts a `FORMAT` parameter, which specifies the format of the input data. The possible values are `TEXT`, `CSV` \(Comma Separated Values\), and `BINARY`.

```
COPY <table_name> FROM </path/to/filename> WITH (FORMAT csv);
```

The `FORMAT csv` will read comma-separated values. The `FORMAT text` by default uses tabulators to separate the values, the `DELIMITER` option specifies a different character as value delimiter.

```
COPY <table_name> FROM </path/to/filename> WITH (FORMAT text, DELIMITER '|');
```

By default, the default client encoding is used, this can be changed with the `ENCODING` option. This is useful if data is coming from another operating system.

```
COPY <table_name> FROM </path/to/filename> WITH (ENCODING 'latin1');
```

