gp_array_agg aggregate
========================
This extension provides an alternative to in-built array_agg. gp_array_agg
supports parallel aggregation.




Performance
-----------
TODO

Versions
--------
TODO

Installation
------------
Installing this is very simple, especially if you're using pgxn client.
All you need to do is this:



    $ make USE_PGXS=1 install
    $ psql dbname -c "CREATE EXTENSION gp_array_agg"


License
-------
TODO
