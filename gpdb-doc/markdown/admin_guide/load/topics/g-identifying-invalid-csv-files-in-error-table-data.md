# Identifying Invalid CSV Files in Error Table Data 

If a CSV file contains invalid formatting, the *rawdata* field in the error table can contain several combined rows. For example, if a closing quote for a specific field is missing, all the following newlines are treated as embedded newlines. When this happens, Greenplum stops parsing a row when it reaches 64K, puts that 64K of data into the error table as a single row, resets the quote flag, and continues. If this happens three times during load processing, the load file is considered invalid and the entire load fails with the message "`rejected` `N` `or more rows`". See [Escaping in CSV Formatted Files](g-escaping-in-csv-formatted-files.html) for more information on the correct use of quotes in CSV files.

**Parent topic:** [Handling Load Errors](../../load/topics/g-handling-load-errors.html)

