#!/usr/bin/python2

import optparse
import subprocess
import sys
import shutil
from gporca import GporcaCommon

def install_gpdb(dependency_name):
    status = subprocess.call("mkdir -p /usr/local/gpdb", shell=True)
    if status:
        return status
    status = subprocess.call(
        ["tar",
         "-xzf",
         dependency_name + "/" + dependency_name + ".tar.gz",
         "-C",
         "/usr/local/gpdb"])
    return status

def create_gpadmin_user():
    status = subprocess.call("gpdb_src/concourse/scripts/setup_gpadmin_user.bash")
    if status:
        return status

def icg():
    status = subprocess.call(
        "printf '\nLD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\nexport \
        LD_LIBRARY_PATH' >> /usr/local/gpdb/greenplum_path.sh", shell=True)
    if status:
        return status
    status = subprocess.call([
        "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
        && make cluster\""], cwd="gpdb_src/gpAux/gpdemo", shell=True)
    if status:
        return status
    return subprocess.call([
        "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
        && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer=on' \
        make installcheck-good\""], cwd="gpdb_src", shell=True)

def copy_output():
    shutil.copyfile("gpdb_src/src/test/regress/regression.diffs", "icg_output/regression.diffs")
    shutil.copyfile("gpdb_src/src/test/regress/regression.out", "icg_output/regression.out")

def main():
    parser = optparse.OptionParser()
    parser.add_option("--build_type", dest="build_type", default="RELEASE")
    parser.add_option("--compiler", dest="compiler")
    parser.add_option("--cxxflags", dest="cxxflags")
    parser.add_option("--output_dir", dest="output_dir", default="install")
    parser.add_option("--gpdb_name", dest="gpdb_name")
    (options, args) = parser.parse_args()
    ciCommon = GporcaCommon()
    status = ciCommon.install_system_deps()
    if status:
        return status
    for dependency in args:
        status = ciCommon.install_dependency(dependency)
        if status:
            return status
    status = install_gpdb(options.gpdb_name)
    if status:
        return status
    status = ciCommon.configure()
    if status:
        return status
    status = create_gpadmin_user()
    if status:
        return status
    status = icg()
    if status:
        copy_output()
    return status

if __name__ == "__main__":
    sys.exit(main())
