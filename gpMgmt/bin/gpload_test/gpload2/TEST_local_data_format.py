from TEST_local_base import *

@prepare_before_test(num=201)
def test_201_gpload_delimiter_ascii_08():
    "201 gpload formatOpts delimiter ascii 08 (Backspace) with reuse"
    copy_data('external_file_201.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter=r"E'\x08'")

@prepare_before_test(num=202)
def test_202_gpload_delimiter_ascii_127():
    "202 gpload formatOpts delimiter ascii 127 (DEL) with reuse"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_201.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter=r"E'\x7F'")

@prepare_before_test(num=203, times=1)
def test_203_gpload_delimiter_2_characters():
    "203 gpload fails if formatOpts delimiter has 2 characters"
    copy_data('external_file_201.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter="'7F'")

@prepare_before_test(num=204, times=1)
def test_204_gpload_delimiter_not_string():
    "204 gpload fails if formatOpts delimiter is not a string"
    copy_data('external_file_201.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter='8')

@prepare_before_test(num=205, times=1)
def test_205_gpload_delimiter_out_range_of_ascii():
    "205 gpload fails if formatOpts delimiter is out rang of ascii 127 with reuse"
    copy_data('external_file_201.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter=r"E'\x9F'")

@prepare_before_test(num=206)
def test_206_gpload_delimiter_ascii_27():
    "206 gpload formatOpts delimiter ascii 27 (Escape) with reuse"
    copy_data('external_file_201.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter=r"E'\x1B'")

@prepare_before_test(num=207, times=1)
def test_207_gpload_delimiter_with_ascii_13_in_line():
    "207 gpload fails if there is ascii 13 (CR) in line"  # ascii 0(null), 10(\n), 13(\r) are invalid delimiters
    copy_data('external_file_207.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2',delimiter=r"E'\x1B'")

@prepare_before_test(num=208)
def test_208_gpload_delimiter_ascii_08_csv():
    "208 gpload csv formatOpts delimiter ascii 08 (Backspace) with reuse"
    copy_data('external_file_208.csv','data_file.csv')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable2',delimiter=r"E'\x08'")

@prepare_before_test(num=209)
def test_209_gpload_delimiter_ascii_127_csv():
    "209 gpload csv formatOpts delimiter ascii 127 (DEL) with reuse"
    copy_data('external_file_209.csv','data_file.csv')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable2',delimiter=r"E'\x7F'")

@prepare_before_test(num=210)
def test_210_gpload_formatOpts_escape_backslash():
    "210 gpload formatOpts escape '\\' with reuse"
    copy_data('external_file_210.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable2',escape="'\\'")

@prepare_before_test(num=211, times=1)
def test_211_gpload_formatOpts_escape_delimiter():
    "211 gpload fails if formatOpts escape '|' same as delimiter"
    copy_data('external_file_211.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable2',escape="'|'")

@prepare_before_test(num=212)
def test_212_gpload_formatOpts_escape_off():
    "212 gpload formatOpts escape off with reuse"
    copy_data('external_file_212.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable2',escape="'off'")

@prepare_before_test(num=213, times=1)
def test_213_gpload_formatOpts_escape_2_characters():
    "213 gpload fails if formatOpts escape has 2 characters: 'aa' with reuse"
    copy_data('external_file_210.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable2',escape="'aa'")

@prepare_before_test(num=214)
def test_214_gpload_formatOpts_escape_o():
    "214 gpload formatOpts escape 'o' "
    copy_data('external_file_214.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=False, format='text', file='data_file.txt',table='texttable2',escape="'o'")

@prepare_before_test(num=215)
def test_215_gpload_formatOpts_escape_default():
    "215 gpload formatOpts escape default is backslash"
    copy_data('external_file_210.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable2')

@prepare_before_test(num=216)
def test_216_gpload_formatOpts_escape_default_csv():
    "216 gpload csv formatOpts escape default is double quote"
    copy_data('external_file_216.csv','data_file.csv')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='texttable2')

@prepare_before_test(num=217, times=1)
def test_217_gpload_formatOpts_escape_off_csv():
    "217 gpload fails if csv formatOpts escape is off with"
    copy_data('external_file_212.txt','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='texttable2',escape="'off'")

'''
escape only is available in quoted part for csv format.
'''
@prepare_before_test(num=218)
def test_218_gpload_formatOpts_escape_c_csv():
    "218 gpload csv formatOpts escape 'c' "
    copy_data('external_file_218.csv','data_file.csv')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=False, format='csv', file='data_file.csv',table='texttable2',escape="'c'")

@prepare_before_test(num=219)
def test_219_gpload_formatOpts_escape_o_csv():
    "219 gpload csv formatOpts escape 'o'(ascii 111) "
    copy_data('external_file_219.csv','data_file.csv')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=False, format='csv', file='data_file.csv',table='texttable2',escape="'o'")

@prepare_before_test(num=220, times=1)
def test_220_gpload_fill_missing_fields_disabled():
    "220 gpload fails if fill missing fields is false and there are missing fields"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_220.txt','data_file.txt')
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable1', fill_missing_fields=False)

@prepare_before_test(num=230)
def test_230_gpload_null_as_fill_missing_fields_disabled():
    "230 gpload with default null as option and don't fill missing fields"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_230.txt','data_file.txt')
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', fill_missing_fields=False)

@prepare_before_test(num=231)
def test_231_gpload_null_as_fill_missing_fields_enabled():
    "231 gpload with default null as option and fix missing fields"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_231.txt','data_file.txt')
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable', fill_missing_fields=True)

@prepare_before_test(num=232)
def test_232_gpload_null_as_escape():
    "232 gpload with null as '\\'"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_232.txt','data_file.txt')
    f = open(mkpath('query232.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable2 WHERE s1 is NULL'")
    f.close()
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable2', escape="'\\'", null_as="'\\\\'")

@prepare_before_test(num=233, times=1)
def test_233_gpload_null_as_0x08():
    "233 gpload fails with null as 0x8(backspace)"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_233.txt','data_file.txt')
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable2', null_as=r"E'\x08'", fill_missing_fields=False)

@prepare_before_test(num=234)
def test_234_gpload_null_as_table():
    "234 gpload with null as '\t'"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_234.txt','data_file.txt')
    f = open(mkpath('query234.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable2 WHERE s1 is NULL'")
    f.close()
    write_config_file(reuse_tables=True,fast_match=False,file='data_file.txt',table='texttable2', escape="'\\'", null_as="'\t'")

@prepare_before_test(num=235)
def test_235_gpload_default_null_as_csv():
    "235 gpload csv format with default null as option"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_235.csv','data_file.csv')
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable')

@prepare_before_test(num=236)
def test_236_gpload_null_as_table_csv():
    "236 gpload csv format with null as '\t'"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_234.txt','data_file.csv')  #reuse case 234's data file
    f = open(mkpath('query236.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'SELECT COUNT(*) FROM texttable2 WHERE s1 is NULL'")
    f.close()
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable2', escape="'\\'", null_as="'\t'")

@prepare_before_test(num=240)
def test_240_gpload_default_encoding():
    "240 gpload uses default encoding as UTF8"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_240.txt','data_file.txt')
    write_config_file(reuse_tables=True,file='data_file.txt')

@prepare_before_test(num=241, times=1)
def test_241_gpload_UTF8_format_text_GBK_encoding():
    "241 gpload fails if the text file is UTF8 but encoding is GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_240.txt','data_file.txt')
    write_config_file(reuse_tables=True,file='data_file.txt', encoding='GBK')

@prepare_before_test(num=242)
def test_242_gpload_GBK_format_text_GBK_encoding():
    "242 both the text file and encoding are GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_242.txt','data_file.txt')
    write_config_file(reuse_tables=True,file='data_file.txt',table='texttable2',encoding='GBK')

@prepare_before_test(num=243)
def test_243_gpload_GBK_csv_format_GBK_encoding():
    "243 both the csv file and encoding are GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_242.txt','data_file.csv')
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable2',encoding='GBK')

@prepare_before_test(num=244,times=1)
def test_244_gpload_GBK_format_text_default_encoding():
    "244 gpload fails if the text file is GBK with default encoding"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_242.txt','data_file.txt')
    write_config_file(reuse_tables=True,file='data_file.txt',table='texttable2',)

@prepare_before_test(num=250)
def test_250_gpload_eol_CR():
    "250 gpload loads text file with CR as end of line"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_250.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2')

@prepare_before_test(num=251)
def test_251_gpload_eol_CRLF():
    "251 gpload loads text file with CRLF as end of line"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_251.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable2')

@prepare_before_test(num=252)
def test_252_gpload_eol_LF_in_quote_csv():
    "252 gpload loads csv file as 1 line if end of line in quote"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_252.csv','data_file.csv')
    write_config_file(reuse_tables=True,format='csv',file='data_file.csv',table='texttable2')