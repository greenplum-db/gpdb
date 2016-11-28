#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function gen_env(){
	cat > ~/run_unit_tests.sh <<-EOF
	set -exo pipefail

	cd "\${1}/gpdb_src/gpAux/extensions/gpcloud"
	make test
	EOF

	chmod a+x ~/run_unit_tests.sh
}

function run_unit_tests() {
	bash -e ~/run_unit_tests.sh $(pwd)
}

function _main() {
	time prep_env_for_centos
	time gen_env

	time run_unit_tests
}

function prep_env_for_centos() {
  case "$TARGET_OS_VERSION" in
    5)
      BLDARCH=rhel5_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
      source /opt/gcc_env.sh
      ;;

    6)
      BLDARCH=rhel6_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
      source /opt/gcc_env.sh
      # This is necessesary to build gphdfs.
      # It should be removed once the build image has this included.
      yum install -y ant-junit
      ;;

    7)
      BLDARCH=rhel7_x86_64
      alternatives --set java /usr/lib/jvm/java-1.7.0-openjdk-1.7.0.111-2.6.7.2.el7_2.x86_64/jre/bin/java
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk-1.7.0.111-2.6.7.2.el7_2.x86_64
      ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
      source /opt/gcc_env.sh
      yum install -y ant-junit
      ;;

    *)
    echo "TARGET_OS_VERSION not set or recognized for Centos/RHEL"
    exit 1
    ;;
  esac

  ln -sf /$(pwd)/gpdb_src/gpAux/ext/${BLDARCH}/python-2.6.2 /opt/python-2.6.2
  export PATH=${JAVA_HOME}/bin:${PATH}
}

_main "$@"
