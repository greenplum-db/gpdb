# pg_upgrade integration tests

## Running GPDB5 to GPDB6 upgrade tests

Step 1: setup environment

    ./scripts/init-gpdb5-cluster.bash [INSTALLATION DIRECTORY] [SOURCE DIRECTORY]
    ./scripts/init-gpdb6-cluster.bash [INSTALLATION DIRECTORY] [SOURCE DIRECTORY]

    example:

    ./scripts/init-gpdb5-cluster.bash \
        /Users/adamberlin/workspace/gpdb5/gpAux/greenplum-db-installation \
        /Users/adamberlin/workspace/gpdb5

Step 2: run tests

    make check