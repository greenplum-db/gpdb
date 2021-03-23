from TEST_local_base import *
# coding=utf-8

# clean the database environment before running lecacy cases
# in case that pytest run legacy in different order when new files are merged and that may lead case fail
runfile(mkpath('setup.sql'))

@prepare_before_test(num=1)
def test_01_gpload_formatOpts_delimiter():
    "1  gpload formatOpts delimiter '|' with reuse "
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="'|'")

@prepare_before_test(num=2)
def test_02_gpload_formatOpts_delimiter():
    "2  gpload formatOpts delimiter '\t' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',delimiter="'\\t'")

@prepare_before_test(num=3)
def test_03_gpload_formatOpts_delimiter():
    "3  gpload formatOpts delimiter E'\t' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="E'\\t'")

@prepare_before_test(num=4)
def test_04_gpload_formatOpts_delimiter():
    "4  gpload formatOpts delimiter E'\u0009' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter=r"E'\u0009'")

@prepare_before_test(num=5)
def test_05_gpload_formatOpts_delimiter():
    "5  gpload formatOpts delimiter E'\\'' with reuse"
    copy_data('external_file_03.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="E'\''")

@prepare_before_test(num=6)
def test_06_gpload_formatOpts_delimiter():
    "6  gpload formatOpts delimiter \"'\" with reuse"
    copy_data('external_file_03.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="\"'\"")

@prepare_before_test(num=7)
def test_07_gpload_reuse_table_insert_mode_without_reuse():
    "7  gpload insert mode without reuse"
    runfile(mkpath('setup.sql'))
    f = open(mkpath('query7.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()
    write_config_file(mode='insert',reuse_tables=False)

@prepare_before_test(num=8)
def test_08_gpload_reuse_table_update_mode_with_reuse():
    "8  gpload update mode with reuse"
    drop_tables()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,file='data_file.txt')

@prepare_before_test(num=9)
def test_09_gpload_reuse_table_update_mode_without_reuse():
    "9  gpload update mode without reuse"
    f = open(mkpath('query9.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'\n"+"\\! psql -d reuse_gptest -c 'select * from texttable where n2=222;'")
    f.close()
    copy_data('external_file_05.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=False,file='data_file.txt')

@prepare_before_test(num=10)
def test_10_gpload_reuse_table_merge_mode_with_reuse():
    "10  gpload merge mode with reuse "
    drop_tables()
    copy_data('external_file_06.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=11)
def test_11_gpload_reuse_table_merge_mode_without_reuse():
    "11  gpload merge mode without reuse "
    copy_data('external_file_07.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=False, file='data_file.txt')

@prepare_before_test(num=12)
def test_12_gpload_reuse_table_merge_mode_with_different_columns_number_in_file():
    "12 gpload merge mode with reuse (RERUN with different columns number in file) "
    psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=13)
def test_13_gpload_reuse_table_merge_mode_with_different_columns_number_in_DB():
    "13  gpload merge mode with reuse (RERUN with different columns number in DB table) "
    preTest = mkpath('pre_test_13.sql')
    psql_run(preTest, dbname='reuse_gptest')
    copy_data('external_file_09.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=14)
def test_14_gpload_reuse_table_update_mode_with_reuse_RERUN():
    "14 gpload update mode with reuse (RERUN) "
    write_config_file(mode='update', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=15)
def test_15_gpload_reuse_table_merge_mode_with_different_columns_order():
    "15 gpload merge mode with different columns' order "
    copy_data('external_file_10.txt','data/data_file.tbl')
    input_columns = {'s_s1':'text',
        's_s2':'text',
        's_dt':'timestamp',
        's_s3':'text',
        's_n1':'smallint',
        's_n2':'integer',
        's_n3':'bigint',
        's_n4':'decimal',
        's_n5':'numeric',
        's_n6':'real',
        's_n7':'double precision',
        's_n8':'text',
        's_n9':'text'}
    output_mapping =  {"s1": "s_s1", "s2": "s_s2",  "dt": "s_dt", "s3": "s_s3", "n1": "s_n1",
    "n2": "s_n2", "n3": "s_n3", "n4": "s_n4", "n5": "s_n5", "n6": "s_n6", "n7": "s_n7", "n8": "s_n8", "n9": "s_n9"}
    write_config_file(mode='merge', reuse_tables=True, file='data/data_file.tbl',columns=input_columns,mapping=output_mapping)

@prepare_before_test(num=16)
def test_16_gpload_formatOpts_quote():
    "16  gpload formatOpts quote unspecified in CSV with reuse "
    copy_data('external_file_11.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable',delimiter="','")

@prepare_before_test(num=17)
def test_17_gpload_formatOpts_quote():
    "17  gpload formatOpts quote '\\x26'(&) with reuse"
    copy_data('external_file_12.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", quote=r"'\x26'")

@prepare_before_test(num=18)
def test_18_gpload_formatOpts_quote():
    "18  gpload formatOpts quote E'\\x26'(&) with reuse"
    copy_data('external_file_12.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", quote="E'\x26'")

@prepare_before_test(num=19)
def test_19_gpload_formatOpts_escape():
    "19  gpload formatOpts escape '\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="'\\'")

@prepare_before_test(num=20)
def test_20_gpload_formatOpts_escape():
    "20  gpload formatOpts escape '\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="'\x5C'")

@prepare_before_test(num=21)
def test_21_gpload_formatOpts_escape():
    "21  gpload formatOpts escape E'\\\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="E'\\\\'")

# case 22 is flaky on concourse. It may report: Fatal Python error: GC object already tracked during testing.
# This is seldom issue. we can't reproduce it locally, so we disable it, in order to not blocking others
#def test_22_gpload_error_count(self):
#    "22  gpload error count"
#    f = open(mkpath('query22.sql'),'a')
#    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
#    f.close()
#    f = open(mkpath('data/large_file.csv'),'w')
#    for i in range(0, 10000):
#        if i % 2 == 0:
#            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
#        else:
#            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
#    f.close()
#    copy_data('large_file.csv','data_file.csv')
#    write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",log_errors=True,error_limit='90000000')
#   self.doTest(22)

@prepare_before_test(num=23)
def test_23_gpload_error_count():
    "23  gpload error_table"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query23.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    f = open(mkpath('data/large_file.csv'),'w')
    for i in range(0, 10000):
        if i % 2 == 0:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
        else:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
    f.close()
    copy_data('large_file.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='csvtable', delimiter="','", error_table="err_table", error_limit=90000000)

@prepare_before_test(num=24)
def test_24_gpload_error_count():
    "24  gpload error count with ext schema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query24.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    f = open(mkpath('data/large_file.csv'),'w')
    for i in range(0, 10000):
        if i % 2 == 0:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
        else:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
    f.close()
    copy_data('large_file.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='csvtable', delimiter="','", log_errors=True, error_limit=90000000, externalSchema='test')

@prepare_before_test(num=25)
def test_25_gpload_ext_staging_table():
    "25  gpload reuse ext_staging_table if it is configured"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query25.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", log_errors=True,error_limit=10,staging_table='staging_table')

@prepare_before_test(num=26)
def test_26_gpload_ext_staging_table_with_externalschema():
    "26  gpload reuse ext_staging_table if it is configured with externalschema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query26.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','",log_errors=True,error_limit=10, staging_table='staging_table',externalSchema='test')

@prepare_before_test(num=27)
def test_27_gpload_ext_staging_table_with_externalschema():
    "27  gpload reuse ext_staging_table if it is configured with externalschema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query27.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='test.csvtable',delimiter="','",log_errors=True,error_limit=10,staging_table='staging_table',externalSchema="'%'")

@prepare_before_test(num=28)
def test_28_gpload_ext_staging_table_with_dot():
    "28  gpload reuse ext_staging_table if it is configured with dot"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query28.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, file='data_file.csv',table='test.csvtable',format='csv',delimiter="','",log_errors=True,error_limit=10,staging_table='t.staging_table')

@prepare_before_test(num=29)
def test_29_gpload_reuse_table_insert_mode_with_reuse_and_null():
    "29  gpload insert mode with reuse and null"
    runfile(mkpath('setup.sql'))
    f = open(mkpath('query29.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable where n2 is null;'")
    f.close()
    copy_data('external_file_14.txt','data_file.txt')
    write_config_file(mode='insert', reuse_tables=True, file='data_file.txt',log_errors=True, error_limit=100)

@prepare_before_test(num=30)
def test_30_gpload_reuse_table_update_mode_with_fast_match():
    "30  gpload update mode with fast match"
    drop_tables()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=31)
def test_31_gpload_reuse_table_update_mode_with_fast_match_and_different_columns_number():
    "31 gpload update mode with fast match and differenct columns number) "
    psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=32)
def test_32_gpload_update_mode_without_reuse_table_with_fast_match():
    "32  gpload update mode when reuse table is false and fast match is true"
    drop_tables()
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=False,fast_match=True,file='data_file.txt')

@prepare_before_test(num=33)
def test_33_gpload_reuse_table_merge_mode_with_fast_match_and_external_schema():
    "33  gpload update mode with fast match and external schema"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt',externalSchema='test')

@prepare_before_test(num=34)
def test_34_gpload_reuse_table_merge_mode_with_fast_match_and_encoding():
    "34  gpload merge mode with fast match and encoding GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt',encoding='GBK')

@prepare_before_test(num=35)
def test_35_gpload_reuse_table_merge_mode_with_fast_match_default_encoding():
    "35  gpload does not reuse table when encoding is setted from GBK to empty"
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=36)
def test_36_gpload_reuse_table_merge_mode_default_encoding():
    "36  gpload merge mode with encoding GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',encoding='GBK')

@prepare_before_test(num=37)
def test_37_gpload_reuse_table_merge_mode_invalid_encoding():
    "37  gpload merge mode with invalid encoding"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',encoding='xxxx')

@prepare_before_test(num=38)
def test_38_gpload_without_preload():
    "38  gpload insert mode without preload"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',error_table="err_table",error_limit=1000,preload=False)

@prepare_before_test(num=39)
def test_39_gpload_fill_missing_fields():
    "39  gpload fill missing fields"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=False,fast_match=False,file='data_file.txt',table='texttable1', error_limit=1000, fill_missing_fields=True)

@prepare_before_test(num=40)
def test_40_gpload_merge_mode_with_multi_pk():
    "40  gpload merge mode with multiple pk"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_pk.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',table='testpk')
    #write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file.txt',table='testpk')
    copy_data('external_file_pk2.txt','data_file2.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file2.txt',table='testpk',config='config/config_file2')
    # write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file2.txt',table='testpk',config='config/config_file2')
    f = open(mkpath('query40.sql'),'w')
    f.write("""\\! psql -d reuse_gptest -c "create table testpk (n1 integer, s1 integer, s2 varchar(128), n2 integer, primary key(n1,s1,s2))\
            partition by range (s1)\
            subpartition by list(s2)\
            SUBPARTITION TEMPLATE\
            ( SUBPARTITION usa VALUES ('usa'),\
                    SUBPARTITION asia VALUES ('asia'),\
                    SUBPARTITION europe VALUES ('europe'),\
                    DEFAULT SUBPARTITION other_regions)\
            (start (1) end (13) every (1),\
            default partition others)\
            ;"\n""")
    f.write("\\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.write("\\! psql -d reuse_gptest -c 'drop table testpk;'\n")
    f.close()

@prepare_before_test(num=41)
def test_41_gpload_special_char():
    "41 gpload special char"
    copy_data('external_file_15.txt','data_file.txt')
    columns = {"'\"Field1\"'": 'bigint',"'\"Field#2\"'": 'text'}
    write_config_file(mode='insert',reuse_tables=True,fast_match=False, file='data_file.txt',table='testSpecialChar', columns=columns,delimiter="';'")
    copy_data('external_file_16.txt','data_file2.txt')
    update_columns=["'\"Field#2\"'"]
    match_columns = ["'\"Field1\"'", "'\"Field#2\"'"]
    write_config_file(update_columns=update_columns ,config='config/config_file2', mode='merge',reuse_tables=True,fast_match=False, file='data_file2.txt',table='testSpecialChar',columns=columns, delimiter="';'",match_columns=match_columns)
    f = open(mkpath('query41.sql'),'a')
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.close()

@prepare_before_test(num=42)
def test_42_gpload_update_condition():
    "42 gpload update condition"
    copy_data('external_file_01.txt','data_file.txt')
    copy_data('external_file_03.txt','data_file2.txt')
    f = open(mkpath('query42.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.close()
    write_config_file(mode='insert', format='text',file='data_file.txt',table='texttable')
    update_columns = ['s2']
    match_columns = ['n1']
    write_config_file(update_columns=update_columns,match_columns=match_columns,config='config/config_file2',mode='update', update_condition="s3='shpits'", format='text',file='data_file2.txt',table='texttable',delimiter="\"'\"")

@prepare_before_test(num=43)
def test_43_gpload_column_without_data_type():
    "43 gpload column name has capital letters and without data type"
    copy_data('external_file_15.txt','data_file.txt')
    columns = {"'\"Field1\"'": '',"'\"Field#2\"'": ''}
    write_config_file(mode='insert',reuse_tables=True,fast_match=False, file='data_file.txt',table='testSpecialChar',columns=columns, delimiter="';'")
