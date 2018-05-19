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

untar_gpdb_cmd = "mkdir -p gpdb_src && tar -xf gpdb_tarball/gpdb_src.tar.gz -C gpdb_src --strip 1"
exec_command(untar_gpdb_cmd)

print "Beginning to {0}".format(action)
if action == 'build':
    build_gpdb_cmd = "gpdb_src/concourse/scripts/build_gpdb.py --mode={0} --output_dir=bin_gpdb/install --action={1} --configure-option='{2}' --orca-in-gpdb-install-location bin_orca bin_xerces".format(mode, action, configure_option)
    exec_command(build_gpdb_cmd)

    package_tarball_cmd = "env src_root=bin_gpdb/install dst_tarball=package_tarball/bin_gpdb.tar.gz gpdb_src/concourse/scripts/package_tarball.bash"
    exec_command(package_tarball_cmd)

elif action == 'test':
    environment_variables = os.environ['BLDWRAP_POSTGRES_CONF_ADDONS']

    test_gpdb_cmd = "env {0} gpdb_src/concourse/scripts/build_gpdb.py --mode={1} --gpdb_name=bin_gpdb --action={2} --configure-option='{3}'".format(environment_variables, mode, action, configure_option) 
    exec_command(test_gpdb_cmd)

