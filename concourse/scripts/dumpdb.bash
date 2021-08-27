#!/bin/bash
set -e
set -u

source /usr/local/greenplum-db-devel/greenplum_path.sh
source ./gpdb_src/gpAux/gpdemo/gpdemo-env.sh

# launch the cluster when necessary
psql -d postgres -qt -P pager=off -c 'select 1' >/dev/null 2>&1 || gpstart -a

psql \
    -X \
    -c "select datname from pg_database where datname != 'template0'" \
    --set ON_ERROR_STOP=on \
    --no-align \
    -t \
    --field-separator ' ' \
    --quiet \
    template1 | while read -r database ; do

    echo "-----------------------------------------------------------------"
    echo "Drop objects not shipped to customers from DATABASE: ${database}."
    echo "-----------------------------------------------------------------"
    psql "${database}" -f ./gpdb_src/concourse/scripts/drop_functions_with_dependencies_not_shipped_to_customers.sql
    echo ""
    echo ""
done

pg_dumpall -f ./sqldump/dump.sql
xz -z ./sqldump/dump.sql
