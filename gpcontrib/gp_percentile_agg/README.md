gp_percentile_agg aggregate
========================
This extension provides an alternative to in-built percentile_cont and
percentile_disc ordered aggregates. gp_percentile_cont and gp_percentile_disc
supports percentile calculation based on the total count assuming data is
passed in sorted.

Installation
------------
Installing this is very simple, especially if you're using pgxn client.
All you need to do is this:



    $ make USE_PGXS=1 install
    $ psql dbname -c "CREATE EXTENSION gp_percentile_agg"
