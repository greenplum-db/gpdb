#!/usr/bin/env pytest

from TEST_local_base import write_config_file, psql_run, mkpath
from TEST_local_base import prepare_before_test, drop_tables, runfile
from TEST_local_base import runfile, copy_data, run

@prepare_before_test(num=651,times=1)
def test_651_gpload_sql_before_success():
    "651 gpload executes `before sql` successfully"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query651.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM test_651'")
    f.close()
    sql = '''CREATE TABLE texttable_651 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_651 (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_651',sql=True,before="INSERT INTO test_651 VALUES(1)")

@prepare_before_test(num=652, times=1)
def test_652_gpload_sql_before_fail():
    "652 gpload fails to execute `before sql`"
    file = mkpath('setup.sql')
    runfile(file)
    sql = '''CREATE TABLE texttable_652 (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_652',sql=True,before="INSERT INTO test_652 VALUES(1)")

@prepare_before_test(num=653, times=1)
def test_653_gpload_sql_before_creating_table():
    "653 gpload creates target table in `before sql`"
    # Actually, gpload doesn't support doing this
    # as it firstly checks if the target table exists,
    # then executes the 'before sql'

    file = mkpath('setup.sql')
    runfile(file)
    sql = '''CREATE TABLE texttable_653 (c1 int);'''
    write_config_file(format='text',table='texttable_653',sql=True,before=sql)

@prepare_before_test(num=661, times=1)
def test_661_gpload_sql_after_success():
    "661 gpload executes `after sql` successfully"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query661.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_661'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_661'\n")
    f.close()
    sql = '''CREATE TABLE texttable_661 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_661(c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_661',sql=True,after="INSERT INTO test_661 VALUES(1)")

@prepare_before_test(num=662,times=1)
def test_662_gpload_sql_after_fail():
    "662 gpload fails to execute `after sql`"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query661.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_662'")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) texttable_662'\n")
    f.close()
    sql = '''CREATE TABLE texttable_662 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_662',sql=True,after="INSERT INTO test_662 VALUES(1)")

@prepare_before_test(num=663,times=1)
def test_663_gpload_sql_before_after():
    "663 gpload's before and after not in a transaction, both success"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query663.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_663_before'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_663_after'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_663'\n")
    f.close()
    sql = '''CREATE TABLE texttable_663 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_663_before (c1 int);
             CREATE TABLE test_663_after (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_663',sql=True,before="INSERT INTO test_663_before VALUES(1)",after="INSERT INTO test_663_after VALUES(2)")

@prepare_before_test(num=664,times=1)
def test_664_gpload_sql_before_after():
    "664 gpload's before and after not in a transaction, before sql fails"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query664.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_664_before'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_664_after'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_664'\n")
    f.close()
    sql = '''CREATE TABLE texttable_664 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_664_before (c1 int);
             CREATE TABLE test_664_after (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_664',sql=True,before="INSERT INTO test_664_before VALUES('a')",after="INSERT INTO test_664_after VALUES(2)")

@prepare_before_test(num=665,times=1)
def test_665_gpload_sql_before_after():
    "665 gpload's before and after not in a transaction, after sql fails"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query665.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_665_before'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_665_after'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_665'\n")
    f.close()
    sql = '''CREATE TABLE texttable_665 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_665_before (c1 int);
             CREATE TABLE test_665_after (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(format='text',table='texttable_665',sql=True,before="INSERT INTO test_665_before VALUES(1)",after="INSERT INTO test_665_after VALUES('a')")

@prepare_before_test(num=666,times=1)
def test_666_gpload_sql_before_after():
    "666 gpload's before and after in a transaction, before sql fails"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query666.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_666_before'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_666_after'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_666'\n")
    f.close()
    sql = '''CREATE TABLE texttable_666 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_666_before (c1 int);
             CREATE TABLE test_666_after (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(mode='merge',format='text',table='texttable_666',sql=True,before="INSERT INTO test_666_before VALUES('a')",after="INSERT INTO test_666_after VALUES(2)")

@prepare_before_test(num=667,times=1)
def test_667_gpload_sql_before_after():
    "667 gpload's before and after in a transaction, after sql fails"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query667.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_667_before'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT * FROM test_667_after'\n")
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable_667'\n")
    f.close()
    sql = '''CREATE TABLE texttable_667 (
             s1 text, s2 text, s3 text, dt timestamp, n1 smallint, n2 integer, n3 bigint, n4 decimal,
             n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
             CREATE TABLE test_667_before (c1 int);
             CREATE TABLE test_667_after (c1 int);'''
    (ok, out) = psql_run(cmd=sql,dbname='reuse_gptest')
    if not ok:
        raise Exception("Unable to execute sql %s" % out)
    write_config_file(mode='merge',format='text',table='texttable_667',sql=True,before="INSERT INTO test_667_before VALUES(1)",after="INSERT INTO test_667_after VALUES('a')")
