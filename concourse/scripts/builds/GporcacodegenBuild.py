import os
import subprocess
import sys
from GpdbBuildBase import GpdbBuildBase

class GporcacodegenBuild(GpdbBuildBase):
    def __init__(self):
        pass

    def configure(self):
        return subprocess.call(' '.join(["./configure",
                                         "--enable-codegen",
                                         "--enable-mapreduce",
                                         "--with-perl",
                                         "--with-libxml",
                                         "--with-python",
                                         "--disable-gpfdist",
                                         "--prefix=/usr/local/gpdb"]),
                                        cwd="gpdb_src", shell=True)
    def icg(self):
        status = subprocess.call(
            "printf '\nLD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\nexport \
            LD_LIBRARY_PATH' >> /usr/local/gpdb/greenplum_path.sh", shell=True)
        if status:
            return status
        status = subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && make create-demo-cluster\""], cwd="gpdb_src/gpAux/gpdemo", shell=True)
        if status:
            return status
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer=on -c codegen=on' \
            make installcheck-good\""], cwd="gpdb_src", shell=True)
    
