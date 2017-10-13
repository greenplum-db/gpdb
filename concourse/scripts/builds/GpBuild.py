import os
import subprocess
import sys
from GpdbBuildBase import GpdbBuildBase

class GpBuild(GpdbBuildBase):
    def __init__(self, mode):
        self.mode = 'on' if mode == 'orca' else 'off'

    def configure(self):
        return subprocess.call(["./configure",
                                "--enable-mapreduce",
                                "--with-gssapi",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpcloud",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")

    def icg(self):
        status = subprocess.call(
            "printf '\nLD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\nexport \
            LD_LIBRARY_PATH' >> /usr/local/gpdb/greenplum_path.sh", shell=True)
        if status:
            return status
        status = subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && make create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\""], cwd="gpdb_src/gpAux/gpdemo", shell=True)
        if status:
            return status
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={0}' \
            make installcheck-world\"".format(self.mode)], cwd="gpdb_src", shell=True)
