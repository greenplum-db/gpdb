from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.gparray import GpArray
from test.behave_utils.utils import run_gpcommand, getRows
from time import gmtime, strftime
import subprocess
from socket import gethostname

@given('the kerberos is setup on the cluster')
@when('the kerberos is setup on the cluster')
@then('the kerberos is setup on the cluster')
def impl(context):
    setup_path = os.path.join(os.getcwd(), '../../cdb-pg/src/test/regress/kerberos')
    setup_file = '%s/setup_test.sh' % setup_path
       
    subprocess.call([setup_file])

@given('a connection to database is succesfully established as a kerberos user')
@when('a connection to database is succesfully established as a kerberos user')
@then('a connection to database is succesfully established as a kerberos user')
def impl(context):
    host = gethostname()
    psql_cmd = 'psql -U "gpadmin/kerberos-test" -h %s template1 -c """select 1;"""' % host
    cmd = Command(name='psql connection with kerberos user',
           cmdStr=psql_cmd)
    cmd.run(validateAfter=True)
    results = cmd.get_results()

@given('the valid until timestamp is expired for the kerberos user')
@when('the valid until timestamp is expired for the kerberos user')
@then('the valid until timestamp is expired for the kerberos user')
def impl(context):
    sql_cmd = """Alter role "gpadmin/kerberos-test" valid until '2014-04-10 11:46:00-07'"""
    execute_sql('template1', sql_cmd)

@given('a kerberos user connection to the database will print {msg} to stdout')
@when('a kerberos user connection to the database will print {msg} to stdout')
@then('a kerberos user connection to the database will print {msg} to stdout')
def impl(context, msg):
    host = gethostname()
    psql_cmd = 'psql -U "gpadmin/kerberos-test" -h %s template1 -c """select 1;"""' % host
    cmd = Command(name='psql connection with kerberos user',
           cmdStr=psql_cmd)
    cmd.run()
    results = cmd.get_results()
    if not msg in results.stderr.strip():
	raise Exception('Output not as expected!') 

@given('the valid until timestamp is not expired for the kerberos user')
@when('the valid until timestamp is not expired for the kerberos user')
@then('the valid until timestamp is not expired for the kerberos user')
def impl(context):
    time_gmt = strftime("%Y-%m-%d %H:%M:%S", gmtime())	
    sql_cmd = """Alter role "gpadmin/kerberos-test" valid until '%s'""" % time_gmt
    execute_sql('template1', sql_cmd)

