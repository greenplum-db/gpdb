# pg_upgrade integration tests

## Running GPDB5 to GPDB6 upgrade tests

Step 1: setup environment

    # setup installations for 5 and 6
    ln -s /some/path/to/gpdb6/installation gpdb6
    ln -s /some/path/to/gpdb5/installation gpdb5

    # setup demo clusters for 5 and 6
    source configuration/gpdb5-env.sh && \ 
        source ./gpdb5/greenplum_path.sh && \
        make -C /some/path/to/gpdb5/source/gpAux/gpdemo && \
        gpstop -a; # ensure cluster is stopped

    source configuration/gpdb6-env.sh && \ 
        source ./gpdb6/greenplum_path.sh && \
        make -C /some/path/to/gpdb6/source/gpAux/gpdemo && \
        gpstop -a; # ensure cluster is stopped
        
    # make a backup of the original data directories
    cp -r gpdb5-data gpdb5-data-copy
    cp -r gpdb6-data gpdb6-data-copy

Step 2: run tests

    make check