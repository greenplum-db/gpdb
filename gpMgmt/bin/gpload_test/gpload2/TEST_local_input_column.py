from TEST_local_base import *
# coding=utf-8

@prepare_before_test(num=64, times=1)
def test_64_gpload_multi_input_file():
    "64 gpload with multiple input file"
    copy_data('external_file_01.txt','data_file.txt')
    copy_data('external_file_01.txt','data_file1.txt')
    write_config_file(format='text',file=['data_file.txt','data_file1.txt'],table='texttable')

@prepare_before_test(num=65, times=1)
def test_65_gpload_multi_input_file_with_a_notexist():
    "65 gpload with multiple input files including a not exist one"
    copy_data('external_file_01.txt','data_file.txt')
    copy_data('external_file_01.txt','data_file1.txt')
    write_config_file(format='text',file=['data_file.txt','data_file1.txt','data_file99.txt'],table='texttable')

@prepare_before_test(num=66, times=1)
def test_66_gpload_multi_input_file_gz_bz2():
    "66 gpload with multiple input files including bz2 and gz fiile"
    copy_data('external_file_01.txt','data_file.txt')
    copy_data('external_file_01.txt','data_file1.txt')
    run('gzip data_file.txt')
    run('bzip2 data_file1.txt')
    write_config_file(format='text',file=['data_file.txt.gz','data_file1.txt.bz2'],table='texttable')

@prepare_before_test(num=67, times=1)
def test_67_gpload_wrong_column_name():
    "67 gpload merge mode with wrong column name"
    copy_data('external_file_16.txt','data_file.txt')
    update_columns=["'\"Field#2\"'"]
    columns = {"'\"Field1\"'": 'bigint',"'\"Field#2\"'": 'text'}
    match_columns = ["'\"Field1\"'", "'\"Field###2\"'"]
    write_config_file(update_columns=update_columns, mode='merge',reuse_tables=True,fast_match=False, file='data_file.txt',table='testSpecialChar',columns=columns, delimiter="';'",match_columns=match_columns)

@prepare_before_test(num=68, times=1)
def test_68_gpload_wrong_column_data_type_without_fastmatch():
    "68 gpload merge mode with wrong column data type without fastmatch"
    copy_data('external_file_16.txt','data_file.txt')
    update_columns=["'\"Field#2\"'"]
    columns = {"'\"Field1\"'": 'text',"'\"Field#2\"'": 'text'}
    match_columns = ["'\"Field1\"'", "'\"Field#2\"'"]
    write_config_file(update_columns=update_columns, mode='merge',reuse_tables=True,fast_match=False, file='data_file2.txt',table='testSpecialChar',columns=columns, delimiter="';'",match_columns=match_columns)

@prepare_before_test(num=69, times=1)
def test_69_gpload_wrong_column_data_type_with_fastmatch():
    "69 gpload merge mode with wrong column data type with fastmatch"
    copy_data('external_file_16.txt','data_file.txt')
    update_columns=["'\"Field#2\"'"]
    columns = {"'\"Field1\"'": 'text',"'\"Field#2\"'": 'text'}
    match_columns = ["'\"Field1\"'", "'\"Field#2\"'"]
    write_config_file(update_columns=update_columns, mode='merge',reuse_tables=True,fast_match=True, file='data_file2.txt',table='testSpecialChar',columns=columns, delimiter="';'",match_columns=match_columns)

@prepare_before_test(num=70, times=1)
def test_70_gpload_chinese_column_name():
    "70 gpload insert data when column name has chinese"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data("external_file_17.txt", "data_file.txt")
    columns = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp',  "'\"列four\"'": 'decimal'}
    write_config_file( mode='insert',file='data_file.txt',reuse_tables=True,fast_match=True, table='chinese表',columns=columns, delimiter="';'",encoding='utf-8')

