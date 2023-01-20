# gplogfilter 

Searches through Greenplum Database log files for specified entries.

## <a id="section2"></a>Synopsis 

```
gplogfilter [<timestamp_options>] [<pattern_options>] 
     [<output_options>] [<input_options>] [<input_file>] 

gplogfilter --help 

gplogfilter --version
```

## <a id="section3"></a>Description 

The `gplogfilter` utility can be used to search through a Greenplum Database log file for entries matching the specified criteria. If an input file is not supplied, then `gplogfilter` will use the `$COORDINATOR_DATA_DIRECTORY` environment variable to locate the Greenplum coordinator log file in the standard logging location. To read from standard input, use a dash \(`-`\) as the input file name. Input files may be compressed using `gzip`. In an input file, a log entry is identified by its timestamp in `YYYY-MM-DD [hh:mm[:ss]]` format.

You can also use `gplogfilter` to search through all segment log files at once by running it through the [gpssh](gpssh.html) utility. For example, to display the last three lines of each segment log file:

```
gpssh -f seg_host_file
=> source /usr/local/greenplum-db/greenplum_path.sh
=> gplogfilter -n 3 /gpdata/*/log/gpdb*.csv
```

By default, the output of `gplogfilter` is sent to standard output. Use the `-o` option to send the output to a file or a directory. If you supply an output file name ending in `.gz`, the output file will be compressed by default using maximum compression. If the output destination is a directory, the output file is given the same name as the input file.

## <a id="section4"></a>Options 

**Timestamp Options**

-b datetime \| --begin=datetime
:   Specifies a starting date and time to begin searching for log entries in the format of `YYYY-MM-DD [hh:mm[:ss]]`.

:   If a time is specified, the date and time must be enclosed in either single or double quotes. This example encloses the date and time in single quotes:

:   ```
gplogfilter -b '2013-05-23 14:33'
```

-e datetime \| --end=datetime
:   Specifies an ending date and time to stop searching for log entries in the format of `YYYY-MM-DD [hh:mm[:ss]]`.

:   If a time is specified, the date and time must be enclosed in either single or double quotes. This example encloses the date and time in single quotes:

:   ```
gplogfilter -e '2013-05-23 14:33' 
```

-d<time\> \| --duration=<time\>
:   Specifies a time duration to search for log entries in the format of `[hh][:mm[:ss]]`. If used without either the `-b` or `-e` option, will use the current time as a basis.

**Pattern Matching Options**

-c i \[gnore\] \| r \[espect\] \| --case=i \[gnore\] \| r \[espect\]
:   Matching of alphabetic characters is case sensitive by default unless proceeded by the `--case=ignore` option.

-C 'string' \| --columns='string'
:   Selects specific columns from the log file. Specify the desired columns as a comma-delimited string of column numbers beginning with 1, where the second column from left is 2, the third is 3, and so on. See "Viewing the Database Server Log Files" in the *Greenplum Database Administrator Guide* for details about the log file format and for a list of the available columns and their associated number.

-f 'string' \| --find='string'
:   Finds the log entries containing the specified string.

-F 'string' \| --nofind='string'
:   Rejects the log entries containing the specified string.

-m regex \| --match=regex
:   Finds log entries that match the specified Python regular expression. See [https://docs.python.org/library/re.html](https://docs.python.org/library/re.html) for Python regular expression syntax.

-M regex \| --nomatch=regex
:   Rejects log entries that match the specified Python regular expression. See [https://docs.python.org/library/re.html](https://docs.python.org/library/re.html) for Python regular expression syntax.

-t \| --trouble
:   Finds only the log entries that have `ERROR:`, `FATAL:`, or `PANIC:` in the first line.

**Output Options**

-n <integer\> \| --tail=<integer\>
:   Limits the output to the last <integer\> of qualifying log entries found.

-s <offset\> \[limit\] \| --slice=<offset\> \[limit\]
:   From the list of qualifying log entries, returns the <limit\> number of entries starting at the <offset\> entry number, where an <offset\> of zero \(`0`\) denotes the first entry in the result set and an <offset\> of any number greater than zero counts back from the end of the result set.

-o <output\_file\> \| --out=<output\_file\>
:   Writes the output to the specified file or directory location instead of `STDOUT`.

-z 0-9 \| --zip=0-9
:   Compresses the output file to the specified compression level using `gzip`, where `0` is no compression and `9` is maximum compression. If you supply an output file name ending in `.gz`, the output file will be compressed by default using maximum compression.

-a \| --append
:   If the output file already exists, appends to the file instead of overwriting it.

**Input Options**

input\_file
:   The name of the input log file\(s\) to search through. If an input file is not supplied, `gplogfilter` will use the `$COORDINATOR_DATA_DIRECTORY` environment variable to locate the Greenplum Database coordinator log file. To read from standard input, use a dash \(`-`\) as the input file name.

-u \| --unzip
:   Uncompress the input file using `gunzip`. If the input file name ends in `.gz`, it will be uncompressed by default.

--help
:   Displays the online help.

--version
:   Displays the version of this utility.

## <a id="section9"></a>Examples 

Display the last three error messages in the coordinator log file:

```
gplogfilter -t -n 3
```

Display all log messages in the coordinator log file timestamped in the last 10 minutes:

```
gplogfilter -d :10
```

Display log messages in the coordinator log file containing the string `|con6 cmd11|`:

```
gplogfilter -f '|con6 cmd11|'
```

Using [gpssh](gpssh.html), run `gplogfilter` on the segment hosts and search for log messages in the segment log files containing the string `con6` and save output to a file.

```
gpssh -f seg_hosts_file -e 'source 
/usr/local/greenplum-db/greenplum_path.sh ; gplogfilter -f 
con6 /gpdata/*/log/gpdb*.csv' > seglog.out
```

## <a id="section10"></a>See Also 

[gpssh](gpssh.html), [gpsync](gpsync.html)

