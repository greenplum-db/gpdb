from TEST_local_base import write_config_file, psql_run, mkpath
from TEST_local_base import prepare_before_test, drop_tables, runfile
from TEST_local_base import runfile, copy_data, run

@prepare_before_test(num=601, times=1)
def test_601_with_trucate_true():
    "601 gpload with truncate is true"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query601.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from testtruncate;'")
    f.close()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=False,fast_match=False,file='data_file.txt',table='testtruncate', error_limit=1002, truncate=True)

@prepare_before_test(num=602, times=2)
def test_602_with_reuse_tables_true():
    "602 gpload with reuse_tables is true"
    drop_tables()
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query602.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c \"SELECT count(*) from pg_class WHERE relname like 'ext_gpload_reusable%' OR relname like 'staging_gpload_reusable%';\"")
    f.close()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', error_limit=1002, truncate=False)

''' The case 603 ~ 607 depend on the previous one, they should be serially tested together '''
@prepare_before_test(num=603, times=2)
def test_603_with_reuse_tables_true_and_staging():
    "603 gpload with reuse_tables is true and staging_table"
    drop_tables()
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query603.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c \"SELECT count(*) from pg_class WHERE relname = 'STAGING_test';\"")
    f.close()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', staging_table='\'"STAGING_test"\'', error_limit=1002, truncate=False)

@prepare_before_test(num=604, times=1)
def test_604_with_reuse_tables_true_table_changed():
    "604 gpload with reuse_tables is true and specifying a staging_table, but table's schema is changed. so staging table dosen't match the destination table, load failed"
    psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', staging_table='\'"STAGING_test"\'', error_limit=1002, truncate=False)

@prepare_before_test(num=605, times=1)
def test_605_with_reuse_tables_true_table_changed():
    "605 gpload with reuse_tables is true, but table's schema is changed, the staging table isn't reused."
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', error_limit=1002, truncate=False)

@prepare_before_test(num=606, times=1)
def test_606_with_reuse_tables_true_port_changed():
    "606 gpload with reuse_tables is true, but port is changed, external table is not reused"
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', input_port='8899',error_limit=1002, truncate=False)

def test_607_clear_database():
    "drop tables in database and recreate them to clear the environment"
    file = mkpath('setup.sql')
    runfile(file)

@prepare_before_test(num=608, times=2)
def test_608_gpload_ext_staging_table():
    "608 gpload ignore staging_table if the reuse is false"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=False, format='csv', file='data_file.csv', table='csvtable', delimiter="','", log_errors=True,error_limit=10,staging_table='"Staging_table"')