@prepare_before_test(num=71, times=2)
def test_71_gpload_chinese_column_name_merge_mode():
    "71 gpload merge data when column name has chinese"
    copy_data("external_file_18.txt", "data_file.txt")
    columns = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp',  "'\"列four\"'": 'decimal'}
    match_columns=["'\"列#2\"'"]
    update_columns=["'\"列1\"'", "'\"lie3\"'", "'\"列four\"'"]
    write_config_file( mode='merge',file='data_file.txt',reuse_tables=True,fast_match=True, table='chinese表',columns=columns, match_columns=match_columns, update_columns=update_columns, delimiter="';'",encoding='utf-8')
    f = open(mkpath('query71.sql'),'a')
    f.write("""\\! psql -d reuse_gptest -c 'select * from chinese表 order by "列#2";'""")
    f.close()

@prepare_before_test(num=72, times=1)
def test_72_gpload_chinese_column_name_without_datatype():
    "72 gpload insert data when column name has chinese and input columns without datatype"
    copy_data("external_file_18.txt", "data_file.txt")
    columns = {"'\"列1\"'":'',"'\"列#2\"'": '', "'\"lie3\"'": '',  "'\"列four\"'": ''}
    match_columns=["'\"列#2\"'"]
    update_columns=["'\"列1\"'", "'\"lie3\"'", "'\"列four\"'"]
    write_config_file( mode='insert',file='data_file.txt',reuse_tables=True,fast_match=True, table='chinese表',columns=columns, match_columns=match_columns, update_columns=update_columns, delimiter="';'",encoding='utf-8')

@prepare_before_test(num=73, times=2)
def test_73_gpload_chinese_column_name_with_a_notexist_col():
    "73 gpload merge data when column name has chinese and a extral notexist column"
    copy_data("external_file_18.txt", "data_file.txt")
    columns = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp',  "'\"列four\"'": 'decimal','notexist':'int'}
    match_columns=["'\"列#2\"'"]
    update_columns=["'\"列1\"'", "'\"lie3\"'", "'\"列four\"'"]
    write_config_file( mode='merge',file='data_file.txt',reuse_tables=True,fast_match=True, table='chinese表',columns=columns, match_columns=match_columns, update_columns=update_columns, delimiter="';'",encoding='utf-8')

@prepare_before_test(num=74)
def test_74_gpload_insert_partial_columns():
    "74 gpload insert partial columns"
    copy_data("external_file_18.txt", "data_file.txt")
    columns = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp'}
    write_config_file( mode='insert',file='data_file.txt',reuse_tables=True, fast_match=True, table='chinese表',columns=columns, delimiter="';'")
    f = open(mkpath('query74.sql'),'a')
    f.write("""\\! psql -d reuse_gptest -c 'select count(*) from chinese表 where "列four" is NULL;'""")
    f.close()

