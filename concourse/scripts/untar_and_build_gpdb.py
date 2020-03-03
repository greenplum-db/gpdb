#!/usr/bin/env python
import subprocess
import sys
import os

action = os.environ['ACTION']
mode = os.environ['MODE']
configure_option=os.environ['CONFIGURE_OPTION']

def exec_command(cmd):
  print "Executing command: {0}".format(cmd)
  retcode = subprocess.call(cmd, shell=True)
  if retcode != 0:
    sys.exit(retcode)

def get_assert_mode():
    with open('gporca-commits-to-test/gpdb_assert_mode.txt') as fp:
        assert_mode = fp.read().strip()
        return assert_mode 

untar_gpdb_cmd = "mkdir -p gpdb_src && tar -xf gpdb_tarball/gpdb*_src.tar.gz -C gpdb_src --strip 1"
exec_command(untar_gpdb_cmd)

# Default build mode is to enable assert. If the user explicitly specifies
# 'disable-assert' option, only then don't enable the assert build.
assert_mode = get_assert_mode()
if assert_mode == 'enable-cassert':
    configure_option += ' --enable-cassert'
elif assert_mode != 'disable-cassert':
    raise Exception('Unknown build type: (%s). Possible options are \'enable-cassert\' or \'disable-cassert\'')

print "Beginning to {0}".format(action)
if action == 'build':
    build_gpdb_cmd = "gpdb_main_src/concourse/scripts/build_gpdb.py --mode={0} --output_dir=gpdb_binary/install --action={1} --configure-option='{2}' --orca-in-gpdb-install-location bin_orca bin_xerces".format(mode, action, configure_option)
    exec_command(build_gpdb_cmd)

    package_tarball_cmd = "env src_root=gpdb_binary/install dst_tarball=package_tarball/bin_gpdb.tar.gz gpdb_main_src/concourse/scripts/package_tarball.bash"
    exec_command(package_tarball_cmd)

elif action.startswith('test'):
    environment_variables = os.environ['BLDWRAP_POSTGRES_CONF_ADDONS']

    test_gpdb_cmd = "env {0} gpdb_main_src/concourse/scripts/build_gpdb.py --mode={1} --gpdb_name=bin_gpdb --action={2} --configure-option='{3}'".format(environment_variables, mode, action, configure_option) 
    exec_command(test_gpdb_cmd)

