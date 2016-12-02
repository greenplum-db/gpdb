import os
import subprocess
import sys

class GpdbBuildBase:
    def __init__(self):
        pass

    def num_cpus(self):
        # Use multiprocessing module, available in Python 2.6+
        try:
            import multiprocessing
            return multiprocessing.cpu_count()
        except (ImportError, NotImplementedError):
            pass

        # Get POSIX system config value for number of processors.
        posix_num_cpus = os.sysconf("SC_NPROCESSORS_ONLN")
        if posix_num_cpus != -1:
            return posix_num_cpus

        # Guess
        return 2
	
    def install_dependency(self, dependency_name):
        return subprocess.call("tar -xzf " + dependency_name + "/*.tar.gz -C /usr/local", shell=True)
    
    def configure(self):
        return subprocess.call(["./configure",
                                "--enable-mapreduce",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpfdist",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")
    