@prepare_before_test(num=75)
def test_75_gpload_merge_chinese_standard_conforming_str_on():
    """75 gpload merge data with different quotations '"col"'  "\"col\""  "'col'" and "col" 
    with standard_conforming_strings on"""
    copy_data("external_file_19.txt", "data_file.txt")
    runfile(mkpath('setup.sql'))
    # '"col"'
    columns1 = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp'}
    # "\"col\""
    columns2 = {'"\\"列1\\""': 'text','"\\"列#2\\""': 'int', '"\\"lie3\\""': 'timestamp'}
    # "'col'"
    columns3 = {'"\'列1\'"': 'text','"\'列#2\'"': 'int', '"\'lie3\'"': 'timestamp'}
    # "col"
    columns4 = {'"列1"': 'text','"列#2"': 'int', '"lie3"': 'timestamp'}
    update_columns = {'"列1"': 'text'}
    match_columns = {'"列#2"': 'int'}
    write_config_file(mode='insert',file='data_file.txt',config='config/config_file1',reuse_tables=True, fast_match=True, table='chinese表',columns=columns1, delimiter="';'",sql=True,before='"set standard_conforming_strings =on;"')
    write_config_file(mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file2',reuse_tables=True, fast_match=True, table='chinese表',columns=columns2, delimiter="';'",sql=True,before='"set standard_conforming_strings =on;"')
    write_config_file(mode='insert',file='data_file.txt',config='config/config_file3',reuse_tables=True, fast_match=True, table='chinese表',columns=columns3, delimiter="';'",sql=True,before='"set standard_conforming_strings =on;"')
    write_config_file(mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file4',reuse_tables=True, fast_match=True, table='chinese表',columns=columns4, delimiter="';'",sql=True,before='"set standard_conforming_strings =on;"')
    f = open(mkpath('query75.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file1')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file3')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file4')+"\n")
    f.close()


@prepare_before_test(num=76)
def test_76_gpload_merge_chinese_standard_conforming_str_off():
    """76 gpload merge data with different quotations '"col"'  "\"col\""  "'col'" and "col" 
    with standard_conforming_strings off"""
    copy_data("external_file_18.txt", "data_file.txt")
    # '"col"'
    columns1 = {"'\"列1\"'": 'text',"'\"列#2\"'": 'int', "'\"lie3\"'": 'timestamp'}
    # "\"col\""
    columns2 = {'"\\"列1\""': 'text','"\\"列#2\""': 'int', '"\\"lie3\""': 'timestamp'}
    # "'col'"
    columns3 = {'"\'列1\'"': 'text','"\'列#2\'"': 'int', '"\'lie3\'"': 'timestamp'}
    # "col"
    columns4 = {'"列1"': 'text','"列#2"': 'int', '"lie3"': 'timestamp'}
    update_columns = {'"列1"': 'text'}
    match_columns = {'"列#2"': 'int'}
    write_config_file( mode='insert',file='data_file.txt',config='config/config_file1',reuse_tables=True, fast_match=True, table='chinese表',columns=columns1, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file2',reuse_tables=True, fast_match=True, table='chinese表',columns=columns2, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='insert',file='data_file.txt',config='config/config_file3',reuse_tables=True, fast_match=True, table='chinese表',columns=columns3, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file4',reuse_tables=True, fast_match=True, table='chinese表',columns=columns4, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    f = open(mkpath('query76.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file1')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file3')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file4')+"\n")
    f.close()


@prepare_before_test(num=77)
def test_77_gpload_merge_capital_letters_standard_conforming_str_off():
    """77 gpload merge data capital letters with different quotations '"col"'  "\"col\""  "'col'" and "col" 
    and with standard_conforming_string off"""
    copy_data('external_file_16.txt','data_file.txt')
    update_columns=["'\"Field#2\"'"]
    match_columns = ["'\"Field1\"'", "'\"Field#2\"'"]

    # '"col"'
    columns1 = {"'\"Field1\"'": 'text',"'\"Field#2\"'": 'text'}
    # "\"col\""
    columns2 = {'"\\"Field1\\""': 'text','"\\"Field#2\\""': 'text'}
    # "'col'"
    columns3 = {'"\'Field1\'"': 'text','"\'Field#2\'"': 'text'}
    # "col"
    columns4 = {'"Field1"': 'text','"Field#2"': 'text'}

    match_columns = {"'\"Field1\"'": 'text'}
    update_columns = {"'\"Field#2\"'": 'text'}
    write_config_file( mode='insert',file='data_file.txt',config='config/config_file1',reuse_tables=True, fast_match=True, table='testSpecialChar',columns=columns1, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file2',reuse_tables=True, fast_match=True, table='testSpecialChar',columns=columns2, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file3',reuse_tables=True, fast_match=True, table='testSpecialChar',columns=columns3, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    write_config_file( mode='merge',update_columns=update_columns,match_columns=match_columns,file='data_file.txt',config='config/config_file4',reuse_tables=True, fast_match=True, table='testSpecialChar',columns=columns4, delimiter="';'",sql=True,before='"set standard_conforming_strings =off;"')
    f = open(mkpath('query77.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file1')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file3')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file4')+"\n")
    f.close()
