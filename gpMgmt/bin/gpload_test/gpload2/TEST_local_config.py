from TEST_local_base import *

@prepare_before_test(num=44, cmd="-h "+str(hostNameAddrs))
def test_44_gpload_config_h():
    "44 gpload command config test -h hostname"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file( host="",format='text',file='data_file.txt',table='texttable',delimiter="'|'")

@prepare_before_test(num=45, cmd="-p "+str(coordinatorPort))
def test_45_gpload_config_p():
    "45 gpload command config test -p port"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(port="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=46, cmd="-p 9999")
def test_46_gpload_config_wrong_p():
    "46 gpload command config test -p port"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(port="", format='text',file='data_file.txt',table='texttable')

''' this case runs extreamly slowly, comment it here
@prepare_before_test(num=47, cmd="-h 1.2.3.4")
def test_47_gpload_config_wrong_h():
    "47 gpload command config test -h hostname"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file( host="",format='text',file='data_file.txt',table='texttable',delimiter="'|'")
'''

@prepare_before_test(num=48, cmd="-d reuse_gptest")
def test_48_gpload_config_d():
    "48 gpload command config test -d database"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(database="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=49, cmd="-d notexistdb")
def test_49_gpload_config_wrong_d():
    "49 gpload command config test -d with wrong database"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(database="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=50, cmd="-U "+str(PGUSER))
def test_50_gpload_config_U():
    "50 gpload command config test -U username"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(user="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=51, cmd="-U notexistusr")
def test_51_gpload_config_wrong_U():
    "51 gpload command config test -U wrong username"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(user="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=52, cmd='--gpfdist_timeout 2')
def test_52_gpload_config_gpfdist_timeout():
    "52 gpload command config test gpfdist_timeout"
    drop_tables()
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')

'''  maybe some bug in gpfdist
@prepare_before_test(num=53, cmd='--gpfdist_timeout aa')
def test_53_gpload_config_gpfdist_timeout_wrong():
    "53 gpload command config test gpfdist_timeout with a string"
    runfile(mkpath('setup.sql'))
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')
'''

@prepare_before_test(num=54, cmd='-l 54tmp.log')
def test_54_gpload_config_l():
    "54 gpload command config test -l logfile"
    run('rm 54tmp.log')
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=55)
def test_55_gpload_yaml_version():
    "55 gpload yaml version"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(version='1.0.0.2',format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=56)
def test_56_gpload_yaml_wrong_database():
    "56 gpload yaml writing a not exist database"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(database='notexist',format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=57)
def test_57_gpload_yaml_wrong_user():
    "57 gpload yaml writing a not exist user"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(user='notexist',format='text',file='data_file.txt',table='texttable')
''' wrong host runs slowly
@prepare_before_test(num=58)
def test_58_gpload_yaml_wrong_host():
    "58 gpload yaml writing a not exist host"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(host='1.2.3.4',format='text',file='data_file.txt',table='texttable')
'''
@prepare_before_test(num=59)
def test_59_gpload_yaml_wrong_port():
    "59 gpload yaml writing a not exist port"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(port='111111',format='text',file='data_file.txt',table='texttable')

'''
@prepare_before_test(num=60)
def test_60_gpload_local_hostname():
    "60 gpload yaml local host with 127.0.0.1 and none and a not exist host"
    runfile(mkpath('setup.sql'))
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(local_host=['127.0.0.1'],format='text',file='data_file.txt',table='texttable')
    write_config_file(config='config/config_file2',local_host=None,format='text',file='data_file.txt',table='texttable')
    write_config_file(config='config/config_file3',local_host=['123.123.1.1'],format='text',file='data_file.txt',table='texttable')
    f = open('query60.sql','w')
    f.write("\\! gpload -f "+mkpath('config/config_file')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file3')+"\n")
    f.close()
'''

@prepare_before_test(num=61, times=1)
def test_61_gpload_local_no_port():
    "61 gpload yaml file port not specified"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(input_port=None,format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=62, times=1)
def test_62_gpload_wrong_port_range():
    "62 gpload yaml file use wrong port_range"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(input_port=None,port_range='[8081,8070]'  ,format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=63, times=1)
def test_63_gpload_test_port_range():
    "63 gpload yaml file use port_range"
    runfile(mkpath('setup.sql'))
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(input_port=None,port_range='[8082,8090]'  ,format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=100, times=1)
def test_100_gpload_transform():
    runfile(mkpath('setup.sql'))
    write_config_file(file='data/transform/prices.xml',
                      transform_config='data/transform/transform_config.yaml',
                      transform='prices_input',
                      format='text',
                      table='prices',
                      mode='insert')

@prepare_before_test_2(num=101)
def test_101_gpload_test_port_range():
    "101 gpload header reuse table MPP:31557"
    copy_data('external_file_101.txt','data_file.txt')
    write_config_file(input_port=None,port_range='[8082,8090]', mode='insert',reuse_tables='true',fast_match='false', file='data_file.txt',config='config/config_file1', table='testheaderreuse', delimiter="','", format='csv', quote="'\x22'", encoding='LATIN1', log_errors=True, error_limit='1000', header='true', truncate=True, match_columns=False)
    write_config_file(input_port=None,port_range='[8082,8090]', mode='insert',reuse_tables='true',fast_match='false', file='data_file.txt',config='config/config_file2', table='testheaderreuse', delimiter="','", format='csv', quote="'\x22'", encoding='LATIN1', log_errors=True, error_limit='1000', truncate=True, match_columns=False)
    f = open(mkpath('query101.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file1')+ "\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ "\n")
    f.close()
