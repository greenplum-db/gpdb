# coding=utf-8
import tempfile
import pytest
import TEST_local_base as TestBase
from TEST_local_base import prepare_before_test_2


def append_raw_sql(test_num, sql):
    f = open(TestBase.mkpath('query%s.sql' % test_num), 'a')
    f.write("%s;\n" % sql)
    f.close()


def append_sql(test_num, sql):
    f = open(TestBase.mkpath('query%s.sql' % test_num), 'a')
    f.write("\\! psql -d reuse_gptest -c \"%s;\"\n" % sql)
    f.close()


def append_gpload_cmd(test_num, config_path):
    f = open(TestBase.mkpath('query%s.sql' % test_num), 'a')
    f.write("\\! gpload -f %s\n" % config_path)
    f.close()


@pytest.mark.order(401)
@TestBase.prepare_before_test(num=401, times=1)
def test_401_gpload_yaml_existing_external_schema():
    """401 test gpload works with an existing external schema"""
    TestBase.drop_tables()
    schema = "'\"EXT_schema_test\"'"
    f = open(TestBase.mkpath('query401.sql'), 'a')
    f.write("\\! psql -d reuse_gptest -c \"select count(*) from pg_tables where schemaname = 'EXT_schema_test';\"")
    f.close()
    TestBase.write_config_file(externalSchema=schema, reuse_tables=True)


@pytest.mark.order(402)
@TestBase.prepare_before_test(num=402, times=1)
def test_402_gpload_yaml_non_existing_external_schema():
    """402 test gpload reports error when external schema doesn't exist."""
    TestBase.drop_tables()
    schema = "non_ext_schema_test"
    TestBase.psql_run(cmd='CREATE SCHEMA IF NOT EXISTS %s;' % schema,
                      dbname='reuse_gptest')
    TestBase.write_config_file(externalSchema=schema)


@pytest.mark.order(403)
@TestBase.prepare_before_test(num=403, times=1)
def test_403_gpload_yaml_percent_external_schema():
    """403 test gpload works with percent sign(%) to use the table's schema"""
    schema = "table_schema_test"
    TestBase.psql_run(cmd='CREATE SCHEMA IF NOT EXISTS %s;'%schema,
                      dbname='reuse_gptest')
    query = """CREATE TABLE %s.texttable (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);""" % schema
    TestBase.psql_run(cmd=query, dbname='reuse_gptest')
    TestBase.write_config_file(table='%s.texttable' % schema, externalSchema='\'%\'')


@pytest.mark.order(404)
@TestBase.prepare_before_test(num=404, times=1)
def test_404_gpload_yaml_percent_default_external_schema():
    """404 test gpload works with percent sign(%) to use the default table's\
    schema"""
    TestBase.drop_tables()
    TestBase.write_config_file(externalSchema='\'%\'')


@pytest.mark.order(405)
@TestBase.prepare_before_test(num=405, times=2)
def test_405_gpload_external_schema_merge():
    """405 test gpload works with an existing external schema "EXT_schema_test" """
    TestBase.drop_tables()
    schema = "'\"EXT_schema_test\"'"
    f = open(TestBase.mkpath('query405.sql'), 'a')
    f.write("\\! gpload -f "+TestBase.mkpath('config/config_file1')+'\n')
    f.write("\\! psql -d reuse_gptest -c \"select count(*) from pg_tables where schemaname = 'EXT_schema_test';\"")
    f.close()
    TestBase.write_config_file(externalSchema=schema, reuse_tables=True, mode='merge')
    TestBase.write_config_file(config='config/config_file1',externalSchema=schema, reuse_tables=True, mode='merge')


@pytest.mark.order(406)
@TestBase.prepare_before_test(num=406, times=2)
def test_406_gpload_external_schema_merge():
    """406 test gpload works with schema test and write as "Test" in config"""
    TestBase.drop_tables()
    schema = '"Test"'
    f = open(TestBase.mkpath('query406.sql'), 'a')
    f.write("\\! gpload -f "+TestBase.mkpath('config/config_file1')+'\n')
    f.write("\\! psql -d reuse_gptest -c \"select count(*) from pg_tables where schemaname = 'test';\"")
    f.close()
    TestBase.write_config_file(externalSchema=schema, reuse_tables=True, mode='merge')
    TestBase.write_config_file(config='config/config_file1',externalSchema=schema, reuse_tables=True, mode='merge')


