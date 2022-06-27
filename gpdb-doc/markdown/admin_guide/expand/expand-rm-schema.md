# Removing the Expansion Schema 

To clean up after expanding the Greenplum cluster, remove the expansion schema.

You can safely remove the expansion schema after the expansion operation is complete and verified. To run another expansion operation on a Greenplum system, first remove the existing expansion schema.

## To remove the expansion schema 

1.  Log in on the master host as the user who will be running your Greenplum Database system \(for example, `gpadmin`\).
2.  Run the `gpexpand` utility with the `-c` option. For example:

    ```
    $ gpexpand -c 
    $ 
    ```

    **Note:** Some systems require you to press Enter twice.


**Parent topic:** [Expanding a Greenplum System](../expand/expand-main.html)

