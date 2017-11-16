import subprocess
from GpdbBuildBase import GpdbBuildBase

INSTALL_DIR="/usr/local/gpdb"


class GpBuild(GpdbBuildBase):
    def __init__(self, mode="orca"):
        GpdbBuildBase.__init__(self)
        self.mode = 'on' if mode == 'orca' else 'off'

    def configure(self):
        config_options = ["--enable-mapreduce", "--with-gssapi", "--with-perl", "--with-libxml", "--with-python",
                          "--prefix=%s" % INSTALL_DIR]
        if self.mode == 'off':
            config_options.append("--disable-orca")
        cmd_with_options = ["./configure"]
        cmd_with_options.extend(config_options)
        return subprocess.call(cmd_with_options, cwd="gpdb_src")

    @staticmethod
    def create_demo_cluster():
        return subprocess.call([
            "runuser gpadmin -c \"source %s/greenplum_path.sh \
            && make create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\"" % INSTALL_DIR],
            cwd="gpdb_src/gpAux/gpdemo", shell=True)

    def unit_test(self):
        status = self.create_demo_cluster()
        if status:
            return status
        status = self.run_install_check_with_command('make -C gpMgmt/bin check')
        subprocess.check_call(["cat", "gpdb_src/gpMgmt/gpMgmt_testunit_results.log"])
        subprocess.check_call(["runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && pwd && source gpdb_src/gpAux/gpdemo/gpdemo-env.sh && gpstop -a\""], shell=True)
        return status

    def install_check(self, option='good'):
        status = self.create_demo_cluster()
        if status:
            return status
        if option == 'world':
            return self.run_install_check_with_command('make installcheck-world')
        else:
            return self.run_install_check_with_command('make -C src/test installcheck-good')

    def run_install_check_with_command(self, make_command):
        return subprocess.call([
            "runuser gpadmin -c \"source {0}/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={1}' \
            {2} \"".format(INSTALL_DIR, self.mode, make_command)], cwd="gpdb_src", shell=True)
