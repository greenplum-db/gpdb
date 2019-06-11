import subprocess
import os
from GpdbBuildBase import GpdbBuildBase

INSTALL_DIR="/usr/local/gpdb"

def fail_on_error(status):
    import sys
    if status:
        sys.exit(status)

class GpBuild(GpdbBuildBase):
    def __init__(self, mode="orca"):
        GpdbBuildBase.__init__(self)
        self.mode = 'on' if mode == 'orca' else 'off'
        self.configure_options =  [
                                    "--enable-mapreduce",
                                    "--enable-orafce",
                                    "--with-gssapi",
                                    "--with-perl",
                                    "--with-libxml",
                                    "--with-python",
                                    # TODO: Remove this line as soon as zstd is built into Ubuntu docker image
                                    "--without-zstd",
                                    "--prefix={0}".format(INSTALL_DIR)
                                  ]
        self.source_gcc_env_cmd = ''

    def configure(self):
        if self.mode == 'off':
            self.configure_options.append("--disable-orca")
        cmd_with_options = ["./configure"]
        cmd_with_options.extend(self.configure_options)
        cmd = " ".join(cmd_with_options)
        return self.run_cmd(cmd, "gpdb_src")

    def create_demo_cluster(self):
        return subprocess.call([
            "runuser gpadmin -c \"source {0}/greenplum_path.sh \
            && {1} make create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\"".format(INSTALL_DIR, self.source_gcc_env_cmd)],
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
            return self.run_install_check_with_command('make -C src/test/regress installcheck-good')

    def run_install_check_with_command(self, make_command):
        return subprocess.call([
            "runuser gpadmin -c \"source {0}/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={1}' \
            {2} \"".format(INSTALL_DIR, self.mode, make_command)], cwd="gpdb_src", shell=True)

    def _run_gpdb_command(self, command, stdout=None, stderr=None, source_env_cmd=''):
        cmd = "source {0}/greenplum_path.sh && source gpdb_src/gpAux/gpdemo/gpdemo-env.sh".format(INSTALL_DIR)
        if len(source_env_cmd) != 0:
            #over ride the command if requested
            cmd = source_env_cmd
        runcmd = "runuser gpadmin -c \"{0} && {1} \"".format(cmd, command)
        print "Executing {}".format(runcmd)
        return subprocess.call([runcmd], shell=True, stdout=stdout, stderr=stderr)

    def run_explain_test_suite(self, dbexists):
        source_env_cmd = ''
        if dbexists:
            source_env_cmd='source {0}/greenplum_path.sh && source ~/.bash_profile '.format(INSTALL_DIR)
        else:
            status = self.create_demo_cluster()
            fail_on_error(status)
            status = self._run_gpdb_command("createdb")
            fail_on_error(status)
            status = self._run_gpdb_command("psql -f sql/schema.sql")
            fail_on_error(status)

            with open("load_stats.txt", "w") as f:
                status = self._run_gpdb_command("psql -f sql/stats.sql", stdout=f)
            if status:
                with open("load_stats.txt", "r") as f:
                    print f.read()
                fail_on_error(status)

        # Now run the queries !!
        os.mkdir('out')
        status = 0
        sql_files = os.listdir("sql")
        sql_files.sort()
        for fsql in sql_files:
            if fsql.endswith('.sql') and fsql not in ['stats.sql', 'schema.sql']:
                output_fname = 'out/{}'.format(fsql.replace('.sql', '.out'))
                # GUC name should match with name in gpdb 5X_STABLE branch
                with open(output_fname, 'w') as fout:
                    current_status = self._run_gpdb_command("env PGOPTIONS='-c optimizer_enable_full_join=on' psql -a -f sql/{}".format(fsql), stdout=fout, stderr=fout, source_env_cmd=source_env_cmd)
                    print "status: {0}".format(current_status)
                    status = status if status != 0 else current_status

        return status



    def append_configure_options(self, configure_options):
        if configure_options:
            self.configure_options.extend(configure_options)

    def set_gcc_env_file(self, gcc_env_file):
        if gcc_env_file is not None:
            self.source_gcc_env_cmd = "source {0} &&".format(gcc_env_file)

    def make(self):
        num_cpus = self.num_cpus()
        cmd = ["make", "-j" + str(num_cpus), "-l" + str(2 * num_cpus)]
        cmd = " ".join(cmd)
        return self.run_cmd(cmd, "gpdb_src")

    def run_cmd(self, cmd, working_dir):
        cmd =  self.source_gcc_env_cmd + cmd
        return  subprocess.call(cmd, shell=True, cwd=working_dir)

    def make_install(self):
        cmd = "make install"
        return self.run_cmd(cmd, "gpdb_src")

    def unittest(self):
        cmd = "make -s unittest-check"
        return self.run_cmd(cmd, "gpdb_src/src/backend")
