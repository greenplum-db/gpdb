#!/bin/bash -l

set -exo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
INSTALL_DIR="/usr/local/gpdb"

PXF_HOME="${INSTALL_DIR}/pxf"

function run_regression_test() {
    cat > /home/gpadmin/run_regression_test.sh <<-EOF
#!/bin/bash
    set -exo pipefail
    if [ "$TARGET_OS" != "ubuntu" ]; then
      source /opt/gcc_env.sh
    fi
    source ${INSTALL_DIR}/greenplum_path.sh
    cd "\${1}/gpdb_src/gpAux"
    source gpdemo/gpdemo-env.sh
    cd "\${1}/gpdb_src/gpAux/extensions/pxf"
    make installcheck USE_PGXS=1
    [ -s regression.diffs ] && cat regression.diffs && exit 1
    exit 0
EOF

    chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
    chmod a+x /home/gpadmin/run_regression_test.sh

    su gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function run_pxf_automation() {
    cat > /home/gpadmin/run_pxf_automation_test.sh <<-EOF
#!/bin/bash
    set -exo pipefail
    source ${INSTALL_DIR}/greenplum_path.sh
    source \${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    # set variables needed by PXF Automation and Parot to run in GPDB mode with SingleCluster and standalone PXF
    export PG_MODE=GPDB
    export GPHD_ROOT=$1
    export PXF_HOME=${PXF_HOME}
    psql -d template1 -c "CREATE EXTENSION PXF"
    cd \${1}/pxf_automation_src
    make GROUP=gpdb
    exit 0
EOF

    chown gpadmin:gpadmin /home/gpadmin/run_pxf_automation_test.sh
    chmod a+x /home/gpadmin/run_pxf_automation_test.sh
    su gpadmin -c "bash /home/gpadmin/run_pxf_automation_test.sh $(pwd)"
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash
}

function unpack_tarball() {
    local tarball=$1
    echo "Unpacking tarball: $(ls ${tarball})"
    tar xfp ${tarball} --strip-components=1
}

function setup_singlecluster() {
    pushd singlecluster
        if [ -f ./*.tar.gz ]; then
            unpack_tarball ./*.tar.gz
        fi
    popd

    pushd singlecluster/bin
        export SLAVES=1
        ./init-gphd.sh
        # zookeeper required for HBase
        ./start-zookeeper.sh
        ./start-hdfs.sh
        ./start-yarn.sh
        ./start-hive.sh
        ./start-hbase.sh
    popd
}

function install_hadoop_client_rpms() {
    local hdfsrepo=$1
    local yum_repofile_url=$2

    pushd /etc/yum.repos.d > /dev/null
        # download yum repo definition file for the vendor stack
        wget ${yum_repofile_url}
        # install required RPMs
        yum install -y hadoop-client
        yum install -y hive
        yum install -y hbase
        # copy cluster configuration files from single cluster
        mkdir -p /etc/hadoop/conf
        cp ${hdfsrepo}/hadoop/etc/hadoop/core-site.xml /etc/hadoop/conf/
        cp ${hdfsrepo}/hadoop/etc/hadoop/hdfs-site.xml /etc/hadoop/conf/
        # mapred below is needed for test for mapreduce.input.fileinputformat.input.dir.recursive to be set to true
        cp ${hdfsrepo}/hadoop/etc/hadoop/mapred-site.xml /etc/hadoop/conf/
        mkdir -p /etc/hive/conf
        cp ${hdfsrepo}/hive/conf/hive-site.xml /etc/hive/conf
        mkdir -p /etc/hbase/conf
        cp ${hdfsrepo}/hbase/conf/hbase-site.xml /etc/hbase/conf
    popd
}

function setup_hadoop_client() {
    local hdfsrepo=$1

    case ${HADOOP_CLIENT} in
         CDH)
             install_hadoop_client_rpms ${hdfsrepo} "https://archive.cloudera.com/cdh5/redhat/6/x86_64/cdh/cloudera-cdh5.repo"
             ;;
         HDP)
             install_hadoop_client_rpms ${hdfsrepo} "http://public-repo-1.hortonworks.com/HDP/centos6/2.x/updates/2.6.2.0/hdp.repo"
             ;;
         TAR)
             # TAR-based setup, edit the properties in pxf-env.sh to specify HADOOP_ROOT value
             sed -i -e "s|^[[:blank:]]*export HADOOP_ROOT=.*$|export HADOOP_ROOT=${hdfsrepo}|g" ${PXF_HOME}/conf/pxf-env.sh
             ;;
         *)
             echo "FATAL: Unknown HADOOP_CLIENT=${HADOOP_CLIENT} parameter value"
             exit 1
             ;;
    esac

    echo "Contents of ${PXF_HOME}/conf/pxf-env.sh :"
    cat ${PXF_HOME}/conf/pxf-env.sh
}

function start_pxf() {
    local hdfsrepo=$1
    pushd ${PXF_HOME} > /dev/null
        #Check if some other process is listening on 51200
        netstat -tlpna | grep 51200 || true
        su gpadmin -c "bash ./bin/pxf init"
        su gpadmin -c "bash ./bin/pxf start"
    popd > /dev/null
}

function install_psi_python_tool() {
    pip install --upgrade pip  # this was required on centos7
    pip install psi -t ${INSTALL_DIR}/lib/python
}

function configure_with_planner() {
    PYTHONPATH=${SCRIPT_DIR}:${PYTHONPATH} python2 -c "from builds.GpBuild import GpBuild; \
                                                       GpBuild(\"planner\").configure()"
}

function copy_gpdb_bits_to_gphome() {
    mkdir -p ${INSTALL_DIR}
    tar -xzf bin_gpdb/*.tar.gz -C ${INSTALL_DIR}
    sed -i -e "s|GPHOME=.*|GPHOME=/usr/local/gpdb|g" ${INSTALL_DIR}/greenplum_path.sh
}

function install_java() {
    # centos java has been installed by dockerfile; install via apt-get for ubuntu
    if [ "$TARGET_OS" == "ubuntu" ]; then
        apt-get update
        apt-get install -y openjdk-8-jdk
        export JAVA_HOME=$(readlink -f /usr/bin/java | sed "s:/jre/bin/java::")
        echo "export JAVA_HOME=${JAVA_HOME}" >> ~/.bash_profile
        echo "export JAVA_HOME=${JAVA_HOME}" >> /home/gpadmin/.bash_profile
        chown gpadmin:gpadmin /home/gpadmin/.bash_profile
    fi
}

function make_cluster() {
    PYTHONPATH=${SCRIPT_DIR}:${PYTHONPATH} python2 -c "from builds.GpBuild import GpBuild; \
                                                       x = GpBuild(\"planner\").create_demo_cluster(); \
                                                       exit(x)"
}

function _main() {
    if [ -z "$TARGET_OS" ]; then
        echo "FATAL: TARGET_OS is not set"
        exit 1
    fi

    time configure_with_planner
    time copy_gpdb_bits_to_gphome
    time install_psi_python_tool
    time setup_gpadmin_user
    time install_java

    # Reserve port 51200 for PXF service
    echo "pxf             51200/tcp               # PXF Service" >> /etc/services

    # setup hadoop before making GPDB cluster to use system python for yum install
    time setup_singlecluster
    time setup_hadoop_client $(pwd)/singlecluster

    time make_cluster
    time start_pxf $(pwd)/singlecluster
    chown -R gpadmin:gpadmin singlecluster pxf_automation_src
    time run_regression_test
    if [ "$TARGET_OS" != "ubuntu" ]; then
      # eventually include this after Ubuntu is supported better
      time run_pxf_automation $(pwd)/singlecluster
    fi
}

_main "$@"
