# coding=utf-8
from TEST_local_base import write_config_file, psql_run, mkpath
from TEST_local_base import prepare_before_test, drop_tables, runfile
from TEST_local_base import runfile, copy_data, run, hostNameAddrs, masterPort
import pytest

@pytest.mark.order(301)
@prepare_before_test(num=301, times=1)
def test_301_gpload_yaml_with_header():
    "301 gpload yaml config with header true"
    copy_data('external_file_301.txt','data_file.txt')
    write_config_file(config='config/config_file',format='text',file='data_file.txt',table='texttable', header='true')

@pytest.mark.order(310)
@prepare_before_test(num=310, times=1)
def test_310_gpload_yaml_with_error_limit_0():
    "310 gpload yaml config with error limit, useless value 0"
    copy_data('external_file_301.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable', error_limit=0, header='true')

@pytest.mark.order(311)
@prepare_before_test(num=311, times=1)
def test_311_gpload_yaml_with_error_limit_1():
    "311 gpload yaml config with error limit, invalid value 1"
    copy_data('external_file_301.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable', error_limit=1, header='true')

@pytest.mark.order(312)
@prepare_before_test(num=312, times=1)
def test_312_gpload_yaml_with_error_limit_2_not_reached():
    "312 gpload yaml config with error limit, reached"
    copy_data('external_file_312.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable', error_limit=2) 

@pytest.mark.order(313)
@prepare_before_test(num=313, times=1)
def test_313_gpload_yaml_with_error_limit_2_reached():
    "313 gpload yaml config with error limit, not reached"
    copy_data('external_file_312.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable', error_limit=3) 

@pytest.mark.order(314)
@prepare_before_test(num=314, times=1)
def test_314_gpload_yaml_with_log_errors():
    "314 gpload yaml config with log errors"
    copy_data('external_file_312.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable', error_limit=3, log_errors=True) 

@pytest.mark.order(351)
@prepare_before_test(num=351, times=1)
def test_351_gpload_yaml_force_not_null_is_true():
    "351 gpload yaml config force not null is true"
    copy_data('external_file_351.csv','data_file.csv')
    write_config_file(format='csv',file='data_file.csv',table='csvtable', force_not_null=['make'], delimiter="','") 

@pytest.mark.order(352)
@prepare_before_test(num=352, times=1)
def test_352_gpload_yaml_force_not_null_is_false():
    "352 gpload yaml config force not null is false"
    copy_data('external_file_351.csv','data_file.csv')
    write_config_file(format='csv',file='data_file.csv',table='csvtable', delimiter="','") 
