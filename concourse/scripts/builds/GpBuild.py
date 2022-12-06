import subprocess
import os

INSTALL_DIR="/usr/local/greenplum-db-devel"

def fail_on_error(status):
    import sys
    if status:
        sys.exit(status)

class GpBuild:
    def __init__(self, mode="orca"):
        self.mode = 'on' if mode == 'orca' else 'off'
        self.configure_options =  [
                                    "--enable-mapreduce",
                                    "--enable-orafce",
                                    "--with-gssapi",
                                    "--with-perl",
                                    "--with-libxml",
                                    "--with-python",
                                    "--with-uuid=e2fs",
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
        return self._run_cmd(cmd, "gpdb_src")

    def create_demo_cluster(self, install_dir=INSTALL_DIR, no_mirrors=False):
        NO_MIRROR_STRING=""
        if no_mirrors:
            NO_MIRROR_STRING="NUM_PRIMARY_MIRROR_PAIRS=1 WITH_MIRRORS=false"
        return subprocess.call([
            "runuser gpadmin -c \"source {0}/greenplum_path.sh \
            && {1} make {2} create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\"".format(install_dir, self.source_gcc_env_cmd, NO_MIRROR_STRING)],
            cwd="gpdb_src/gpAux/gpdemo", shell=True)

    def _run_gpdb_command(self, command, stdout=None, stderr=None, source_env_cmd='', print_command=True):
        if len(source_env_cmd) != 0:
            #over ride the command if requested
            cmd = source_env_cmd
        runcmd = "runuser gpadmin -c \"{0} && {1} \"".format(cmd, command)
        if print_command:
            print "Executing {}".format(runcmd)
        return subprocess.call([runcmd], shell=True, stdout=stdout, stderr=stderr)

    def run_explain_test_suite(self, dbexists):
        cmd = 'echo \\\\timing on>> /home/gpadmin/.psqlrc'
        self._run_cmd(cmd, "gpdb_src")

        source_env_cmd = ''
        if dbexists:
            source_env_cmd='source {0}/greenplum_path.sh && source ~/.bash_profile '.format(INSTALL_DIR)
        else:
            source_env_cmd = "source {0}/greenplum_path.sh && source gpdb_src/gpAux/gpdemo/gpdemo-env.sh".format(INSTALL_DIR)
            status = self.create_demo_cluster(INSTALL_DIR, True)
            fail_on_error(status)
            status = self._run_gpdb_command("createdb", source_env_cmd=source_env_cmd)
            fail_on_error(status)
            status = self._run_gpdb_command("psql -f schema.sql", source_env_cmd=source_env_cmd)
            fail_on_error(status)

            with open("load_stats.txt", "w") as f:
                status = self._run_gpdb_command("psql -q -f stats.sql", stdout=f, source_env_cmd=source_env_cmd)
            if status:
                with open("load_stats.txt", "r") as f:
                    print f.read()
                fail_on_error(status)

        # set gucs if any were specified
        status = self._run_cmd("{0} && cat gporca-commits-to-test/optional_gucs.txt >> $MASTER_DATA_DIRECTORY/postgresql.conf".format(source_env_cmd), None)
        fail_on_error(status)
        # use 32 segments for explain tests (this number is fairly arbitrary, it might be better to make this dependent on the workload?)
        status = self._run_cmd("{0} && echo 'optimizer_segments=32\ngp_segments_for_planner=32' >> $MASTER_DATA_DIRECTORY/postgresql.conf".format(source_env_cmd), None)
        fail_on_error(status)
        status = self._run_gpdb_command("gpstop -ar", source_env_cmd=source_env_cmd)
        fail_on_error(status)

        # Now run the queries !!
        os.mkdir('out')
        status = 0
        sql_files = os.listdir("sql")
        sql_files.sort()
        for fsql in sql_files:
            if fsql.endswith('.sql') and fsql not in ['stats.sql', 'schema.sql']:
                output_fname = 'out/{}'.format(fsql.replace('.sql', '.out'))
                with open(output_fname, 'w') as fout:
                    print "Running query: " + fsql
                    current_status = self._run_gpdb_command("psql -a -f sql/{}".format(fsql), stdout=fout, stderr=fout, source_env_cmd=source_env_cmd, print_command=False)
                    if current_status != 0:
                        print "ERROR: {0}".format(current_status)
                    status = status if status != 0 else current_status

        return status

    def _run_cmd(self, cmd, working_dir):
        cmd =  self.source_gcc_env_cmd + cmd
        return  subprocess.call(cmd, shell=True, cwd=working_dir)

    def install_dependency(self, dependency_name, untar_dir="/usr/local"):
        subprocess.call("mkdir -p {0}".format(untar_dir), shell=True)
        subprocess.call("tar -xzf {0}/*.tar.gz -C {1}".format(dependency_name, untar_dir), shell=True)
        return subprocess.call(["ldconfig", os.path.join(untar_dir, "lib")])
