---
title: Example 3—Multiple gpfdists instances 
---

Creates a readable external table, *ext\_expenses,* from all files with the *txt* extension using the `gpfdists` protocol. The column delimiter is a pipe \( \| \) and NULL \(' '\) is a space. For information about the location of security certificates, see [gpfdists:// Protocol](g-gpfdists-protocol.html).

1.  Run `gpfdist` with the `--ssl` option.
2.  Run the following command.

    ```
    =# CREATE EXTERNAL TABLE ext_expenses ( name text, 
       date date,  amount float4, category text, desc1 text ) 
       LOCATION ('gpfdists://etlhost-1:8081/*.txt', 
                 'gpfdists://etlhost-2:8082/*.txt')
       FORMAT 'TEXT' ( DELIMITER '|' NULL ' ') ;
    
    ```


**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

