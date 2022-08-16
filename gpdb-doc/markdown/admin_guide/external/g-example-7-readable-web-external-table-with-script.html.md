---
title: Example 7—Readable External Web Table with Script 
---

Creates a readable external web table that runs a script once per segment host:

```
=# CREATE EXTERNAL WEB TABLE log_output (linenum int, 
    message text) 
   EXECUTE '/var/load_scripts/get_log_data.sh' ON HOST 
   FORMAT 'TEXT' (DELIMITER '|');

```

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