@pytest.mark.order(430)
@TestBase.prepare_before_test(num=430, times=1)
def test_430_gpload_yaml_table_empty_string():
    """430 test gpload reports error if table is an empty string"""
    TestBase.write_config_file(table="")


@pytest.mark.order(431)
@TestBase.prepare_before_test(num=431, times=1)
def test_431_gpload_yaml_table_not_exist():
    """431 test gpload reports error if the target table doesn't exist"""
    TestBase.write_config_file(table="non_exist_table")


@pytest.mark.order(432)
@TestBase.prepare_before_test(num=432, times=1)
def test_432_gpload_yaml_table_schema_not_exist():
    """432 test gpload reports error if the target schema doesn't exist"""
    TestBase.write_config_file(table="non_exist_schema.table")


@pytest.mark.order(433)
@TestBase.prepare_before_test(num=433, times=1)
def test_433_gpload_yaml_table_with_schema():
    """433 test gpload works with schame in the table string"""
    schema = "table_schema_test"
    TestBase.psql_run(cmd='CREATE SCHEMA IF NOT EXISTS %s;' % schema,
                      dbname='reuse_gptest')
    query = """DROP TABLE %s.texttable IF EXISTS;
            CREATE TABLE %s.texttable (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);""" % (schema,schema)
    TestBase.psql_run(cmd=query, dbname='reuse_gptest')
    TestBase.write_config_file(table='%s.texttable' % schema)


