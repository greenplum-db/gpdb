# Checking the Log Files for Failed Segments 

Log files can provide information to help determine an error's cause. The master and segment instances each have their own log file in `pg_log` of the data directory. The master log file contains the most information and you should always check it first.

Use the `gplogfilter` utility to check the Greenplum Database log files for additional information. To check the segment log files, run `gplogfilter` on the segment hosts using `gpssh`.

## To check the log files 

1.  Use `gplogfilter` to check the master log file for `WARNING`, `ERROR`, `FATAL` or `PANIC` log level messages:

    ```
    $ gplogfilter -t
    ```

2.  Use `gpssh` to check for `WARNING`, `ERROR`, `FATAL`, or `PANIC` log level messages on each segment instance. For example:

    ```
    $ gpssh -f seg_hosts_file -e 'source 
    /usr/local/greenplum-db/greenplum_path.sh ; gplogfilter -t 
    /data1/primary/*/pg_log/gpdb*.log' > seglog.out
    
    ```


**Parent topic:** [Detecting a Failed Segment](../../highavail/topics/g-detecting-a-failed-segment.html)