@pytest.mark.order(440)
@prepare_before_test_2(num=440)
def test_440_gpload_yaml_table_name_special_char():
    """440 test gpload works with table name contains special chars"""
    test_num = 440
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            ("\"", ("s_\"\"_c", "'\"s_\"\"_c\"'")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            # FIXME: Not working ones
            # "'": ("s_'_c", "s_'_c"),
    ]
    for (c, escape) in spec_dict:
        table_name_sql = escape[0]
        table_name_yml = escape[1]
        append_raw_sql(test_num, "DROP TABLE IF EXISTS \"%s\"" % table_name_sql)
        append_raw_sql(
                test_num,
                """CREATE TABLE \"%s\"
                (c1 text, c2 int) DISTRIBUTED BY (c1);""" % table_name_sql)
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                config=config_path,
                table=table_name_yml,
                update_columns="c2",
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(441)
@prepare_before_test_2(num=441)
def test_441_gpload_yaml_schema_name_special_char():
    """441 test gpload works with schema name contains special chars"""
    test_num = 441
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            ("\"", ("s_\"\"_c", "'\"s_\"\"_c\"'")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            # FIXME: Not working ones
            # "'": ("s_'_c", "s_'_c"),
    ]

    for (c, escape) in spec_dict:
        name_sql = escape[0]
        name_yml = escape[1]
        append_raw_sql(
                test_num, "DROP SCHEMA IF EXISTS \"%s\" CASCADE;" % name_sql)
        append_raw_sql(
                test_num, "CREATE SCHEMA \"%s\";" % name_sql)
        append_raw_sql(
                test_num,
                """CREATE TABLE \"%s\"."test_table"
                (c1 text, c2 int) DISTRIBUTED BY (c1);""" % name_sql)
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                config=config_path,
                table="'\"%s\".\"test_table\"'" % name_yml,
                update_columns="c2",
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(442)
@prepare_before_test_2(num=442)
def test_442_gpload_yaml_external_schema_name_special_char():
    """442 test gpload works with schema name contains special chars"""
    test_num = 442
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            ("\"", ("s_\"\"_c", "'\"s_\"\"_c\"'")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            # FIXME: Not working ones
            # "'": ("s_'_c", "s_'_c"),
    ]

    for (c, escape) in spec_dict:
        name_sql = escape[0]
        name_yml = escape[1]
        append_raw_sql(
                test_num, "DROP SCHEMA IF EXISTS \"%s\" CASCADE;" % name_sql)
        append_raw_sql(
                test_num, "CREATE SCHEMA \"%s\";" % name_sql)
        append_raw_sql(
                test_num,
                """CREATE TABLE IF NOT EXISTS ext_schema_spec_char_test
                (c1 text, c2 int) DISTRIBUTED BY (c1);""")
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                config=config_path,
                externalSchema="'\"%s\"'" % name_yml,
                table="ext_schema_spec_char_test",
                update_columns="c2",
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(460)
@TestBase.prepare_before_test(num=460, times=1)
def test_460_gpload_mode_default():
    """460 test gpload works in the default insert mode"""
    TestBase.drop_tables()
    ok, out = TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode="")
    f = open(TestBase.mkpath('query460.sql'), 'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()


@pytest.mark.order(461)
@TestBase.prepare_before_test(num=461, times=1)
def test_461_gpload_mode_insert():
    """461 test gpload works in the insert mode"""
    TestBase.drop_tables()
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='insert')
    f = open(TestBase.mkpath('query461.sql'), 'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()


@pytest.mark.order(490)
@TestBase.prepare_before_test(num=490, times=1)
def test_490_gpload_mode_update_error_no_columns_in_yaml():
    """490 test gpload fails if UPDATE_COLUMNS is not specified in update mode"""
    TestBase.write_config_file(mode='update', update_columns=[])


@pytest.mark.order(491)
@TestBase.prepare_before_test(num=491, times=1)
def test_491_gpload_mode_update_error_no_columns_matches():
    """491 test gpload fails if UPDATE_COLUMNS doesn't match columns"""
    TestBase.write_config_file(mode='update', update_columns=['no_exists_column'])


@pytest.mark.order(492)
@TestBase.prepare_before_test(num=492, times=1)
def test_492_gpload_mode_update_error_any_columns_not_match():
    """492 test gpload fails if any of UPDATE_COLUMNS doesn't match columns"""
    TestBase.write_config_file(mode='update', update_columns=['n2', 'no_exists_column'])


def do_test_gpload_mode_update_one_match_column(
        test_num, column, base, target):
    append_sql(test_num, "TRUNCATE TABLE texttable;")
    # Insert some base data first
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(
            mode='insert', config=config_path, file=base)
    append_gpload_cmd(test_num, config_path)

    # Then do update
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(
            mode='update', config=config_path,
            match_columns=[column], update_columns=['s3'],
            file=target)
    append_gpload_cmd(test_num, config_path)
    append_sql(test_num, "SELECT COUNT(*) FROM texttable WHERE s3 = '42'")


@pytest.mark.order(493)
@prepare_before_test_2(num=493)
def test_493_gpload_mode_update_one_match_column():
    """493 test gpload works if MATCH_COLUMNS has one matched column"""
    TestBase.drop_tables()
    base = 'data/column_match_01.txt'
    target = 'data/column_match_target.txt'
    # text column
    do_test_gpload_mode_update_one_match_column(493, "s1", base, target)
    # timestamp column
    do_test_gpload_mode_update_one_match_column(493, "dt", base, target)
    # integer column
    do_test_gpload_mode_update_one_match_column(493, "n2", base, target)
    # bigint column
    do_test_gpload_mode_update_one_match_column(493, "n3", base, target)
    # decimal column
    do_test_gpload_mode_update_one_match_column(493, "n4", base, target)
    # numeric column
    do_test_gpload_mode_update_one_match_column(493, "n5", base, target)
    # real column
    do_test_gpload_mode_update_one_match_column(493, "n6", base, target)
    # precision column
    do_test_gpload_mode_update_one_match_column(493, "n7", base, target)



@pytest.mark.order(494)
@prepare_before_test_2(num=494)
def test_494_gpload_mode_update_multiple_match_columns():
    """494 test gpload works if MATCH_COLUMNS has multiple matched columns"""
    TestBase.drop_tables()
    append_sql(494, "TRUNCATE TABLE texttable;")
    base = 'data/column_match_02.txt'
    target = 'data/column_match_target.txt'
    # Insert some base data first
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(mode='insert', config=config_path, file=base)
    append_gpload_cmd(494, config_path)

    # Then do update
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(
            mode='update', config=config_path,
            match_columns=['s1', 's2'], update_columns=['s3'],
            file=target)
    append_gpload_cmd(494, config_path)
    append_sql(494, "SELECT COUNT(*) FROM texttable WHERE s3 = '42'")


@pytest.mark.order(495)
@prepare_before_test_2(num=495)
def test_495_gpload_mode_update_multiple_update_columns():
    """495 test gpload works if UPDATE_COLUMNS has multiple columns"""
    TestBase.drop_tables()
    append_sql(495, "TRUNCATE TABLE texttable;")
    base = 'data/column_match_03.txt'
    target = 'data/column_match_target.txt'
    # Insert some base data first
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(mode='insert', config=config_path, file=base)
    append_gpload_cmd(495, config_path)

    # Then do update
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(
            mode='update', config=config_path,
            match_columns=['s1'], update_columns=['s2', 's3'],
            file=target)
    append_gpload_cmd(495, config_path)
    append_sql(495, "SELECT * FROM texttable")


@pytest.mark.order(496)
@prepare_before_test_2(num=496)
def test_496_gpload_mode_update_update_condition():
    """496 test gpload works with UPDATE_CONDITION"""
    TestBase.drop_tables()
    append_sql(496, "TRUNCATE TABLE texttable;")
    base = 'data/column_match_04.txt'
    target = 'data/column_match_target.txt'
    # Insert some base data first
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(mode='insert', config=config_path, file=base)
    append_gpload_cmd(496, config_path)

    # Then do update
    config_fd, config_path = tempfile.mkstemp()
    TestBase.write_config_file(
            mode='update', config=config_path,
            match_columns=['s1'], update_columns=['s2', 's3'],
            update_condition="n1 = 42",
            file=target)
    append_gpload_cmd(496, config_path)
    append_sql(496, "SELECT * FROM texttable")


@pytest.mark.order(497)
@TestBase.prepare_before_test(num=497, times=1)
def test_497_gpload_mode_udpate_wrong_update_conditino_reports_error():
    """497 illegal update condition should report error"""
    TestBase.write_config_file(mode='update', update_condition='non_col = 5')


@pytest.mark.order(499)
@TestBase.prepare_before_test(num=499, times=1)
def test_499_gpload_mode_update_error_match_column_cannot_be_dist_key():
    """499 test gpload reports error if UPDATE_COLUMNS contains a dist key"""
    TestBase.write_config_file(mode='update', update_columns=['n1'])


@pytest.mark.order(500)
@TestBase.prepare_before_test(num=500, times=1)
def test_500_gpload_mode_merge_same_as_insert_for_empty_table():
    """500 test gpload works in merge mode and target table is empty"""
    TestBase.drop_tables()
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='merge')
    f = open(TestBase.mkpath('query500.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()


@pytest.mark.order(501)
@TestBase.prepare_before_test(num=501, times=1)
def test_501_gpload_mode_merge_same_data_twice():
    """501 test gpload works in merge mode and load same data twice"""
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='merge')
    f = open(TestBase.mkpath('query501.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()


@pytest.mark.order(502)
@TestBase.prepare_before_test(num=502, times=1)
def test_502_gpload_mode_merge_do_update_and_insert():
    """502 test gpload works in merge mode and do update and insert at the same
    time"""
    TestBase.drop_tables()
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='merge')
    TestBase.write_config_file(
            mode='merge', config='config/config_file2',
            file='data/external_file_502.txt', null_as='NUL')
    f = open(TestBase.mkpath('query502.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file2') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select * from texttable order by s1, s2, n1;'")
    f.close()


@pytest.mark.order(503)
@TestBase.prepare_before_test(num=503, times=1)
def test_503_gpload_mode_merge_do_update_and_insert_with_update_condition():
    """502 test gpload works in merge mode and do update and insert at the
    same time"""
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='merge')
    TestBase.write_config_file(
            mode='merge', config='config/config_file2',
            file='data/external_file_502.txt', null_as='NUL',
            update_condition="s1 != 'ggg'")
    f = open(TestBase.mkpath('query503.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file2') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select * from texttable order by s1, s2, n1;'")
    f.close()


def do_test_merge(test_num, match_columns, update_columns):
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    append_raw_sql(test_num, "DROP TABLE IF EXISTS merge_test")
    append_raw_sql(
            test_num,
            "CREATE TABLE merge_test(c1 text, c2 text, c3 text, c4 int) "
            "DISTRIBUTED BY(c4)")
    append_raw_sql(
            test_num,
            "INSERT INTO merge_test VALUES ('aaa', 'init', 'init', 0)")
    TestBase.write_config_file(
            mode='merge',
            columns=[
                ('c1', 'text'), ('c2', 'text'), ('c3', 'text'), ('c4', 'timestamp'),
                ('c5', 'smallint'), ('c6', 'integer'), ('c7', 'bigint'),
                ('c8', 'decimal'), ('c9', 'numeric'), ('c10', 'real'),
                ('c11', 'double precision')],
            table='merge_test',
            file='data/column_mapping_01.txt',
            match_columns=match_columns,
            update_columns=update_columns,
            mapping={'c1': 'c1', 'c2': 'c2', 'c3': 'c3', 'c4': 'c5'})
    append_gpload_cmd(test_num, "config/config_file")
    append_raw_sql(test_num, "SELECT * FROM merge_test")


@pytest.mark.order(504)
@prepare_before_test_2(num=504)
def test_504_gpload_mode_merge_same_update_match_column():
    """504 test gpload can use the same column in both UPDATE_COLUMNS and
    MATCH_COLUMNS."""
    do_test_merge(504, ['c1'], ['c1'])


@pytest.mark.order(505)
@prepare_before_test_2(num=505)
def test_505_gpload_mode_merge_multiple_match_column_no_match():
    """505 test merge mode multiple MATCH_COLUMNS don't match"""
    do_test_merge(505, ['c1', 'c2'], ['c1'])


@pytest.mark.order(506)
@prepare_before_test_2(num=506)
def test_506_gpload_mode_merge_empty_match_column_reports_error():
    "506 test merge mode empty MATCH_COLUMNS reports errors"
    do_test_merge(506, [], ['c1'])


@pytest.mark.order(507)
@prepare_before_test_2(num=507)
def test_507_gpload_mode_merge_empty_update_column_reports_error():
    "507 test merge mode empty UPDATE_COLUMNS reports errors"
    do_test_merge(507, ['c1'], [])


@pytest.mark.order(508)
@prepare_before_test_2(num=508)
def test_508_gpload_mode_merge_no_exist_match_column_reports_error():
    "508 test merge mode non-existing MATCH_COLUMNS reports errors"
    do_test_merge(508, ['cannot_see_me'], ['c1'])


@pytest.mark.order(509)
@prepare_before_test_2(num=509)
def test_509_gpload_mode_merge_no_exist_update_column_reports_error():
    "509 test merge mode non-existing UPDATE_COLUMNS reports errors"
    do_test_merge(509, ['c1'], ['cannot_see_me'])


def do_test_mapping(test_num, mapping):
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    append_raw_sql(test_num, "DROP TABLE IF EXISTS mapping_test")
    append_raw_sql(
            test_num,
            "CREATE TABLE mapping_test(s1 text, s2 text, s3 text, s4 int) DISTRIBUTED BY (s1)")
    TestBase.write_config_file(
            mode='insert',
            columns=[
                ('c1', 'text'), ('c2', 'text'), ('c3', 'text'), ('c4', 'timestamp'),
                ('c5', 'smallint'), ('c6', 'integer'), ('c7', 'bigint'),
                ('c8', 'decimal'), ('c9', 'numeric'), ('c10', 'real'),
                ('c11', 'double precision')],
            mapping=mapping,
            table='mapping_test',
            file='data/column_mapping_01.txt')
    append_gpload_cmd(test_num, "config/config_file")
    append_raw_sql(test_num, "SELECT * FROM mapping_test")


@pytest.mark.order(520)
@prepare_before_test_2(num=520)
def test_520_gpload_mode_insert_mapping():
    "520 test gpload insert with mapping works."
    mapping = {'s3': 'c1', 's2': 'c2', 's1': 'c3'}
    do_test_mapping(520, mapping)


@pytest.mark.order(521)
@prepare_before_test_2(num=521)
def test_521_gpload_mode_insert_mapping_target_not_exist():
    """521 test gpload insert with mapping to a non-exists target column
    reports error."""
    mapping = {'n1': 'c1', 's2': 'c2', 's3': 'c3'}
    do_test_mapping(521, mapping)


@pytest.mark.order(522)
@prepare_before_test_2(num=522)
def test_522_gpload_mode_insert_mapping_source_not_exist():
    """522 test gpload insert with mapping to a non-exists source column
    reports error."""
    mapping = {'s1': 'c1', 's2': 'c2', 's3': 'n3'}
    do_test_mapping(522, mapping)


@pytest.mark.order(523)
@prepare_before_test_2(num=523)
def test_523_gpload_mode_insert_mapping_type_not_match():
    """523 test gpload insert with mismatched type mapping reports error."""
    mapping = {'s4': 'c1', 's2': 'c2', 's3': 'c3'}
    do_test_mapping(523, mapping)


@pytest.mark.order(524)
@prepare_before_test_2(num=524)
def test_524_gpload_mode_insert_mapping_expression_function_multi_col():
    """524 test gpload insert simple function expression mapping with multiple
    source target columns as input argments."""
    mapping = {'s1': 'concat(c1, \'_\', c2)'}
    do_test_mapping(524, mapping)


@pytest.mark.order(525)
@prepare_before_test_2(num=525)
def test_525_gpload_mode_insert_mapping_expression_constant():
    """525 test gpload insert function expression mapping"""
    mapping = {'s1': '"\'const_str\'"'}
    do_test_mapping(525, mapping)


@pytest.mark.order(526)
@prepare_before_test_2(num=526)
def test_526_gpload_mode_insert_mapping_expression_operator():
    """526 test gpload insert operator expression mapping"""
    mapping = {'s1': 'c1 = \'aaa\''}
    do_test_mapping(526, mapping)


@pytest.mark.order(527)
@prepare_before_test_2(num=527)
def test_527_gpload_mode_insert_mapping_udf_expression_operator():
    """527 test gpload insert UDF expression mapping"""
    append_raw_sql(527, "\\c reuse_gptest")
    append_raw_sql(527, """
        CREATE OR REPLACE FUNCTION increment_527(i integer)
        RETURNS integer AS $$
        BEGIN
                RETURN i + 1;
        END;
        $$ LANGUAGE plpgsql""")
    mapping = {'s1': 'increment_527(41)'}
    do_test_mapping(527, mapping)


@pytest.mark.order(528)
@prepare_before_test_2(num=528)
def test_528_gpload_mode_insert_mapping_expression_mixed():
    """528 test gpload insert mapping mixed with column and different type of
    expressions"""
    mapping = {
            's1': 'c1 = \'aaa\'',
            's2': 'concat(c2, \'_postfix\')',
            's3': 'c3',
            's4': '(5)'}
    do_test_mapping(528, mapping)


@pytest.mark.order(529)
@prepare_before_test_2(num=529)
def test_529_gpload_mode_insert_mapping_expression_no_exists_udf():
    """529 test gpload insert mapping UDF expression doesn't exist"""
    mapping = {'s1': 'rocket_bites(\'frog\')'}
    do_test_mapping(529, mapping)


def do_test_mapping_update_merge(test_num, mapping, mode):
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    append_raw_sql(test_num, "DROP TABLE IF EXISTS mapping_test")
    append_raw_sql(
            test_num,
            "CREATE TABLE mapping_test(s1 text, s2 text, s3 text, s4 int) "
            "DISTRIBUTED BY(s1)")
    append_raw_sql(
            test_num,
            "INSERT INTO mapping_test VALUES ('aaa', '', '', 0)")
    TestBase.write_config_file(
            mode=mode,
            columns=[
                ('c1', 'text'), ('c2', 'text'), ('c3', 'text'), ('c4', 'timestamp'),
                ('c5', 'smallint'), ('c6', 'integer'), ('c7', 'bigint'),
                ('c8', 'decimal'), ('c9', 'numeric'), ('c10', 'real'),
                ('c11', 'double precision')],
            mapping=mapping,
            table='mapping_test',
            file='data/column_mapping_01.txt',
            match_columns=['s1'],
            update_columns=['s2', 's3'])
    append_gpload_cmd(test_num, "config/config_file")
    append_raw_sql(test_num, "SELECT * FROM mapping_test")


@pytest.mark.order(530)
@prepare_before_test_2(num=530)
def test_530_gpload_mode_update_mapping():
    "530 test gpload insert with mapping works."
    mapping = {'s1': 'c1', 's2': 'c3', 's3': 'c2'}
    do_test_mapping_update_merge(530, mapping, 'update')


@pytest.mark.order(531)
@prepare_before_test_2(num=531)
def test_531_gpload_mode_merge_mapping():
    "531 test gpload merge with mapping works."
    mapping = {'s1': 'c1', 's2': 'c3', 's3': 'c2'}
    do_test_mapping_update_merge(531, mapping, 'merge')


@pytest.mark.order(532)
@prepare_before_test_2(num=532)
def test_532_gpload_mode_update_mapping_mismatch_type():
    "532 test gpload insert with mapping works."
    mapping = {'s1': 'c1', 's2': 'c3', 's4': 'c2'}
    do_test_mapping_update_merge(532, mapping, 'update')


@pytest.mark.order(533)
@prepare_before_test_2(num=533)
def test_533_gpload_mode_merge_mapping_mismatch_type():
    "533 test gpload merge with mapping works."
    mapping = {'s1': 'c1', 's2': 'c3', 's4': 'c2'}
    do_test_mapping_update_merge(533, mapping, 'merge')


@pytest.mark.order(534)
@prepare_before_test_2(num=534)
def test_534_gpload_mode_update_mapping_expression():
    "534 test gpload update with expression mapping works."
    mapping = {'s1': 'c1', 's2': 'concat(c2, \'_\', c3)', 's3': '(42)'}
    do_test_mapping_update_merge(534, mapping, 'update')


@pytest.mark.order(535)
@prepare_before_test_2(num=535)
def test_535_gpload_mode_merge_mapping_expression():
    "535 test gpload update with expression mapping works."
    mapping = {'s1': 'c1', 's2': 'concat(c2, \'_\', c3)', 's3': '(42)'}
    do_test_mapping_update_merge(535, mapping, 'merge')


@pytest.mark.order(540)
@prepare_before_test_2(num=540)
def test_540_gpload_yaml_update_column_special_char():
    "540 test gpload works with table name contains special chars"
    TestBase.drop_tables()
    test_num = 540
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            ("'", ("s_'_c", "s_'_c")),
            # FIXME: Not working ones
            # "\"": ("s_\"\"_c", "'\"s_\\\\\"_c\"'"),
    ]

    for (c, escape) in spec_dict:
        name_sql = escape[0]
        name_yml = escape[1]
        append_raw_sql(
                test_num, "DROP TABLE IF EXISTS update_column_special_char")
        append_raw_sql(
                test_num,
                """CREATE TABLE update_column_special_char
                (c1 text, "%s" int) DISTRIBUTED BY (c1);""" % name_sql)
        append_raw_sql(
                test_num,
                "INSERT INTO update_column_special_char VALUES('a', 0)")
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                mode="update",
                config=config_path,
                table="update_column_special_char",
                match_columns=["c1"],
                update_columns=[name_yml],
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(541)
@prepare_before_test_2(num=541)
def test_541_gpload_yaml_match_column_special_char():
    "541 test gpload works with table name contains special chars"
    test_num = 541
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            ("'", ("s_'_c", "s_'_c")),
            # FIXME: Not working ones
            # "\"": ("s_\"\"_c", "'\"s_\\\\\"_c\"'"),
    ]

    for (c, escape) in spec_dict:
        name_sql = escape[0]
        name_yml = escape[1]
        append_raw_sql(
                test_num, "DROP TABLE IF EXISTS match_column_special_char")
        append_raw_sql(
                test_num,
                """CREATE TABLE match_column_special_char
                ("%s" text, c2 int) DISTRIBUTED BY ("%s");""" % (name_sql,name_sql))
        append_raw_sql(
                test_num,
                "INSERT INTO match_column_special_char VALUES('a', 0)")
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                mode="update",
                config=config_path,
                table="match_column_special_char",
                match_columns=["%s" % name_yml],
                update_columns=["c2"],
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(542)
@prepare_before_test_2(num=542)
def test_542_gpload_yaml_mapping_target_special_char():
    "541 test gpload works with table name contains special chars"
    test_num = 542
    TestBase.drop_tables()
    append_raw_sql(test_num, "\\c reuse_gptest")
    # "char": ("escape_in_sql", "escape_in_yml")
    spec_dict = [
            ("\\", ("s_\\_c", "s_\\_c")),
            ("$", ("s_$_c", "s_$_c")),
            ("#", ("s_#_c", "s_#_c")),
            (",", ("s_,_c", "s_,_c")),
            ("(", ("s_(_c", "s_(_c")),
            (".", ("s_._c", "'\"s_._c\"'")),
            ("/", ("s_/_c", "s_/_c")),
            ("'", ("s_'_c", "s_'_c")),
            # FIXME: Not working ones
            # "\"": ("s_\"\"_c", "'\"s_\\\\\"_c\"'"),
    ]

    for (c, escape) in spec_dict:
        name_sql = escape[0]
        name_yml = escape[1]
        append_raw_sql(
                test_num, "DROP TABLE IF EXISTS match_column_special_char")
        append_raw_sql(
                test_num,
                """CREATE TABLE match_column_special_char
                ("%s" text, c2 int) DISTRIBUTED BY ("c2");""" % name_sql)
        config_fd, config_path = tempfile.mkstemp()
        TestBase.write_config_file(
                columns=[('c1', 'text'), ('c2', 'integer')],
                mapping={"%s" % name_yml: 'c1'},
                config=config_path,
                table="match_column_special_char",
                file="data/two_col_one_row.txt")
        append_gpload_cmd(test_num, config_path)


@pytest.mark.order(543)
@TestBase.prepare_before_test(num=543, times=1)
def test_543_gpload_mode_merge_insert_with_update_condition():
    """543 test gpload works in merge mode and do insert with update_condation
    same time"""
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='insert')
    TestBase.write_config_file(
            mode='merge', config='config/config_file2',
            file='data/external_file_543.txt', null_as='NUL',
            update_condition="n7 > 123")
    f = open(TestBase.mkpath('query543.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file2') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select * from texttable order by s1, s2, n1;'")
    f.close()


@pytest.mark.order(544)
@TestBase.prepare_before_test(num=544, times=1)
def test_544_gpload_mode_merge_update_with_update_condition():
    """544 test gpload works in merge mode and do insert with update_condation
    same time"""
    TestBase.psql_run(cmd='TRUNCATE TABLE texttable', dbname='reuse_gptest')
    TestBase.write_config_file(mode='insert', file='data/external_file_543.txt')
    TestBase.write_config_file(
            mode='merge', config='config/config_file2',
            file='data/external_file_544.txt', null_as='NUL',
            update_condition="n7 > 123")
    f = open(TestBase.mkpath('query544.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file2') + "\n")
    f.write("\\! psql -d reuse_gptest -c 'select * from texttable order by s1, s2, n1;'")
    f.close()


@pytest.mark.order(545)
@TestBase.prepare_before_test(num=545, times=1)
def test_545_gpload_merge_staging_DK():
    """545 test gpload merge using target table distribution key as
    staging table DK as default"""
    file = TestBase.mkpath('setup.sql')
    TestBase.runfile(file)
    TestBase.copy_data('external_file_04.txt', 'data_file.txt')
    TestBase.write_config_file(mode='merge', file='data_file.txt', table='testtruncate', reuse_tables=True)
    f = open(TestBase.mkpath('query545.sql'), 'a')
    f.write("\\! psql -d reuse_gptest -c '\\d staging_gpload_reusable_*'")
    f.close()


@pytest.mark.order(546)
@TestBase.prepare_before_test(num=546, times=1)
def test_546_gpload_merge_staging_DK():
    """546 test gpload merge using match column as staging table DK 
    when target is DISTRIBUTED RANDOMLY"""
    file = TestBase.mkpath('setup.sql')
    TestBase.runfile(file)
    TestBase.copy_data("external_file_47.txt", "data_file.txt")
    TestBase.write_config_file(mode='insert', file='data_file.txt', table='testheaderreuse', delimiter="','", reuse_tables=False)
    match_col = ['field1']
    update_col = ['field2']
    TestBase.copy_data("external_file_546.txt", "data_file1.txt")
    TestBase.write_config_file(mode='merge', file='data_file1.txt', update_columns=update_col,delimiter="','",
        match_columns=match_col, config='config/config_file1', table='testheaderreuse', reuse_tables=True)
    f = open(TestBase.mkpath('query546.sql'), 'w')
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file') + "\n")
    f.write("\\! gpload -f " + TestBase.mkpath('config/config_file1') + "\n")
    f.write("\\! psql -d reuse_gptest -c '\\d staging_gpload_reusable_*'")
    f.close()

@pytest.mark.order(547)
@TestBase.prepare_before_test(num=547, times=1)
def test_547_gpload_insert_without_DK():
    """547 test gpload merge distribution key mismatch, use match column instead.
    """
    file = TestBase.mkpath('setup.sql')
    TestBase.runfile(file)
    TestBase.copy_data('column_match_05.txt', 'data_file.txt')
    TestBase.write_config_file(columns=[("s1", "text"), ("s2", "text"), ("s3", "text"), ("dt", "timestamp"), ("n2", "int"), ("n3", "int"), ("n4", "numeric"), ("n5", "numeric"), ("n6", "real"), ("n7", "double precision")],
                               mode='merge', file='data_file.txt', table='testtruncate', reuse_tables=True,
                               match_columns=["dt"], update_columns=['s3'])
    f = open(TestBase.mkpath('query547.sql'), 'a')
    f.write("\\! psql -d reuse_gptest -c '\\d staging_gpload_reusable_*'")
    f.close()
